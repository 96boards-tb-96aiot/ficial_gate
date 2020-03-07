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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <getopt.h>

#include "face_common.h"
#include "ui.h"
#include "rockface_control.h"
#include "load_feature.h"
#include "play_wav.h"
#include "rkisp_control.h"
#include "rkcif_control.h"
#include "shadow_display.h"
#include "video_common.h"

extern bool g_expo_weights_en;

void usage(const char *name)
{
    printf("Usage: %s options\n", name);
    printf("-h --help  Display this usage information.\n"
           "-f --face  Set face number.\n"
           "-e --expo  Set expo weights.\n"
           "-i --isp   Use isp camera.\n"
           "-c --cif   Use cif camera.\n");
    printf("e.g. %s -f 30000 -e -i -c\n", name);
    exit(0);
}

int MiniGUIMain(int argc, const char *argv[])
{
    int face_cnt = 0;
    int next_option;

    const char* const short_options = "hf:eic";
    const struct option long_options[] = {
        {"help", 0, NULL, 'h'},
        {"face", 1, NULL, 'f'},
        {"expo", 0, NULL, 'e'},
        {"isp", 0, NULL, 'i'},
        {"cif", 0, NULL, 'c'},
    };

    do {
        next_option = getopt_long(argc, argv, short_options, long_options, NULL);
        switch (next_option) {
        case 'f':
            face_cnt = atoi(optarg);
            break;
        case 'e':
            g_expo_weights_en = true;
            break;
        case 'i':
            g_isp_en = true;
            break;
        case 'c':
            g_cif_en = true;
            break;
        case -1:
            break;
        default:
            usage(argv[0]);
            break;
        }
    } while (next_option != -1);

    register_shadow_paint_box(shadow_paint_box);
    register_shadow_paint_name(shadow_paint_name);
    register_shadow_display(shadow_display);
    register_shadow_display_vertical(shadow_display_vertical);
    register_get_path_feature(rockface_control_get_path_feature);

    if (play_wav_thread_init())
        return -1;

    play_wav_signal(WELCOME_WAV);

    rockface_control_init(face_cnt);

    if (g_isp_en)
        if (rkisp_control_init())
            return -1;

    if (g_cif_en)
        rkcif_control_init();

    ui_run();

    if (g_isp_en)
        rkisp_control_exit();

    if (g_cif_en)
        rkcif_control_exit();

    rockface_control_exit();

    play_wav_thread_exit();

    return 0;
}
