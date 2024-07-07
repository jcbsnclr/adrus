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
#include <stdio.h>
#include <string.h>

typedef struct {
    size_t start;
    char kind;
    jb_cmd_t *body;
} frame_t;

typedef bool (*jb_ccond_t)(char);

static char peek(jb_lexer_t *lx) {
    if (lx->pos >= lx->len) return '\0';

    return lx->src[lx->pos];
}

static bool take_if(jb_lexer_t *lx, jb_ccond_t cond) {
    if (cond(peek(lx)) && lx->pos < lx->len) {
        lx->pos++;
        return true;
    }

    return false;
}

static bool take_ifc(jb_lexer_t *lx, char c) {
    if (peek(lx) == c && lx->pos < lx->len) {
        lx->pos++;
        return true;
    }

    return false;
}

static bool take(jb_lexer_t *lx, char *c) {
    if (lx->pos >= lx->len) return false;

    *c = lx->src[lx->pos++];

    return true;
}

static bool take_while(jb_lexer_t *lx, jb_ccond_t cond) {
    bool taken = false;

    while (take_if(lx, cond)) taken = true;

    return taken;
}

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_ws(char c) {
    return isspace(c);
}

static bool is_not_nl(char c) {
    return c != '\n';
}

static bool is_not_tok_end(char c) {
    return c != '{' && c != '}' && c != '[' && c != ']' && !is_ws(c) && c != ';' && c != '"';
}

static frame_t *stack_init() {
    frame_t init_frame;
    init_frame.body = JB_BUF;
    init_frame.kind = '\0';
    init_frame.start = 0;

    jb_cmd_t init_cmd;
    init_cmd.body = JB_BUF;
    init_cmd.start = 0;
    init_cmd.end = 0;

    frame_t *stack = JB_BUF;

    jb_buf_push(init_frame.body, init_cmd);
    jb_buf_push(stack, init_frame);

    return stack;
}

static void stack_push(frame_t **stack, jb_val_t val) {
    frame_t *last_frame = jb_buf_last(*stack);
    jb_cmd_t *last_cmd = jb_buf_last(last_frame->body);

    jb_info("last_frame = %p last_cmd = %p", last_frame, last_cmd);

    jb_buf_push(last_cmd->body, val);

    if (last_cmd->start > val.start) last_cmd->start = val.start;
    last_cmd->end = val.end;
}

static void stack_close_cmd(frame_t **stack, size_t pos) {
    frame_t *last_frame = jb_buf_last(*stack);

    jb_buf_last(last_frame->body)->end = pos;

    jb_cmd_t cmd;
    cmd.body = JB_BUF;
    cmd.start = SIZE_MAX;
    cmd.end = pos + 1;

    jb_buf_push(last_frame->body, cmd);
}

static bool matches(char open, char close) {
    return (open == '{' && close == '}') || (open == '[' && close == ']') ||
           (open == '(' && close == ')');
}

static void stack_open(frame_t **stack, size_t start, char kind) {
    frame_t frame;
    frame.kind = kind;
    frame.start = start;
    frame.body = JB_BUF;

    jb_cmd_t cmd;
    cmd.start = start + 1;
    cmd.end = cmd.start;
    cmd.body = JB_BUF;

    jb_buf_push(frame.body, cmd);
    jb_buf_push(*stack, frame);
}

static jb_res_t stack_close(frame_t **stack, size_t end, char kind) {
    frame_t cur;
    if (!jb_buf_pop(*stack, cur)) return JB_ERR(JB_ERR_PARSER, "parser stack exhausted");

    if (!matches(cur.kind, kind))
        return JB_ERR(JB_ERR_PARSER, "unexpected closing delimiter '%c'", kind);

    jb_val_t val;
    val.start = cur.start;
    val.end = end + 1;

    if (kind == '}')
        val.kind = JB_VAL_QUOTE;
    else if (kind == ']')
        val.kind = JB_VAL_INLINE;

    val.body = cur.body;

    stack_push(stack, val);

    return JB_OK_VAL;
}

static void take_sym(jb_lexer_t *lx, jb_val_t *val) {
    val->kind = JB_VAL_STR;
    val->str_val = JB_BUF;
    val->start = lx->pos;
    take_while(lx, is_not_tok_end);
    val->end = lx->pos;

    size_t len = val->end - val->start;

    jb_buf_fit(val->str_val, len);
    jb_buf_hdr(val->str_val)->len = len;

    memcpy(val->str_val, lx->src + val->start, len);
}

static jb_res_t take_int(jb_lexer_t *lx, jb_val_t *val) {
    val->start = lx->pos;
    take_while(lx, is_digit);
    val->end = lx->pos;

    val->kind = JB_VAL_INT;
    char *endptr;
    val->int_val = strtoll(lx->src + val->start, &endptr, 10);

    if (endptr != lx->src + val->end)
        return JB_ERR(JB_ERR_PARSER, "failed to fully tokenize integer");

    return JB_OK_VAL;
}

