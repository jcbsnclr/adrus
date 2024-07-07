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

#include <ctype.h>
#include <jbase.h>

static char *symbols = "!£$%^&*;:@#~,<.>/?\\|";

static bool str_has(const char *str, char c) {
    while (*str) {
        if (*str == c) {
            return true;
        }
        str++;
    }

    return false;
}

bool jb_lx_tok(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           str_has(symbols, c);
}

bool jb_lx_ws(char c) {
    return isspace(c);
}

void jb_lx_init_str(jb_lexer_t *lx, const char *src) {
    jb_lx_init(lx, src, strlen(src));
}

void jb_lx_init(jb_lexer_t *lx, const char *src, size_t len) {
    lx->src = src;
    lx->len = len;
    lx->pos = 0;
}

char jb_lx_peek(jb_lexer_t *lx) {
    if (lx->pos >= lx->len) return '\0';

    return lx->src[lx->pos];
}

bool jb_lx_take_if(jb_lexer_t *lx, jb_ccond_t cond) {
    if (cond(jb_lx_peek(lx)) && lx->pos < lx->len) {
        lx->pos++;
        return true;
    }

    return false;
}

bool jb_lx_take_ifc(jb_lexer_t *lx, char c) {
    if (jb_lx_peek(lx) == c && lx->pos < lx->len) {
        lx->pos++;
        return true;
    }

    return false;
}

bool jb_lx_take(jb_lexer_t *lx, char *c) {
    if (lx->pos >= lx->len) return false;

    *c = lx->src[lx->pos++];

    return true;
}

bool jb_lx_take_while(jb_lexer_t *lx, jb_ccond_t cond) {
    bool taken = false;

    while (jb_lx_take_if(lx, cond)) taken = true;

    return taken;
}

bool jb_lx_eof(jb_lexer_t *lx) {
    return lx->pos >= lx->len;
}
