/*
 * Copyright (C) 2019 Rockchip Electronics Co., Ltd.
 * author: Zhihua Wang, hogan.wang@rock-chips.com
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL), available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#include "face_common.h"
#include "database.h"
#include "rockface_control.h"
#include "play_wav.h"
#include "load_feature.h"
#include "video_common.h"
#include "rkisp_control.h"
#include "rkcif_control.h"

#define DEFAULT_FACE_NUMBER 1000
#define DEFAULT_FACE_PATH "/userdata"
#define FACE_SCORE 0.9
#define FACE_SCORE_REGISTER 0.9999
#define FACE_REGISTER_CNT 5
#define FACE_REAL_SCORE 0.9
#define LICENCE_PATH "/userdata/key.lic"
#define FACE_DATA_PATH "/usr/lib"
#define MIN_FACE_WIDTH(w) ((w) / 5)
#define CONVERT_RGB_WIDTH 640
#define CONVERT_IR_WIDTH 640
#define FACE_TRACK_FRAME 0
#define FACE_RETRACK_TIME 1

static void *g_face_data = NULL;
static int g_face_index = 0;
static int g_face_cnt = DEFAULT_FACE_NUMBER;

static rockface_handle_t face_handle;
static int g_total_cnt;

static pthread_t g_tid;
static bool g_run;
static char last_name[NAME_LEN];
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cond = PTHREAD_COND_INITIALIZER;
static bool g_flag;
static rockface_image_t g_rgb_img;
static rockface_det_t g_rgb_face;
static bo_t g_rgb_bo;
static int g_rgb_fd = -1;
static int g_rgb_track = -1;
static pthread_mutex_t g_rgb_track_mutex = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t g_ir_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_ir_cond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t g_ir_img_mutex = PTHREAD_MUTEX_INITIALIZER;
static rockface_image_t g_ir_img;
static bo_t g_ir_bo;
static int g_ir_fd = -1;

static bool g_register = false;
static int g_register_cnt = 0;
static bool g_delete = false;

static rockface_det_t *get_max_face(rockface_det_array_t *face_array)
{
    rockface_det_t *max_face = NULL;
    if (face_array->count == 0)
        return NULL;

    for (int i = 0; i < face_array->count; i++) {
        rockface_det_t *cur_face = &(face_array->face[i]);
        if (max_face == NULL) {
            max_face = cur_face;
            continue;
        }
        int cur_face_box_area = (cur_face->box.right - cur_face->box.left) *
                                (cur_face->box.bottom - cur_face->box.top);
        int max_face_box_area = (max_face->box.right - max_face->box.left) *
                                (max_face->box.bottom - max_face->box.top);
        if (cur_face_box_area > max_face_box_area)
            max_face = cur_face;
    }

    return max_face;
}

static int _rockface_control_detect(rockface_image_t *image, rockface_det_t *out_face, int *track)
{
    int r = 0;
    rockface_ret_t ret;
    rockface_det_array_t face_array0;
    rockface_det_array_t face_array;

    memset(&face_array0, 0, sizeof(rockface_det_array_t));
    memset(&face_array, 0, sizeof(rockface_det_array_t));
    memset(out_face, 0, sizeof(rockface_det_t));

    ret = rockface_detect(face_handle, image, &face_array0);
    if (ret != ROCKFACE_RET_SUCCESS)
        return -1;

    ret = rockface_track(face_handle, image, FACE_TRACK_FRAME, &face_array0, &face_array);
    if (ret != ROCKFACE_RET_SUCCESS)
        return -1;

    rockface_det_t* face = get_max_face(&face_array);
    if (face == NULL || face->score < FACE_SCORE ||
        face->box.right - face->box.left < MIN_FACE_WIDTH(image->width) ||
        face->box.left < 0 || face->box.top < 0 ||
        face->box.right > image->width || face->box.bottom > image->height)
        return -1;

    memcpy(out_face, face, sizeof(rockface_det_t));

    if (track) {
        pthread_mutex_lock(&g_rgb_track_mutex);
        if (g_delete || g_register)
            *track = -1;
        else if (*track == face->id)
            r = -2;
        else
            *track = face->id;
        pthread_mutex_unlock(&g_rgb_track_mutex);
    }

    return r;
}

static int rockface_control_detect(void *ptr, int width, int height, rockface_pixel_format fmt,
                                   rockface_image_t *image, rockface_det_t *face)
{
    int ret;
    static struct timeval t0;
    struct timeval t1;

    memset(face, 0, sizeof(rockface_det_t));
    memset(image, 0, sizeof(rockface_image_t));
    image->width = width;
    image->height = height;
    image->data = ptr;
    image->pixel_format = fmt;

    gettimeofday(&t1, NULL);
    pthread_mutex_lock(&g_rgb_track_mutex);
    if (g_rgb_track >= 0 && (t1.tv_sec - t0.tv_sec) > FACE_RETRACK_TIME) {
        g_rgb_track = -1;
        gettimeofday(&t0, NULL);
    }
    pthread_mutex_unlock(&g_rgb_track_mutex);
    ret = _rockface_control_detect(image, face, &g_rgb_track);
    if (face->score > FACE_SCORE) {
        int left, top, right, bottom;
        left = face->box.left;
        top = face->box.top;
        right = face->box.right;
        bottom = face->box.bottom;
        if (shadow_paint_box_cb)
            shadow_paint_box_cb(left, top, right, bottom);
        rkisp_control_expo_weights_90(left, top, right, bottom);
    } else {
        if (shadow_paint_box_cb)
            shadow_paint_box_cb(0, 0, 0, 0);
        rkisp_control_expo_weights_default();
    }

    return ret;
}

static int rockface_control_init_library(void *data, int num, size_t size, size_t off)
{
    rockface_ret_t ret;

    ret = rockface_face_library_init(face_handle, data, num, size, off);
    if (ret != ROCKFACE_RET_SUCCESS) {
        printf("%s: int library error %d!\n", __func__, ret);
        return -1;
    }

    return 0;
}

static void rockface_control_release_library(void)
{
    rockface_face_library_release(face_handle);
}

static int rockface_control_get_feature(rockface_image_t *in_image,
                                        rockface_feature_t *out_feature,
                                        rockface_det_t *in_face)
{
    rockface_ret_t ret;

    rockface_landmark_t landmark;
    ret = rockface_landmark5(face_handle, in_image, &(in_face->box), &landmark);
    if (ret != ROCKFACE_RET_SUCCESS || landmark.score < FACE_SCORE)
        return -1;

    rockface_image_t out_img;
    memset(&out_img, 0, sizeof(rockface_image_t));
    ret = rockface_align(face_handle, in_image, &(in_face->box), &landmark, &out_img);
    if (ret != ROCKFACE_RET_SUCCESS)
        return -1;

    ret = rockface_feature_extract(face_handle, &out_img, out_feature);
    rockface_image_release(&out_img);
    if (ret != ROCKFACE_RET_SUCCESS)
        return -1;

    return 0;
}

int rockface_control_get_path_feature(char *path, void *feature)
{
    int ret = -1;
    rockface_feature_t *out_feature = (rockface_feature_t*)feature;
    rockface_image_t in_img;
    rockface_det_t face;
    if (rockface_image_read(path, &in_img, 1))
        return -1;
    if (!_rockface_control_detect(&in_img, &face, NULL))
        ret = rockface_control_get_feature(&in_img, out_feature, &face);
    rockface_image_release(&in_img);
    return ret;
}

static void *rockface_control_search(rockface_image_t *image, void *data, int *index, int cnt,
                              size_t size, size_t offset, rockface_det_t *face, int reg)
{
    rockface_ret_t ret;
    rockface_search_result_t result;
    rockface_feature_t feature;

    if (rockface_control_get_feature(image, &feature, face) == 0) {
        //printf("g_total_cnt = %d\n", ++g_total_cnt);
        ret = rockface_feature_search(face_handle, &feature, 0.7, &result);
        if (ret == ROCKFACE_RET_SUCCESS) {
            if (g_register && ++g_register_cnt > FACE_REGISTER_CNT) {
                g_register = false;
                g_register_cnt = 0;
                play_wav_signal(REGISTER_ALREADY_WAV);
            }
            return result.feature;
        }
        if (g_register && *index < cnt && face->score > FACE_SCORE_REGISTER && reg) {
            char name[NAME_LEN];
            int id = database_get_user_name_id();
            if (id < 0) {
                printf("%s: get id fail!\n", __func__);
                return NULL;
            }
            snprintf(name, sizeof(name), "%s%d", USER_NAME, id);
            printf("add %s to %s\n", name, DATABASE_PATH);
            database_insert(&feature, sizeof(feature), name, sizeof(name), true);

            struct face_data *face_data = (struct face_data*)data + (*index);
            strncpy(face_data->name, name, sizeof(face_data->name) - 1);
            memcpy(&face_data->feature, &feature, sizeof(face_data->feature));
            *index += 1;
            rockface_control_release_library();
            rockface_control_init_library(data, *index, size, offset);
            g_register = false;
            g_register_cnt = 0;
            play_wav_signal(REGISTER_SUCCESS_WAV);
            return &face_data->feature;
        }
    }

    return NULL;
}

void rockface_control_set_delete(void)
{
    g_delete = false;
    if (g_register_cnt == 0)
        g_register = true;
}

void rockface_control_set_register(void)
{
    g_register = false;
    g_register_cnt = 0;
    g_delete = true;
}

static void rockface_control_wait(void)
{
    pthread_mutex_lock(&g_mutex);
    if (g_flag)
        pthread_cond_wait(&g_cond, &g_mutex);
    pthread_mutex_unlock(&g_mutex);
}

static void rockface_control_signal(void)
{
    pthread_mutex_lock(&g_mutex);
    g_flag = false;
    pthread_cond_signal(&g_cond);
    pthread_mutex_unlock(&g_mutex);
}

int rockface_control_convert(void *ptr, int width, int height, RgaSURF_FORMAT rga_fmt)
{
    int r;
    rockface_ret_t ret;
    rockface_image_t image;
    rockface_det_t face;
    rga_info_t src, dst;
    rockface_pixel_format fmt;

    if (!g_run)
        return -1;

    if (rga_fmt == RK_FORMAT_YCbCr_420_SP)
        fmt = ROCKFACE_PIXEL_FORMAT_YUV420SP_NV12;
    else {
        printf("%s: unsupport rga fmt\n");
        return -1;
    }
    r = rockface_control_detect(ptr, width, height, fmt, &image, &face);
    if (r) {
        if (r == -1)
            memset(last_name, 0, sizeof(last_name));
        return -1;
    }

    if (!g_flag)
        return -1;

    memset(&g_rgb_face, 0, sizeof(rockface_det_t));
    memset(&g_rgb_img, 0, sizeof(rockface_image_t));
    if (width > height) {
        g_rgb_img.width = CONVERT_RGB_WIDTH;
        g_rgb_img.height = CONVERT_RGB_WIDTH * height / width;
    } else {
        g_rgb_img.width = CONVERT_RGB_WIDTH * width / height;
        g_rgb_img.height = CONVERT_RGB_WIDTH;
    }
    g_rgb_img.pixel_format = ROCKFACE_PIXEL_FORMAT_RGB888;
    if (g_rgb_fd < 0) {
        if (rga_control_buffer_init(&g_rgb_bo, &g_rgb_fd, g_rgb_img.width, g_rgb_img.height, 24))
            return -1;
    }
    g_rgb_img.data = g_rgb_bo.ptr;
    memset(&src, 0, sizeof(rga_info_t));
    src.fd = -1;
    src.virAddr = ptr;
    src.mmuFlag = 1;
    rga_set_rect(&src.rect, 0, 0, width, height, width, height, rga_fmt);
    memset(&dst, 0, sizeof(rga_info_t));
    dst.fd = -1;
    dst.virAddr = g_rgb_bo.ptr;
    dst.mmuFlag = 1;
    rga_set_rect(&dst.rect, 0, 0, g_rgb_img.width, g_rgb_img.height,
                 g_rgb_img.width, g_rgb_img.height, RK_FORMAT_RGB_888);
    if (c_RkRgaBlit(&src, &dst, NULL)) {
        printf("%s: rga fail\n", __func__);
        return -1;
    }

    memcpy(&g_rgb_face, &face, sizeof(rockface_det_t));
    g_rgb_face.box.left = g_rgb_img.width * face.box.left / width;
    g_rgb_face.box.top = g_rgb_img.height * face.box.top / height;
    g_rgb_face.box.right = g_rgb_img.width * face.box.right / width;
    g_rgb_face.box.bottom = g_rgb_img.height * face.box.bottom / height;

    rockface_control_signal();

    return 0;
}

static bool rockface_control_liveness_ir(void)
{
    bool real = false;
    rockface_ret_t ret;
    rockface_image_t image;
    rockface_det_array_t face_array;
    rockface_liveness_t result;

    if (pthread_mutex_trylock(&g_ir_img_mutex))
        return real;

    ret = rockface_detect(face_handle, &g_ir_img, &face_array);
    if (ret != ROCKFACE_RET_SUCCESS)
        goto exit;

    rockface_det_t* face = get_max_face(&face_array);
    if (face == NULL || face->score < FACE_SCORE ||
        face->box.right - face->box.left < MIN_FACE_WIDTH(g_ir_img.width) ||
        face->box.left < 0 || face->box.top < 0 ||
        face->box.right > g_ir_img.width || face->box.bottom > g_ir_img.height)
        goto exit;

    ret = rockface_liveness_detect(face_handle, &g_ir_img, &face->box, &result);
    if (ret != ROCKFACE_RET_SUCCESS)
        goto exit;

    if (result.real_score < FACE_REAL_SCORE)
        goto exit;

    real = true;

exit:
    pthread_mutex_unlock(&g_ir_img_mutex);
    return real;
}

static bool rockface_control_wait_ir(void)
{
    bool ret = false;
    struct timeval now;
    struct timespec timeout;
    static int cnt = 0;

    pthread_mutex_lock(&g_ir_mutex);
    gettimeofday(&now, NULL);
    timeout.tv_sec = now.tv_sec + 1;
    timeout.tv_nsec = now.tv_usec * 1000;
    if (!pthread_cond_timedwait(&g_ir_cond, &g_ir_mutex, &timeout))
        ret = true;
    pthread_mutex_unlock(&g_ir_mutex);
    return ret;
}

static void rockface_control_signal_ir(void)
{
    pthread_mutex_lock(&g_ir_mutex);
    pthread_cond_signal(&g_ir_cond);
    pthread_mutex_unlock(&g_ir_mutex);
}

int rockface_control_convert_ir(void *ptr, int width, int height, RgaSURF_FORMAT rga_fmt)
{
    int ret = -1;
    rga_info_t src, dst;

    if (!g_run)
        return ret;

    if (pthread_mutex_trylock(&g_ir_img_mutex))
        return ret;

    memset(&g_ir_img, 0, sizeof(rockface_image_t));

    if (width > height) {
        g_ir_img.width = CONVERT_IR_WIDTH;
        g_ir_img.height = CONVERT_IR_WIDTH * height / width;
    } else {
        g_ir_img.width = CONVERT_IR_WIDTH * width / height;
        g_ir_img.height = CONVERT_IR_WIDTH;
    }
    g_ir_img.pixel_format = ROCKFACE_PIXEL_FORMAT_RGB888;
    if (g_ir_fd < 0) {
        if (rga_control_buffer_init(&g_ir_bo, &g_ir_fd, g_ir_img.width, g_ir_img.height, 24))
            goto exit;
    }
    g_ir_img.data = g_ir_bo.ptr;
    memset(&src, 0, sizeof(rga_info_t));
    src.fd = -1;
    src.virAddr = ptr;
    src.mmuFlag = 1;
    rga_set_rect(&src.rect, 0, 0, width, height, width, height, rga_fmt);
    memset(&dst, 0, sizeof(rga_info_t));
    dst.fd = -1;
    dst.virAddr = g_ir_bo.ptr;
    dst.mmuFlag = 1;
    rga_set_rect(&dst.rect, 0, 0, g_ir_img.width, g_ir_img.height,
                 g_ir_img.width, g_ir_img.height, RK_FORMAT_RGB_888);
    if (c_RkRgaBlit(&src, &dst, NULL)) {
        printf("%s: rga fail\n", __func__);
        goto exit;
    }

    ret = 0;

exit:
    pthread_mutex_unlock(&g_ir_img_mutex);
    if (ret == 0)
        rockface_control_signal_ir();
    return ret;
}

static void *rockface_control_thread(void *arg)
{
    int index;
    struct face_data *result;
    rockface_det_t face;
    char name[NAME_LEN];
    char *end;
    struct timeval t0, t1;
    int del_timeout = 0;
    int reg_timeout = 0;
    bool real = false;

    g_flag = true;
    while (g_run) {
        real = false;
        rockface_control_wait();
        if (!g_run)
            break;
        if (g_delete) {
            if (!del_timeout) {
                play_wav_signal(DELETE_START_WAV);
            }
            del_timeout++;
            if (del_timeout > 100) {
                del_timeout = 0;
                g_delete = false;
                play_wav_signal(DELETE_TIMEOUT_WAV);
            }
        } else {
            del_timeout = 0;
        }
        if (g_register && g_face_index < g_face_cnt) {
            if (!reg_timeout) {
                play_wav_signal(REGISTER_START_WAV);
            }
            reg_timeout++;
            if (reg_timeout > 100) {
                reg_timeout = 0;
                g_register = false;
                play_wav_signal(REGISTER_TIMEOUT_WAV);
            }
        } else if (g_register && g_face_index >= g_face_cnt) {
            g_register = false;
            g_register_cnt = 0;
            play_wav_signal(REGISTER_LIMIT_WAV);
        } else {
            reg_timeout = 0;
        }
        memcpy(&face, &g_rgb_face, sizeof(face));
        gettimeofday(&t0, NULL);
        result = (struct face_data*)rockface_control_search(&g_rgb_img, g_face_data, &g_face_index,
                        g_face_cnt, sizeof(struct face_data), 0, &face, reg_timeout);
        gettimeofday(&t1, NULL);
        if (g_delete && del_timeout && result) {
            printf("delete %s from %s\n", result->name, DATABASE_PATH);
            database_delete(result->name, true);
            memset(g_face_data, 0, g_face_cnt * sizeof(struct face_data));
            g_face_index = database_get_data(g_face_data, g_face_cnt,
                    sizeof(rockface_feature_t), 0, NAME_LEN, sizeof(rockface_feature_t));
            rockface_control_release_library();
            rockface_control_init_library(g_face_data, g_face_index,
                    sizeof(struct face_data), 0);
            del_timeout = 0;
            g_delete = false;
            play_wav_signal(DELETE_SUCCESS_WAV);
            if (shadow_paint_name_cb)
                shadow_paint_name_cb(NULL, false);
        } else if (result && face.score > FACE_SCORE) {
            end = strrchr(result->name, '.');
            if (end) {
                memset(name, 0, sizeof(name));
                memcpy(name, result->name, end - result->name);
            } else {
                memset(name, 0, sizeof(name));
                strncpy(name, result->name, sizeof(name) - 1);
            }
            //printf("name: %s\n", name);
            //printf("time: %ldus\n", (t1.tv_sec - t0.tv_sec) * 1000000 + t1.tv_usec - t0.tv_usec);
            if (rkcif_control_run()) {
                if (rockface_control_wait_ir())
                    if (rockface_control_liveness_ir())
                        real = true;
            }
            if (shadow_paint_name_cb)
                shadow_paint_name_cb(name, real);
            if (!g_register && real && memcmp(last_name, name, sizeof(last_name))) {
                printf("name: %s\n", name);
                memset(last_name, 0, sizeof(last_name));
                strncpy(last_name, name, sizeof(last_name) - 1);
                if (real) {
                    play_wav_signal(PLEASE_GO_THROUGH_WAV);
                }
            }
        } else {
            if (shadow_paint_name_cb)
                shadow_paint_name_cb(NULL, false);
        }
        if (!real) {
            memset(last_name, 0, sizeof(last_name));
            pthread_mutex_lock(&g_rgb_track_mutex);
            g_rgb_track = -1;
            pthread_mutex_unlock(&g_rgb_track_mutex);
        }
#if 0
        if (face.score > FACE_SCORE)
            printf("box = (%d %d %d %d) score = %f\n", face.box.left, face.box.top,
                    face.box.right, face.box.bottom, face.score);
#endif

        pthread_mutex_lock(&g_mutex);
        g_flag = true;
        pthread_mutex_unlock(&g_mutex);
    }

    pthread_exit(NULL);
}

int rockface_control_init(int face_cnt)
{
    rockface_ret_t ret;

    face_handle = rockface_create_handle();

    ret = rockface_set_licence(face_handle, LICENCE_PATH);
    if (ret != ROCKFACE_RET_SUCCESS) {
        printf("%s: authorization error %d!\n", __func__, ret);
        play_wav_signal(AUTHORIZE_FAIL_WAV);
        return -1;
    }
    ret = rockface_set_data_path(face_handle, FACE_DATA_PATH);
    if (ret != ROCKFACE_RET_SUCCESS) {
        printf("%s: set data path error %d!\n", __func__, ret);
        return -1;
    }

    ret = rockface_init_detector(face_handle);
    if (ret != ROCKFACE_RET_SUCCESS) {
        printf("%s: init detector error %d!\n", __func__, ret);
        return -1;
    }

    ret = rockface_init_recognizer(face_handle);
    if (ret != ROCKFACE_RET_SUCCESS) {
        printf("%s: init recognizer error %d!\n", __func__, ret);
        return -1;
    }

    ret = rockface_init_liveness_detector(face_handle);
    if (ret != ROCKFACE_RET_SUCCESS) {
        printf("%s: init liveness detector error %d!\n", __func__, ret);
        return -1;
    }

    if (face_cnt <= 0)
        g_face_cnt = DEFAULT_FACE_NUMBER;
    else
        g_face_cnt = face_cnt;
    g_face_data = calloc(g_face_cnt, sizeof(struct face_data));
    if (!g_face_data) {
        printf("face data alloc failed!\n");
        return -1;
    }

    if (access(DATABASE_PATH, F_OK) == 0) {
        printf("load face feature from %s\n", DATABASE_PATH);
        if (database_init())
            return -1;
        g_face_index += database_get_data(g_face_data, g_face_cnt, sizeof(rockface_feature_t), 0,
                                          NAME_LEN, sizeof(rockface_feature_t));
        database_exit();
    }

    if (database_init())
        return -1;
    printf("load face feature from %s\n", DEFAULT_FACE_PATH);
    g_face_index += load_feature(DEFAULT_FACE_PATH, ".jpg",
                        (struct face_data*)g_face_data + g_face_index, g_face_cnt - g_face_index);
    printf("face number is %d\n", g_face_index);
    sync();
    if (rockface_control_init_library(g_face_data, g_face_index, sizeof(struct face_data), 0))
        return -1;

    g_run = true;
    if (pthread_create(&g_tid, NULL, rockface_control_thread, NULL)) {
        printf("%s: pthread_create error!\n", __func__);
        g_run = false;
        return -1;
    }

    return 0;
}

void rockface_control_exit(void)
{
    g_run = false;
    rockface_control_signal();
    if (g_tid) {
        pthread_join(g_tid, NULL);
        g_tid = 0;
    }

    rockface_control_release_library();
    rockface_release_handle(face_handle);

    database_exit();

    if (g_face_data) {
        free(g_face_data);
        g_face_data = NULL;
    }

    rga_control_buffer_deinit(&g_rgb_bo, g_rgb_fd);
    rga_control_buffer_deinit(&g_ir_bo, g_ir_fd);
}