static jb_res_t take_str_lit(jb_lexer_t *lx, jb_val_t *val) {
    val->kind = JB_VAL_STR;
    val->str_val = JB_BUF;
    val->start = lx->pos;
    if (!take_ifc(lx, '"')) return JB_ERR(JB_ERR_PARSER, "expected string lit");

    bool closed = false;

    char c;
    while (take(lx, &c)) {  // while not EOF
        if (c == '"') {     // string ended
            closed = true;
            break;
        }

        if (c == '\\') {
            if (!take(lx, &c)) return JB_ERR(JB_ERR_PARSER, "expected escape sequence, found EOF");

            switch (c) {  // handle escape sequence
                case 'n':
                    jb_buf_push(val->str_val, '\n');
                    break;
                case 'r':
                    jb_buf_push(val->str_val, '\r');
                    break;
                case 't':
                    jb_buf_push(val->str_val, '\t');
                    break;
                case 'v':
                    jb_buf_push(val->str_val, '\v');
                    break;

                default:
                    return JB_ERR(JB_ERR_PARSER, "unknown escape sequence '%c'", c);
            }
        } else {  // normal character
            jb_buf_push(val->str_val, c);
        }
    }

    if (!closed) return JB_ERR(JB_ERR_PARSER, "unclosed string literal");

    val->end = ++lx->pos;

    return JB_OK_VAL;
}

jb_res_t jb_parse(char *src, jb_val_t *val) {
    jb_lexer_t lx;
    lx.len = strlen(src);
    lx.pos = 0;
    lx.src = src;

    frame_t *stack = stack_init();

    while (lx.pos < lx.len) {
        char c = peek(&lx);
        jb_val_t val;

        if (c == '#') {
            take_while(&lx, is_not_nl);
        } else if (is_ws(c)) {
            take_while(&lx, is_ws);
        } else if (is_digit(c)) {
            JB_TRY(take_int(&lx, &val));
            stack_push(&stack, val);
        } else if (c == '{' || c == '[') {
            stack_open(&stack, lx.pos++, c);
        } else if (c == '}' || c == ']') {
            JB_TRY(stack_close(&stack, ++lx.pos, c));
        } else if (c == '"') {
            JB_TRY(take_str_lit(&lx, &val));
            stack_push(&stack, val);
        } else if (take_ifc(&lx, ';')) {
            stack_close_cmd(&stack, ++lx.pos);
        } else if (c == '$') {
            size_t start = lx.pos++;
            take_sym(&lx, &val);
            val.start = start;
            val.kind = JB_VAL_REF;
            stack_push(&stack, val);
        } else {
            take_sym(&lx, &val);
            stack_push(&stack, val);
        }
    }

    val->kind = JB_VAL_QUOTE;
    val->body = stack[0].body;
    val->start = 0;
    val->end = lx.len;

    jb_buf_free(stack);

    return JB_OK_VAL;
}

void jb_write_val(FILE *f, jb_val_t *val, size_t indent) {
    for (size_t i = 0; i < indent; i++) fputc(' ', f);

    switch (val->kind) {
        case JB_VAL_INT:
            fprintf(f, "Int %ld (%lu -> %lu)\n", val->int_val, val->start, val->end);
            break;
        case JB_VAL_STR:
            fprintf(f,
                    "Str \"%.*s\" (%lu -> %lu)\n",
                    (int)jb_buf_len(val->str_val),
                    val->str_val,
                    val->start,
                    val->end);
            break;
        case JB_VAL_REF:
            fprintf(f,
                    "Ref \"%.*s\" (%lu -> %lu)\n",
                    (int)jb_buf_len(val->str_val),
                    val->str_val,
                    val->start,
                    val->end);
            break;

        case JB_VAL_QUOTE:
            fprintf(f, "Quote (%lu -> %lu):\n", val->start, val->end);

            for (size_t i = 0; i < jb_buf_len(val->body); i++) {
                jb_val_t *cmd = val->body[i].body;

                for (size_t i = 0; i < indent + 2; i++) fputc(' ', f);
                fprintf(f, "Command (%lu -> %lu):\n", val->start, val->end);
                for (size_t j = 0; j < jb_buf_len(cmd); j++) jb_write_val(f, &cmd[j], indent + 4);
            }

            break;
        case JB_VAL_INLINE:
            fprintf(f, "Inline (%lu -> %lu):\n", val->start, val->end);

            for (size_t i = 0; i < jb_buf_len(val->body); i++) {
                jb_val_t *cmd = val->body[i].body;

                for (size_t i = 0; i < indent + 2; i++) fputc(' ', f);
                fprintf(f, "Command (%lu -> %lu):\n", val->start, val->end);
                for (size_t j = 0; j < jb_buf_len(cmd); j++) jb_write_val(f, &cmd[j], indent + 4);
            }

            break;
    }
}
