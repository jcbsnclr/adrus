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

#include <stdarg.h>
#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include <cmdline.h>

typedef struct {
    char **args;
    int len;
    int ptr;
} args_t;

// check if at end of arg stream
static bool end(args_t *args) {
    return args->ptr >= args->len;
}

// consume next arg
static char *take(args_t *args) {
    if (end(args)) return NULL;
    return args->args[args->ptr++];
}

static char *peek(args_t *args) {
    if (end(args)) return NULL;
    return args->args[args->ptr];
}

// consume next arg if it's a path
static bool take_path(args_t *args, char path[PATH_MAX + 1]) {
    if (end(args)) return false;
    char *arg = args->args[args->ptr];

    if (*arg != '/') return false;
    strncpy(path, arg, PATH_MAX);

    take(args);

    return true;
}

typedef struct {
    char *str;
    int val;
} one_of_t;

#define LAST_OF               \
    (one_of_t) {              \
        .str = NULL, .val = 0 \
    }

static int take_one_of(args_t *args, ...) {
    va_list v;
    va_start(v, args);

    char *arg = peek(args);

    if (arg) {
        one_of_t item;
        for (;;) {
            item = va_arg(v, one_of_t);

            if (!item.str) break;

            if (strcmp(item.str, arg) == 0) {
                take(args);
                return item.val;
            }
        }
    }

    return CMD_QUERY;
}

// consume next argument if it's a tag (+foo/-foo)
static bool take_tag(args_t *args, db_t *db, db_tag_t *f) {
    if (end(args)) return false;
    char *arg = args->args[args->ptr];

    if (*arg == '+')
        f->sign = true;
    else if (*arg == '-')
        f->sign = false;
    else
        return false;

    tag_entry_t *tag = db_def_tag(db, arg + 1);

    if (!tag) jb_warn("no notes with tag '%s'", arg + 1);

    f->tag = tag;
    take(args);

    return true;
}

jb_res_t cmdline_parse(cmdline_t *cmd, db_t *db, int argc, char *argv[]) {
    db_tag_t *buf = JB_BUF;
    args_t args = {argv, argc, 1};

    memset(cmd->path, 0, PATH_MAX + 1);
    cmd->len = 0;
    cmd->cmd = take_one_of(&args, (one_of_t){"rm", CMD_RM}, (one_of_t){"ls", CMD_LS});

    if (cmd->cmd != CMD_QUERY) {
        if (!take_path(&args, cmd->path) || argc < 3)
            return JB_ERR(JB_ERR_USER, "usage: %s [PATH] +/-[TAG]...", argv[1]);

        db_tag_t f;
        while (take_tag(&args, db, &f)) {
            if (cmd->cmd == CMD_OPEN) cmd->cmd = CMD_MODIFY;
            jb_buf_push(buf, f);
        }

        (void)take;

        cmd->len = jb_buf_len(buf);
        size_t size = cmd->len * sizeof(db_tag_t);

        if (cmd->len != 0) {
            cmd->tags = malloc(size);
            memcpy(cmd->tags, buf, size);
        }

        return JB_OK_VAL;
    }

    if (take_path(&args, cmd->path)) cmd->cmd = CMD_OPEN;

    db_tag_t f;
    while (take_tag(&args, db, &f)) {
        if (cmd->cmd == CMD_OPEN) cmd->cmd = CMD_MODIFY;
        jb_buf_push(buf, f);
    }

    (void)take;

    cmd->len = jb_buf_len(buf);
    size_t size = cmd->len * sizeof(db_tag_t);

    if (cmd->len != 0) {
        cmd->tags = malloc(size);
        memcpy(cmd->tags, buf, size);
    }

    return JB_OK_VAL;
}
