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

#include <jbase.h>
#include <linux/limits.h>
#include <stddef.h>
#include <string.h>
#include <util.h>

jb_res_t path_cat(const char *a, const char *b, char *out) {
    size_t a_len = strnlen(a, PATH_MAX);
    size_t b_len = strnlen(b, PATH_MAX);

    if (a_len + b_len > PATH_MAX) return JB_ERR(JB_ERR_USER, "path exceeds PATH_MAX: %s%s", a, b);

    memcpy(out, a, a_len);
    memcpy(out + a_len, b, b_len);

    return JB_OK_VAL;
}
