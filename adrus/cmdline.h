/*
 * adrus - simple CLI note manager
 * Copyright (C) 2024  Jacob Sinclair <jcbsnclr@outlook.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
 
#include <jbase.h>
#include <db.h>

typedef enum {
    CMD_QUERY,
    CMD_OPEN,
    CMD_MODIFY,
    CMD_RM,
    CMD_LS,
} cmd_t;

typedef struct {
    cmd_t cmd;

    char path[PATH_MAX + 1];

    db_tag_t *tags;
    size_t len;
} cmdline_t;

jb_res_t cmdline_parse(cmdline_t *cmd, db_t *db, int argc, char *argv[]);
