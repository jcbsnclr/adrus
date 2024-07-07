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

#define FNV_OFFSET_BASIS 0xcbf29ce484222325
#define FNV_PRIME 0x100000001b3

jb_hash_t jb_fnv1a(const void *buf, size_t bytes) {
    uint8_t *ptr = (uint8_t *)buf;

    jb_hash_t hash = FNV_OFFSET_BASIS;

    for (size_t i = 0; i < bytes; i++) {
        hash *= FNV_PRIME;
        hash ^= ptr[i];
    }

    return hash;
}

jb_hash_t jb_fnv1a_str(const char *str) {
    size_t len = strlen(str);

    return jb_fnv1a(str, len);
}
