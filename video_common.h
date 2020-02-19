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
#ifndef __VIDEO_COMMON_H__
#define __VIDEO_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*shadow_paint_box_callback)(int left, int top, int right, int bottom);
void register_shadow_paint_box(shadow_paint_box_callback cb);
extern shadow_paint_box_callback shadow_paint_box_cb;

typedef void (*shadow_paint_name_callback)(char *name, bool real);
void register_shadow_paint_name(shadow_paint_name_callback cb);
extern shadow_paint_name_callback shadow_paint_name_cb;

typedef void (*shadow_display_callback)(void *src_ptr, int src_fd, int src_fmt, int src_w, int src_h);
void register_shadow_display(shadow_display_callback cb);
extern shadow_display_callback shadow_display_cb;

typedef void (*shadow_display_vertical_callback)(void *src_ptr, int src_fd, int src_fmt, int src_w, int src_h);
void register_shadow_display_vertical(shadow_display_vertical_callback cb);
extern shadow_display_vertical_callback shadow_display_vertical_cb;

int get_video_id(char *name);

#ifdef __cplusplus
}
#endif
#endif
