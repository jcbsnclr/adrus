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
#include <linux/limits.h>

#define BUCKETS 32
#define TAG_MAX 32

typedef struct note_entry {
    char path[PATH_MAX];
    jb_hash_t hash;

    time_t ctime, mtime;

    size_t len, cap;
    struct tag_entry **tags;

    struct note_entry *next;
} note_entry_t;

typedef struct tag_entry {
    char tag[TAG_MAX];
    jb_hash_t hash;

    size_t len, cap;
    note_entry_t **notes;

    struct tag_entry *next;
} tag_entry_t;

typedef struct {
    bool sign;
    tag_entry_t *tag;
} db_tag_t;

typedef struct {
    note_entry_t *bnotes[BUCKETS];
    tag_entry_t *btags[BUCKETS];

    char path[PATH_MAX + 1];
} db_t;

jb_res_t db_init(db_t *db);
void db_free(db_t *db);

void db_add_note(db_t *db, const char *path, time_t ctime, time_t mtime);
void db_tag_note(db_t *db, const char *path, const char *tag);
tag_entry_t *db_def_tag(db_t *db, const char *tag);

tag_entry_t *db_get_tag(db_t *db, const char *name);
note_entry_t *db_get_note(db_t *db, const char *path);

typedef void (*db_cb_t)(db_t *db, void *state, note_entry_t *note);

void db_query(db_t *db, void *state, db_tag_t *filter, size_t len, db_cb_t cb);
jb_res_t db_mutate(db_t *db, const char *note, db_tag_t *filter, size_t len);

jb_res_t db_gc(db_t *db);

void db_ls(db_t *db, const char *glob, db_tag_t *filter, size_t filter_len);
void db_rm(db_t *db, const char *glob, db_tag_t *filter, size_t filter_len);
