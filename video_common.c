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
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "video_common.h"

#define MAX_VIDEO_ID 20

shadow_paint_box_callback shadow_paint_box_cb = NULL;
void register_shadow_paint_box(shadow_paint_box_callback cb)
{
    shadow_paint_box_cb = cb;
}

shadow_paint_name_callback shadow_paint_name_cb = NULL;
void register_shadow_paint_name(shadow_paint_name_callback cb)
{
    shadow_paint_name_cb = cb;
}

shadow_display_callback shadow_display_cb = NULL;
void register_shadow_display(shadow_display_callback cb)
{
    shadow_display_cb = cb;
}

shadow_display_vertical_callback shadow_display_vertical_cb = NULL;
void register_shadow_display_vertical(shadow_display_vertical_callback cb)
{
    shadow_display_vertical_cb = cb;
}

int get_video_id(char *name)
{
    FILE *fp = NULL;
    char buf[1024];
    int i;
    char cmd[128];
    bool exist = false;

    for (i = 0; i < MAX_VIDEO_ID; i++) {
        snprintf(cmd, sizeof(cmd), "/sys/class/video4linux/video%d/name", i);
        if (access(cmd, F_OK))
            continue;
        snprintf(cmd, sizeof(cmd), "cat /sys/class/video4linux/video%d/name", i);
        fp = popen(cmd, "r");
        if (fp) {
            if (fgets(buf, sizeof(buf), fp)) {
                if (strstr(buf, name))
                    exist = true;
            }
            pclose(fp);
        }
        if (exist)
            break;
    }
    return (i == MAX_VIDEO_ID ? -1 : i);
}
