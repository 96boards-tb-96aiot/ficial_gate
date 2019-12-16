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
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#include <minigui/common.h>
#include <minigui/minigui.h>
#include <minigui/gdi.h>
#include <minigui/window.h>
#include <minigui/ctrl/static.h>
#include <minigui/control.h>

#include "face_common.h"
#include "rockface_control.h"
#include "shadow_display.h"

#define IDC_REGISTER 500
#define IDC_DELETE 501
#define IDC_LOGO 502

#define BUTTON_WIDTH 150
#define BUTTON_HEIGHT 60

#define TEXT_SIZE 36

#define IMG_LOGO_HEIGHT 0//145
#define IMAGE_NAME_LEN 128
#define RES_PATH "/usr/local/share/minigui/res/images/"

static HWND g_main_hwnd = HWND_INVALID;
static PLOGFONT g_font = NULL;
DWORD g_bkcolor;

char img_logo[][IMAGE_NAME_LEN] = {"img_logo.png"};
BITMAP img_logo_bmap;
void *image_array[][2] = {
    {img_logo, &img_logo_bmap},
};

static unsigned int g_left, g_top, g_right, g_bottom;
static char g_name[NAME_LEN];
static bool g_update = false;
static bool g_real = false;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int loadres(void)
{
    char img[256];
    for (int j = 0; j < sizeof(image_array) / sizeof(image_array[0]); j++) {
        snprintf(img, sizeof(img), "%s%s", RES_PATH, image_array[j][0]);
        if (LoadBitmap(HDC_SCREEN, image_array[j][1], img))
            printf("LoadBitmap %s fail!\n", img);
    }
    return 0;
}

void unloadres(void)
{
    for (int j = 0; j < sizeof(image_array) / sizeof(image_array[0]); j++)
        UnloadBitmap(image_array[j][1]);
}

static int draw_text(HDC hdc, const char *buf, int n, int left, int top,
                     int right, int bottom, UINT format)
{
    RECT rc;

    rc.left = left;
    rc.top = top;
    rc.right = right;
    rc.bottom = bottom;
    return DrawText(hdc, buf, n, &rc, format);
}

static void ui_paint(HWND hwnd)
{
    HDC hdc;
    hdc = BeginPaint(hwnd);
    SetBkColor(hdc, g_bkcolor);
    pthread_mutex_lock(&mutex);
    if (g_update) {
        if (strlen(g_name)) {
            if (g_real) {
                SetTextColor(hdc, PIXEL_green);
                SetPenColor(hdc, PIXEL_green);
            } else {
                SetTextColor(hdc, PIXEL_yellow);
                SetPenColor(hdc, PIXEL_yellow);
            }
            draw_text(hdc, g_name, -1, g_left + 1, g_top + 1, g_right, g_bottom,
                    DT_NOCLIP | DT_SINGLELINE | DT_LEFT | DT_TOP);
        } else {
            SetTextColor(hdc, PIXEL_red);
            SetPenColor(hdc, PIXEL_red);
        }
        Rectangle(hdc, g_left, g_top, g_right, g_bottom);
        g_update = false;
    }
    pthread_mutex_unlock(&mutex);
    EndPaint(hwnd, hdc);
}

static LRESULT ui_win_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
{
    HDC hdc;

    switch (message) {
    case MSG_CREATE:
#if 0
        CreateWindow(CTRL_STATIC, "",
                SS_REALSIZEIMAGE | SS_CENTERIMAGE | SS_BITMAP | WS_CHILD | WS_VISIBLE,
                IDC_LOGO, 0, 0, g_rcScr.right, IMG_LOGO_HEIGHT, hwnd, (DWORD)&img_logo_bmap);
#endif
        break;
    case MSG_TIMER:
        break;
    case MSG_PAINT:
        ui_paint(hwnd);
        break;
    case MSG_KEYDOWN :
        switch (w_param) {
        case SCANCODE_ENTER:
            break;
        case SCANCODE_CURSORBLOCKLEFT:
            break;
        }
        break;
    case MSG_COMMAND:
        switch (w_param) {
        case IDC_REGISTER:
            rockface_control_set_delete();
            break;
        case IDC_DELETE:
            rockface_control_set_register();
            break;
        }
        break;
    }
    return DefaultMainWinProc(hwnd, message, w_param, l_param);
}

