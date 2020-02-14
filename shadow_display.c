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
#include <minigui/shadow_rga.h>
#include <rga/RgaApi.h>
#include "shadow_display.h"
#include "ui.h"

static int g_crop_video_x, g_crop_video_y, g_crop_video_w, g_crop_video_h;
static int g_crop_screen_w, g_crop_screen_h;

void shadow_display(void *src_ptr, int src_fd, int src_fmt, int src_w, int src_h)
{
    int dst_fd, dst_w, dst_h;
    int screen_w, screen_h;
    int src_act_w, src_act_h;
    rga_info_t src;
    rga_info_t dst;

    shadow_rga_get_user_fd(&dst_fd, &screen_w, &screen_h);
    if (dst_fd <= 0 || screen_w <= 0 || screen_h <= 0) {
        printf("%s: get data fail\n", __func__);
        return;
    }

    if (screen_w < screen_h && screen_h >= src_h) {
        int scale = screen_h / src_h;
        dst_w = screen_w;
        dst_h = scale * src_h;
        src_act_w = src_h * dst_w / dst_h;
        src_act_h = src_h;
        g_crop_video_x = (src_w - src_act_w) / 2;
        g_crop_video_y = (src_h - src_act_h) / 2;
        g_crop_video_w = src_act_w;
        g_crop_video_h = src_act_h;
        g_crop_screen_w = dst_w;
        g_crop_screen_h = dst_h;

        memset(&src, 0, sizeof(rga_info_t));
        if (src_ptr) {
            src.fd = -1;
            src.virAddr = src_ptr;
        } else {
            src.fd = src_fd;
        }
        src.mmuFlag = 1;
        rga_set_rect(&src.rect, (src_w - src_act_w) / 2, 0, src_act_w, src_act_h,
                     src_w, src_h, src_fmt);

        memset(&dst, 0, sizeof(rga_info_t));
        dst.fd = dst_fd;
        dst.mmuFlag = 1;
        rga_set_rect(&dst.rect, 0, 0, dst_w, dst_h, dst_w, dst_h, src_fmt);

        if (c_RkRgaBlit(&src, &dst, NULL)) {
            printf("%s: rga fail\n", __func__);
            return;
        }

        shadow_rga_switch(NULL, dst.fd, src_fmt, dst_w, dst_h);
    } else {
        printf("%s: unsupport screen!\n", __func__);
    }
}

void shadow_display_vertical(void *src_ptr, int src_fd, int src_fmt, int src_w, int src_h)
{
    int dst_fd, dst_w, dst_h;
    int screen_w, screen_h;
    rga_info_t src, dst;

    shadow_rga_get_user_fd(&dst_fd, &screen_w, &screen_h);
    if (dst_fd <= 0 || screen_w <= 0 || screen_h <= 0) {
        printf("%s: get data fail\n", __func__);
        return;
    }

    if (screen_w < screen_h) {
        if (src_w == screen_w && src_h == screen_h) {
            g_crop_video_x = 0;
            g_crop_video_y = 0;
            g_crop_video_w = src_w;
            g_crop_video_h = src_h;
            g_crop_screen_w = src_w;
            g_crop_screen_h = src_h;
            shadow_rga_switch(NULL, src_fd, src_fmt, src_w, src_h);
        } else {
            dst_w = screen_w;
            dst_h = screen_w * src_h / src_w;
            g_crop_video_x = 0;
            g_crop_video_y = 0;
            g_crop_video_w = src_w;
            g_crop_video_h = src_h;
            g_crop_screen_w = dst_w;
            g_crop_screen_h = dst_h;

            memset(&src, 0, sizeof(rga_info_t));
            if (src_ptr) {
                src.fd = -1;
                src.virAddr = src_ptr;
            } else {
                src.fd = src_fd;
            }
            src.mmuFlag = 1;
            rga_set_rect(&src.rect, 0, 0, src_w, src_h, src_w, src_h, src_fmt);

            memset(&dst, 0, sizeof(rga_info_t));
            dst.fd = dst_fd;
            dst.mmuFlag = 1;
            rga_set_rect(&dst.rect, 0, 0, dst_w, dst_h, dst_w, dst_h, src_fmt);

            if (c_RkRgaBlit(&src, &dst, NULL)) {
                printf("%s: rga fail\n", __func__);
                return;
            }

            shadow_rga_switch(NULL, dst.fd, src_fmt, dst_w, dst_h);
        }
    } else {
        printf("%s: unsupport screen!\n", __func__);
    }
}

void shadow_paint_box(int left, int top, int right, int bottom)
{
    ui_paint_box(g_crop_video_w, g_crop_video_h,
            left - g_crop_video_x, top - g_crop_video_y,
            right - g_crop_video_x, bottom - g_crop_video_y);
}

void shadow_paint_name(char *name, bool real)
{
    ui_paint_name(name, real);
}

void shadow_get_crop_screen(int *width, int *height)
{
    *width = g_crop_screen_w;
    *height = g_crop_screen_h;
}
