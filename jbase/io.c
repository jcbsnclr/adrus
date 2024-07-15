/*
 * jbase - C utility library
 * Copyright (C) 2024  Jacob Sinclair <jcbsnclr@outlook.com>
 * * This program is free software: you can redistribute it and/or modify
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

#include <errno.h>
#include <jbase.h>
#include <stdio.h>
#include <string.h>

#include "stdlib.h"

jb_errno_t jb_load_file(const char *path, uint8_t **data, size_t *len) {
    FILE *f = fopen(path, "r");
    if (!f) return errno;

    if (fseeko(f, 0, SEEK_END) == -1) return errno;

    off_t l = ftello(f);
    if (l == -1) return errno;
    *len = l;

    rewind(f);

    *data = malloc(*len);
    if (fread(*data, 1, *len, f) != *len) return errno;

    if (fclose(f) == EOF) return errno;

    return 0;
}

jb_errno_t jb_store_file(const char *path, uint8_t *data, size_t len) {
    FILE *f = fopen(path, "w");
    if (!f) return errno;

    if (fwrite(data, 1, len, f) != len) return errno;

    if (fclose(f) == EOF) return errno;

    return 0;
}

jb_errno_t jb_io_buf_init(jb_io_buf_t *buf, size_t cap) {
    buf->cap = cap;
    buf->len = 0;
    buf->buf = malloc(cap);

    if (!buf->buf) return errno;

    memset(buf->buf, 0, buf->cap);

    return 0;
}

jb_errno_t jb_io_buf_write(jb_io_buf_t *buf, uint8_t *data, size_t len) {
    size_t new_len = buf->len + len;
    size_t new_cap = buf->cap;
    uint8_t *new_buf = buf->buf;

    if (new_len >= buf->cap) {
        while (new_cap < new_len) new_cap *= 2;
        new_buf = realloc(buf->buf, new_cap);

        if (!new_buf) return errno;
    }

    memcpy(new_buf + buf->len, data, len);
    memset(new_buf + new_len, 0, new_cap - new_len);

    buf->len = new_len;
    buf->cap = new_cap;
    buf->buf = new_buf;

    return 0;
}

jb_errno_t jb_io_buf_flush(jb_io_buf_t *buf, FILE *f) {
    size_t res = fwrite(buf->buf, 1, buf->len, f);
    if (res != buf->len) return errno;

    return 0;
}

void jb_io_buf_clear(jb_io_buf_t *buf) {
    buf->len = 0;
    memset(buf->buf, 0, buf->cap);
}

void jb_io_buf_free(jb_io_buf_t *buf) {
    free(buf->buf);
}

jb_errno_t jb_read_line(FILE *f, jb_io_buf_t *buf) {
    jb_errno_t err;

    char c;
    while (fread(&c, 1, 1, f) == 1 && c != '\n') {
        err = jb_io_buf_write(buf, (uint8_t *)&c, 1);
        if (err) return err;
    }

    if (ferror(f)) return errno;

    return 0;
}

jb_errno_t jb_open(const char *path, FILE **out, const char *mode) {
    FILE *f = fopen(path, mode);
    if (!f) return errno;

    *out = f;

    return 0;
}

jb_errno_t jb_basename(const char *path, char *out, size_t size) {
    JB_ASSERT_PTR(path);
    JB_ASSERT_PTR(out);
    JB_ASSERT(size != 0);

    memset(out, 0, size);

    size_t len = strlen(path);

    char *last = strrchr(path, '/');
    if (!last) {
        if (len > size) return ENAMETOOLONG;

        memcpy(out, path, len);
        return 0;
    }

    len = strlen(last + 1);
    if (len > size) return ENAMETOOLONG;

    // memcpy(out, last + 1, len);
    strncpy(out, last + 1, size - len);

    return 0;
}

jb_errno_t jb_dirname(const char *path, char *out, size_t size) {
    JB_ASSERT_PTR(path);
    JB_ASSERT_PTR(out);
    JB_ASSERT(size != 0);

    memset(out, 0, size);

    char *last = strrchr(path, '/');
    if (!last) return 0;

    size_t len = last - path;
    if (len > size) return ENAMETOOLONG;

    strncpy(out, path, len);

    return 0;
}

jb_errno_t jb_path_cat(const char *p1, const char *p2, char *out) {
    memset(out, 0, PATH_MAX + 1);

    size_t l1 = strnlen(p1, PATH_MAX);
    size_t l2 = strnlen(p2, PATH_MAX);

    if (l1 + l2 > PATH_MAX) return ENAMETOOLONG;

    // check if p1 ends in slash
    size_t comb = p1[l1 - 1] != '/' ? 1 : 0;

    memcpy(out, p1, l1);
    if (comb) out[l1] = '/';  // add slash between paths if necessary
    memcpy(out + l1 + comb, p2, l2);

    return 0;
}

jb_errno_t jb_path_res(const char *path, char *out) {
    if (!realpath(path, out)) return errno;
    return 0;
}

jb_errno_t jb_fstat(const char *path, struct stat *buf) {
    struct stat temp;
    struct stat *sbuf = buf ? buf : &temp;

    if (stat(path, sbuf) == -1) return errno;
    return 0;
}