void ui_run(void)
{
    MSG msg;
    HDC sndHdc;
    MAINWINCREATE create_info;
    HWND reg_hwnd;
    HWND del_hwnd;

    loadres();

    memset(&create_info, 0, sizeof(MAINWINCREATE));
    create_info.dwStyle = WS_VISIBLE;
    create_info.dwExStyle = WS_EX_NONE | WS_EX_AUTOSECONDARYDC;
    create_info.spCaption = "ui";
    //create_info.hCursor = GetSystemCursor(0);
    create_info.hIcon = 0;
    create_info.MainWindowProc = ui_win_proc;
    create_info.lx = 0;
    create_info.ty = 0;
    create_info.rx = g_rcScr.right;
    create_info.by = g_rcScr.bottom;
    create_info.dwAddData = 0;
    create_info.hHosting = HWND_DESKTOP;
    //  create_info.language = 0; //en

    g_main_hwnd = CreateMainWindow(&create_info);
    if (g_main_hwnd == HWND_INVALID) {
        printf("%s failed\n", __func__);
        return;
    }

    g_bkcolor = GetWindowElementPixel(g_main_hwnd, WE_BGC_DESKTOP);
    SetWindowBkColor(g_main_hwnd, g_bkcolor);

    ShowWindow(g_main_hwnd, SW_SHOWNORMAL);
    sndHdc = GetSecondaryDC((HWND)g_main_hwnd);
    SetMemDCAlpha(sndHdc, MEMDC_FLAG_SWSURFACE, 0);

    g_font = CreateLogFont(FONT_TYPE_NAME_SCALE_TTF, "ubuntuMono", "ISO8859-1",
                           FONT_WEIGHT_REGULAR, FONT_SLANT_ROMAN, FONT_FLIP_NIL,
                           FONT_OTHER_NIL, FONT_UNDERLINE_NONE, FONT_STRUCKOUT_NONE,
                           TEXT_SIZE, 0);
    if (!g_font) {
        printf("%s create font failed\n", __func__);
        return;
    }
    SetWindowFont(g_main_hwnd, g_font);

    RegisterMainWindow(g_main_hwnd);

    reg_hwnd = CreateWindow(CTRL_BUTTON, "Register", WS_VISIBLE | BS_DEFPUSHBUTTON | WS_CHILD,
                 IDC_REGISTER, g_rcScr.right - BUTTON_WIDTH, g_rcScr.bottom - BUTTON_HEIGHT,
                 BUTTON_WIDTH, BUTTON_HEIGHT, g_main_hwnd, 0);
    if (reg_hwnd <= HWND_NULL)
        return;
    SetWindowFont(reg_hwnd, g_font);

    del_hwnd = CreateWindow(CTRL_BUTTON, "Delete", WS_VISIBLE | BS_DEFPUSHBUTTON | WS_CHILD,
                 IDC_DELETE, 0, g_rcScr.bottom - BUTTON_HEIGHT,
                 BUTTON_WIDTH, BUTTON_HEIGHT, g_main_hwnd, 0);
    if (del_hwnd <= HWND_NULL)
        return;
    SetWindowFont(del_hwnd, g_font);

    while (GetMessage(&msg, g_main_hwnd)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DestroyLogFont(g_font);
    DestroyWindow(reg_hwnd);
    DestroyWindow(del_hwnd);
    DestroyMainWindow(g_main_hwnd);
    MainWindowThreadCleanup(g_main_hwnd);
    unloadres();
}

void ui_paint_box(int width, int height, int left, int top, int right, int bottom)
{
#define MIN_POS_DIFF 10
    int ui_width, ui_height;
    int l, t, r, b;
    shadow_get_crop_screen(&ui_width, &ui_height);
    if (width > 0 && height > 0 && left > 0 && right < width && top > 0 && bottom < height) {
        l = ui_width * left / width;
        t = ui_height * top / height;
        r = ui_width * right / width;
        b = ui_height * bottom / height;
        if (abs(g_left - l) > MIN_POS_DIFF || abs(g_top - t) > MIN_POS_DIFF ||
            abs(g_right - r) > MIN_POS_DIFF || abs(g_bottom - b) > MIN_POS_DIFF) {
            pthread_mutex_lock(&mutex);
            g_left = l;
            g_top = t;
            g_right = r;
            g_bottom = b;
            g_update = true;
            pthread_mutex_unlock(&mutex);
        }
    } else {
        if (g_left || g_top || g_right || g_bottom) {
            pthread_mutex_lock(&mutex);
            g_left = 0;
            g_top = 0;
            g_right = 0;
            g_bottom = 0;
            g_update = true;
            pthread_mutex_unlock(&mutex);
        }
    }
}

void ui_paint_name(char *name, bool real)
{
    char temp[NAME_LEN];
    memset(temp, 0, sizeof(temp));
    pthread_mutex_lock(&mutex);
    if (name) {
        if (strncmp(g_name, name, sizeof(g_name))) {
            memset(g_name, 0, sizeof(g_name));
            strncpy(g_name, name, sizeof(g_name) - 1);
            g_update = true;
        }
    } else {
        if (memcmp(g_name, temp, sizeof(g_name))) {
            memset(g_name, 0, sizeof(g_name));
            g_update = true;
        }
    }
    if (g_real != real) {
        g_update = true;
        g_real = real;
    }
    pthread_mutex_unlock(&mutex);
}

void ui_paint_refresh(void)
{
    int ui_width, ui_height;
    shadow_get_crop_screen(&ui_width, &ui_height);
    RECT rect = {0, IMG_LOGO_HEIGHT, ui_width, ui_height - IMG_LOGO_HEIGHT};
    if (g_main_hwnd == HWND_INVALID)
        return;
    pthread_mutex_lock(&mutex);
    if (g_update)
        InvalidateRect(g_main_hwnd, &rect, TRUE);
    else
        InvalidateRect(g_main_hwnd, &rect, FALSE);
    pthread_mutex_unlock(&mutex);
}
