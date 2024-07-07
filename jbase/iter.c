/*
 * jbase - C utility library
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

#include <jbase.h>

#include "string.h"

#define ITER_MAGIC 0x5f4a425f49544552

static bool not_sep(char c) {
    return c != '/';
}

static bool path_parts_next(void *iter, void *out) {
    jb_path_parts_t *parts = (jb_path_parts_t *)iter;

    if (jb_lx_eof(&parts->lx)) return false;

    if (!jb_lx_take_ifc(&parts->lx, '/')) {
        jb_warn("path_parts_t in invalid state; c = %c, pos = %lu, path = '%s'",
                jb_lx_peek(&parts->lx),
                parts->lx.pos,
                parts->lx.src);

        return false;
    }

    size_t start = parts->lx.pos;
    jb_lx_take_while(&parts->lx, not_sep);
    size_t end = parts->lx.pos;

    size_t len = end - start;

    if (len == 0) return path_parts_next(iter, out);

    memcpy(out, parts->lx.src + start, len);

    return true;
}

bool jb_dir_parts(jb_path_parts_t *parts, const char *path) {
    if (*path != '/') {
        jb_warn("invalid adrus path '%s'", path);
        return false;
    }

    size_t len = strnlen(path, PATH_MAX);
    if (path[len - 1] == '/') len -= 1;

    char *pos = strrchr(path, '/');

    if (pos == path) {
        jb_warn("invalid adrus path '%s'", path);
        return false;
    }

    len = path - pos;

    jb_iter_init(parts);
    parts->iter.next = path_parts_next;
    jb_lx_init(&parts->lx, path, len);

    return true;
}

bool jb_path_parts(jb_path_parts_t *parts, const char *path) {
    if (*path != '/') {
        jb_warn("invalid adrus path '%s'", path);
        return false;
    }

    size_t len = strnlen(path, PATH_MAX);

    if (path[len - 1] == '/') len -= 1;

    jb_iter_init(parts);
    parts->iter.next = path_parts_next;
    jb_lx_init(&parts->lx, path, len);

    return true;
}

void jb_iter_init(void *iter) {
    jb_iter_t *i = (jb_iter_t *)iter;
    i->magic = ITER_MAGIC;
}

bool jb_iter_next(void *iter, void *out) {
    jb_iter_t *i = (jb_iter_t *)iter;

    if (i->magic != ITER_MAGIC) {
        jb_error("object %p is not iterator");
        return false;
    }

    return i->next(iter, out);
}
