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
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "database.h"
#include "face_common.h"

#define DATABASE_TABLE "face_data"

static sqlite3 *g_db = NULL;

int database_init(void)
{
    char *err;
    char cmd[256];

    if (sqlite3_open(DATABASE_PATH, &g_db) != SQLITE_OK) {
        printf("%s open database %s failed!\n", __func__, DATABASE_PATH);
        return -1;
    }
    snprintf(cmd, sizeof(cmd),
             "CREATE TABLE IF NOT EXISTS %s (data blob, name varchar(%d) UNIQUE)",
             DATABASE_TABLE, NAME_LEN);
    if (sqlite3_exec(g_db, cmd, 0, 0, &err) != SQLITE_OK) {
        sqlite3_close(g_db);
        g_db = NULL;
        printf("%s create table %s failed!\n", __func__, DATABASE_TABLE);
        return -1;
    }

    return 0;
}

void database_exit()
{
    sqlite3_close(g_db);
}

int database_insert(void *data, size_t size, char *name, size_t n_size, bool sync_flag)
{
    char cmd[256];
    sqlite3_stmt *stat = NULL;

    if (n_size > NAME_LEN) {
        printf("%s n_size error\n", __func__);
        return -1;
    }
    snprintf(cmd, sizeof(cmd), "INSERT INTO %s VALUES(?, '%s');", DATABASE_TABLE, name);
    if (sqlite3_prepare(g_db, cmd, -1, &stat, 0) != SQLITE_OK)
        return -1;
    sqlite3_exec(g_db, "begin transaction", NULL, NULL, NULL);
    sqlite3_bind_blob(stat, 1, data, size, NULL);
    sqlite3_step(stat);
    sqlite3_finalize(stat);
    sqlite3_exec(g_db, "commit transaction", NULL, NULL, NULL);
    if (sync_flag)
        sync();

    return 0;
}

int database_record_count()
{
    int ret = 0;
    char cmd[256];
    sqlite3_stmt *stat = NULL;

    snprintf(cmd, sizeof(cmd), "SELECT COUNT(*) FROM %s;", DATABASE_TABLE);
    if (sqlite3_prepare(g_db, cmd, -1, &stat, 0) != SQLITE_OK)
        return 0;
    if (sqlite3_step(stat) == SQLITE_ROW)
        ret = sqlite3_column_int(stat, 0);
    sqlite3_finalize(stat);

    return ret;
}

int database_get_data(void *dst, const int cnt, size_t d_size, size_t d_off,
                      size_t n_size, size_t n_off)
{
    int ret = 0;
    char cmd[256];
    sqlite3_stmt *stat = NULL;
    int index = 0;
    const char *name;
    const void *data;
    size_t size;

    snprintf(cmd, sizeof(cmd), "SELECT * FROM %s;", DATABASE_TABLE);
    if (sqlite3_prepare(g_db, cmd, -1, &stat, 0) != SQLITE_OK)
        return 0;
    while (1) {
        ret = sqlite3_step(stat);
        if (ret != SQLITE_ROW)
            break;
        data = sqlite3_column_blob(stat, 0);
        size = sqlite3_column_bytes(stat, 0);
        if (size <= d_size)
            memcpy((char*)dst + index * (n_size + d_size) + d_off, data, size);
        name = sqlite3_column_text(stat, 1);
        size = sqlite3_column_bytes(stat, 1);
        if (size <= n_size)
            strncpy((char*)dst + index * (n_size + d_size) + n_off, name, n_size);
        if (++index >= cnt)
            break;
    }
    sqlite3_finalize(stat);

    return index;
}

bool database_is_name_exist(char *name)
{
    bool exist = false;
    int ret = 0;
    char cmd[256];
    sqlite3_stmt *stat = NULL;

    snprintf(cmd, sizeof(cmd), "SELECT * FROM %s WHERE name = '%s' LIMIT 1;", DATABASE_TABLE, name);
    if (sqlite3_prepare(g_db, cmd, -1, &stat, 0) != SQLITE_OK)
        return false;
    ret = sqlite3_step(stat);
    if (ret == SQLITE_ROW)
        exist = true;
    else
        exist = false;
    sqlite3_finalize(stat);

    return exist;
}

int database_get_user_name_id(void)
{
    int ret = 0;
    char cmd[256];
    sqlite3_stmt *stat = NULL;
    int id = 0;
    const char *name;
    int max_id = -1;
    int *save_id = NULL;
    int ret_id = 0;

    snprintf(cmd, sizeof(cmd), "SELECT * FROM %s WHERE name like '%s%%';",
             DATABASE_TABLE, USER_NAME);

    if (sqlite3_prepare(g_db, cmd, -1, &stat, 0) != SQLITE_OK)
        return -1;
    while (1) {
        ret = sqlite3_step(stat);
        if (ret != SQLITE_ROW)
            break;
        name = sqlite3_column_text(stat, 1);
        sscanf(name, "%*[^_]_%d", &id);
        max_id = (max_id >= id ? max_id : id);
    }
    sqlite3_finalize(stat);

    if (max_id < 0)
        return 0;

    save_id = calloc(max_id + 1, sizeof(int));
    if (!save_id) {
        printf("%s: memory alloc fail!\n", __func__);
        return -1;
    }

    if (sqlite3_prepare(g_db, cmd, -1, &stat, 0) != SQLITE_OK) {
        ret_id = -1;
        goto exit;
    }
    while (1) {
        ret = sqlite3_step(stat);
        if (ret != SQLITE_ROW)
            break;
        name = sqlite3_column_text(stat, 1);
        sscanf(name, "%*[^_]_%d", &id);
        save_id[id] = 1;
    }
    sqlite3_finalize(stat);

    for (int i = 0; i < max_id + 1; i++) {
        if (!save_id[i]) {
            ret_id = i;
            goto exit;
        }
    }
    ret_id = max_id + 1;

exit:
    if (save_id)
        free(save_id);
    return ret_id;
}

void database_delete(char *name, bool sync_flag)
{
    char cmd[256];

    snprintf(cmd, sizeof(cmd), "DELETE FROM %s WHERE name = '%s';", DATABASE_TABLE, name);
    sqlite3_exec(g_db, "begin transaction", NULL, NULL, NULL);
    sqlite3_exec(g_db, cmd, NULL, NULL, NULL);
    sqlite3_exec(g_db, "commit transaction", NULL, NULL, NULL);
    if (sync_flag)
        sync();
}
