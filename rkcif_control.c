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
#include <pthread.h>
#include <stdbool.h>
#include "rockface_control.h"
#include "shadow_display.h"
#include "video_common.h"

#include <camera_engine_rkisp/interface/rkisp_api.h>
#include "rga_control.h"
#include <linux/media-bus-format.h>

static bo_t g_rotate_bo;
static int g_rotate_fd = -1;

static const struct rkisp_api_ctx *ctx;
static const struct rkisp_api_buf *buf;
static bool g_run;
static pthread_t g_tid;

static void *process(void *arg)
{
    rga_info_t src, dst;

    do {
        buf = rkisp_get_frame(ctx, 0);
        memset((char *)buf->buf + ctx->height * ctx->width, 128, ctx->height * ctx->width / 2);

        memset(&src, 0, sizeof(rga_info_t));
        src.fd = buf->fd;
        src.mmuFlag = 1;
        src.rotation = HAL_TRANSFORM_ROT_270;
        rga_set_rect(&src.rect, 0, 0, ctx->width, ctx->height, ctx->width, ctx->height,
                     RK_FORMAT_YCbCr_420_SP);
        memset(&dst, 0, sizeof(rga_info_t));
        dst.fd = g_rotate_fd;
        dst.mmuFlag = 1;
        rga_set_rect(&dst.rect, 0, 0, ctx->height, ctx->width, ctx->height, ctx->width,
                     RK_FORMAT_YCbCr_420_SP);
        if (c_RkRgaBlit(&src, &dst, NULL)) {
            printf("%s: rga fail\n", __func__);
            continue;
        }

        rockface_control_convert_ir(g_rotate_bo.ptr, ctx->height, ctx->width,
                                    RK_FORMAT_YCbCr_420_SP);
#if 0
        //shadow_display(buf->buf, buf->fd, RK_FORMAT_YCbCr_420_SP, ctx->width, ctx->height);
        shadow_display_vertical(g_rotate_bo.ptr, g_rotate_fd, RK_FORMAT_YCbCr_420_SP,
                                ctx->height, ctx->width);
        if (ui_paint_refresh_cb)
            ui_paint_refresh_cb();
#endif

        rkisp_put_frame(ctx, buf);
    } while (g_run);

    pthread_exit(NULL);
}

int rkcif_control_init(void)
{
    char name[32];

    int id = get_video_id("stream_cif_dvp");
    if (id < 0) {
        printf("%s: get video id fail!\n", __func__);
        return -1;
    }

    snprintf(name, sizeof(name), "/dev/video%d", id);
    printf("%s: %s\n", __func__, name);
    ctx = rkisp_open_device(name, 0);
    if (ctx == NULL) {
        printf("%s: ctx is NULL\n", __func__);
        return -1;
    }

    rkisp_set_sensor_fmt(ctx, 1280, 720, MEDIA_BUS_FMT_YUYV8_2X8);
    rkisp_set_fmt(ctx, 1280, 720, ctx->fcc);

    if (rga_control_buffer_init(&g_rotate_bo, &g_rotate_fd, ctx->width, ctx->height, 12))
        return -1;

    if (rkisp_start_capture(ctx))
        return -1;

    g_run = true;
    if (pthread_create(&g_tid, NULL, process, NULL)) {
        printf("pthread_create fail\n");
        return -1;
    }

    return 0;
}

void rkcif_control_exit(void)
{
    g_run = false;
    if (g_tid) {
        pthread_join(g_tid, NULL);
        g_tid = 0;
    }

    rkisp_stop_capture(ctx);
    rkisp_close_device(ctx);

    rga_control_buffer_deinit(&g_rotate_bo, g_rotate_fd);
}
