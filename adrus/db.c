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

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include <ctype.h>
#include <db.h>
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <jbase.h>
#include <stdio.h>
#include <string.h>
#include <util.h>

#include "ftw.h"
#include "stdlib.h"
#include "unistd.h"

JB_TLOCAL static db_t temp;

// read line into a jb_buf
static jb_res_t read_line(FILE *f, char **out) {
    char *buf = JB_BUF;
    char c;

    // read file until error or
    int res;
    while ((res = fread(&c, 1, 1, f)) == 1) {
        if (c == '\n') break;

        jb_buf_push(buf, c);
    }

    if (ferror(f)) return JB_ERR_LIBC(errno, "read_line failed");

    jb_buf_push(buf, '\0');
    // jb_trace("%s", buf);

    size_t len = jb_buf_len(buf);
    *out = malloc(len + 1);
    memcpy(*out, buf, len);
    (*out)[len] = '\0';

    return JB_OK_VAL;
}

static bool valid_hdr_char(char c) {
    return isspace(c) || isalnum(c) || c == '_';
}

// make sure header is valid
static bool validate_hdr(char *hdr) {
    while (*hdr) {
        if (!valid_hdr_char(*hdr)) return false;
        hdr++;
    }

    return true;
}

// process file tree
static jb_res_t process(const char *path, const struct stat *sb) {
    // char *bname = basename(p);
    char bname[NAME_MAX];
    jb_errno_t err = jb_basename(path, bname, NAME_MAX);
    JB_TRY_IO(err, "failed to get basename of path '%s'", path);

    size_t db_path_len = strlen(temp.path);
    char name[PATH_MAX];
    strcpy(name, path + db_path_len);

    FILE *f = fopen(bname, "r");

    if (!f) return JB_ERR_LIBC(errno, "failed to open file '%s'", path);

    char *line;
    JB_TRY(read_line(f, &line));

    if (!validate_hdr(line) || strncmp(line, "adrus", 5) != 0) {
        jb_debug("%s: not adrus file", path);
        return JB_OK_VAL;
    }

    jb_debug("%s: is adrus file", path);

    db_add_note(&temp, name, sb->st_ctime, sb->st_mtime);

    char *hdr = line + 5;
    int n = 0;
    for (;;) {
        char tag_buf[TAG_MAX];
        int res = sscanf(hdr, " %31[a-z]%n", tag_buf, &n);
        if (res != 1) break;
        hdr += n;

        jb_trace("  tag %s", tag_buf);

        db_tag_note(&temp, name, tag_buf);
    }

    return JB_OK_VAL;
}

static int fs_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    (void)ftwbuf;
    if (typeflag == FTW_F) {
        jb_res_t res = process(path, sb);

        if (res JB_IS_ERR) {
            jb_error("%s: %s", path, res.msg);
            free(res.msg);
        }
    }

    return 0;
}

jb_res_t db_init(db_t *db) {
    char *path = getenv("ADRUS_DIR");

    if (!path) {
        const char *adrus = "/.adrus";
        const char *home = getenv("HOME");
        if (!home) return JB_ERR(JB_ERR_USER, "env var HOME not set");

        path = malloc(strlen(home) + strlen(adrus) + 1);
        strcpy(path, home);
        strcat(path, adrus);
    }

    DIR *dir = opendir(path);

    if (!dir) return JB_ERR_LIBC(errno, "failed to open adrus dir '%s'", path);

    closedir(dir);

    if (!realpath(path, temp.path))
        return JB_ERR_LIBC(errno, "failed to get real path of '%s'", path);

    jb_info("scanning notebook '%s'", temp.path);

    for (size_t i = 0; i < BUCKETS; i++) {
        temp.bnotes[i] = NULL;
        temp.btags[i] = NULL;
    }

    nftw(temp.path, fs_cb, 0, FTW_CHDIR);

    *db = temp;

    return JB_OK_VAL;
}

static void free_note_chain(note_entry_t *chain) {
    if (!chain) return;
    if (chain->next) free_note_chain(chain->next);

    free(chain->tags);
    free(chain);
}

static void free_tag_chain(tag_entry_t *chain) {
    if (!chain) return;
    if (chain->next) free_tag_chain(chain->next);

    free(chain->notes);
    free(chain);
}

void db_free(db_t *db) {
    for (size_t i = 0; i < BUCKETS; i++) {
        free_note_chain(db->bnotes[i]);
        free_tag_chain(db->btags[i]);
    }
}

static note_entry_t *get_note_entry(db_t *db, const char *path) {
    jb_hash_t hash = jb_fnv1a_str(path);
    size_t bucket = hash % BUCKETS;

    note_entry_t *cur = db->bnotes[bucket];
    while (cur) {
        // jb_trace("cur->path = %s, path = %s", cur->path, path);
        if (cur->hash == hash && strcmp(cur->path, path) == 0) {
            return cur;
        }

        cur = cur->next;
    }

    return NULL;
}

tag_entry_t *db_get_tag(db_t *db, const char *tag) {
    jb_hash_t hash = jb_fnv1a_str(tag);
    size_t bucket = hash % BUCKETS;

    tag_entry_t *entry = db->btags[bucket];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->tag, tag) == 0) return entry;
        entry = entry->next;
    }

    return NULL;
}

note_entry_t *db_get_note(db_t *db, const char *path) {
    jb_hash_t hash = jb_fnv1a_str(path);
    size_t bucket = hash % BUCKETS;

    note_entry_t *entry = db->bnotes[bucket];
    while (entry) {
        if (entry->hash == hash && strcmp(entry->path, path) == 0) return entry;
        entry = entry->next;
    }

    return NULL;
}

tag_entry_t *db_def_tag(db_t *db, const char *tag) {
    tag_entry_t *entry = db_get_tag(db, tag);
    if (entry) return entry;

    jb_hash_t hash = jb_fnv1a_str(tag);
    size_t bucket = hash % BUCKETS;

    entry = malloc(sizeof(tag_entry_t));

    memcpy(entry->tag, tag, TAG_MAX);
    entry->hash = hash;
    entry->len = 0;
    entry->cap = 16;
    entry->notes = malloc(sizeof(note_entry_t *) * entry->cap);
    entry->next = db->btags[bucket];

    db->btags[bucket] = entry;

    return entry;
}

void db_add_note(db_t *db, const char *path, time_t ctime, time_t mtime) {
    note_entry_t *entry = get_note_entry(db, path);

    if (get_note_entry(db, path)) {
        jb_warn("note '%s' already registered");
        return;
    }

    entry = malloc(sizeof(note_entry_t));

    strncpy(entry->path, path, PATH_MAX - 1);
    entry->hash = jb_fnv1a_str(path);
    entry->ctime = ctime;
    entry->mtime = mtime;
    entry->next = db->bnotes[entry->hash % BUCKETS];

    entry->len = 0;
    entry->cap = 16;
    entry->tags = malloc(sizeof(tag_entry_t *) * entry->cap);

    db->bnotes[entry->hash % BUCKETS] = entry;
}

void db_tag_note(db_t *db, const char *path, const char *tag) {
    tag_entry_t *tag_e = db_def_tag(db, tag);
    note_entry_t *note_e = get_note_entry(db, path);

    if (!note_e) {
        jb_warn("no note '%s'", path);
        return;
    }

    if (!tag_e) {
        jb_error("failed to create tag '%s'", tag);
        return;
    }

    if (tag_e->len >= tag_e->cap) {
        tag_e->cap *= 2;
        tag_e->notes = realloc(tag_e->notes, sizeof(note_entry_t *) * tag_e->cap);
    }

    if (note_e->len >= note_e->cap) {
        note_e->cap *= 2;
        note_e->tags = realloc(note_e->tags, sizeof(tag_entry_t *) * note_e->cap);
    }

    tag_e->notes[tag_e->len++] = note_e;
    note_e->tags[note_e->len++] = tag_e;
}

static bool note_has_tag(note_entry_t *note, tag_entry_t *tag) {
    for (size_t i = 0; i < note->len; i++)
        if (note->tags[i] == tag) return true;

    return false;
}

void db_query(db_t *db, void *state, db_tag_t *filter, size_t len, db_cb_t cb) {
    // iterate through buckets
    for (size_t b = 0; b < BUCKETS; b++) {
        note_entry_t *note = db->bnotes[b];

        // iterate through notes in bucket
        while (note) {
            bool matches = true;

            // iterate through filter
            for (size_t f = 0; f < len; f++) {
                if (note_has_tag(note, filter[f].tag) != filter[f].sign) {
                    // fail matching if note has tag, and tag expected
                    matches = false;
                    break;
                }
            }

            // pass note to callback if matches filter
            if (matches) cb(db, state, note);

            note = note->next;
        }
    }
}

// jb_res_t db_mut(db_t *db, const char *path, db_tag_t *filter, size_t len) {
//     note_entry_t *note = db_get_note(db, path);
//     // tag_entry_t **tags = JB_BUF;  // tags to be serialized

//     if (!note) return JB_ERR(JB_ERR_USER, "no note '%s'", path);

//     jb_info("mutating note at '%s'", note->path);

//     return JB_OK_VAL;
// }

jb_res_t db_mutate(db_t *db, const char *name, db_tag_t *filter, size_t len) {
    note_entry_t *note = db_get_note(db, name);
    tag_entry_t **tags = JB_BUF;  // tags to be serialized

    char path[PATH_MAX];
    jb_path_cat(db->path, name, path);

    if (!note) return JB_ERR(JB_ERR_USER, "no note '%s'", name);

    jb_info("mutating note at '%s'", note->path);

    // open note for reading; we reopen with write later to avoid file overwrite
    FILE *f = fopen(path, "r");
    if (!f) return JB_ERR_LIBC(errno, "failed to open note '%s'", note->path);

    char *line;
    JB_TRY(read_line(f, &line));

    // make sure it's an adrus note
    if (!line || !validate_hdr(line) || strncmp(line, "adrus", 5) != 0)
        return JB_ERR(JB_ERR_USER, "path '%s' is not adrus note", note->path);

    // get ptr to after header
    int ptr = ftell(f);
    if (ptr == -1) return JB_ERR_LIBC(errno, "failed to get position in file");

    // length of file
    if (fseek(f, 0, SEEK_END) == -1) return JB_ERR_LIBC(errno, "failed to seek in file");
    int flen = ftell(f);
    if (flen == -1) return JB_ERR_LIBC(errno, "failed to get position in file");

    // content length
    size_t clen = flen - ptr;

    fseek(f, ptr, SEEK_SET);

    // read file after header into buffer
    char *content = NULL;
    if (clen > 0) {
        content = malloc(clen);

        if (fread(content, 1, clen, f) != clen)
            return JB_ERR_LIBC(errno, "failed to read contents of note");
    }

    // process negative (-foo) arguments
    for (size_t i = 0; i < note->len; i++) {
        bool filtered = false;

        // check if tag is filtered
        for (size_t j = 0; j < len; j++) {
            if (!filter[j].sign && filter[j].tag == note->tags[i]) {
                filtered = true;
                break;
            }
        }

        // add unfiltered tags to list
        if (!filtered) jb_buf_push(tags, note->tags[i]);  // NOLINT
    }

    for (size_t i = 0; i < len; i++) {
        bool dup = false;

        if (!filter[i].sign) continue;

        // remove duplicates
        for (size_t j = 0; j < jb_buf_len(tags); j++) {
            if (tags[j] == filter[i].tag) {
                dup = true;
                break;
            }
        }

        if (!dup) jb_buf_push(tags, filter[i].tag);  // NOLINT
    }

    // re-open file for writing
    f = freopen(NULL, "w", f);
    if (!f) return JB_ERR_LIBC(errno, "failed to open note for writing");

    // write magic
    fprintf(f, "adrus ");
    jb_debug("serialising tags");
    for (size_t i = 0; i < jb_buf_len(tags); i++) {
        jb_trace("  +%s", tags[i]->tag);
        fprintf(f, "%s ", tags[i]->tag);  // write tag to file
    }
    fprintf(f, "\n");

    // write file contents back to file
    if (content) fwrite(content, 1, clen, f);

    return JB_OK_VAL;
}

static jb_res_t delete_empty(const char *path, bool *e) {
    DIR *dir = opendir(path);
    struct dirent *ent;
    bool empty = true;

    jb_debug("processing directory '%s'", path);
    while ((ent = readdir(dir))) {
        switch (ent->d_type) {
            case DT_REG: {
                jb_trace("  file '%s'", ent->d_name);
                empty = false;
            } break;

            case DT_DIR: {
                if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

                jb_trace("  dir '%s'", ent->d_name);
                char buf[PATH_MAX];
                jb_errno_t err = jb_path_cat(path, ent->d_name, buf);
                JB_TRY_IO(err, "failed to concatenate paths");

                bool em;
                JB_TRY(delete_empty(buf, &em));

                if (!em) empty = false;
            } break;
        }
    }

    if (empty) {
        jb_warn("deleting empty directory '%s'", path);
        rmdir(path);
    }

    *e = empty;

    closedir(dir);

    return JB_OK_VAL;
}

jb_res_t db_gc(db_t *db) {
    jb_info("collecting garbage");
    (void)db;
    bool e;
    JB_TRY(delete_empty(db->path, &e));

    if (e)
        jb_warn("folders deleted");
    else
        jb_warn("folders not deleted");

    return JB_OK_VAL;
}

static void ls_glob(db_t *db, void *state, note_entry_t *note) {
    (void)db;

    const char *glob = (char *)state;

    if (fnmatch(glob, note->path, FNM_EXTMATCH) == 0) {
        jb_debug("match success; glob = '%s', path = '%s'", glob, note->path);
        fprintf(stdout, "%s\n", note->path);
    } else {
        jb_debug("match failed; glob = '%s', path = '%s'", glob, note->path);
    }
}

static void rm_glob(db_t *db, void *state, note_entry_t *note) {
    const char *glob = (char *)state;
    char buf[PATH_MAX];

    if (fnmatch(glob, note->path, FNM_EXTMATCH) == 0) {
        jb_debug("match success; glob = '%s', path = '%s'", glob, note->path);

        jb_errno_t err = jb_path_cat(db->path, note->path, buf);
        if (err) {
            jb_error("failed to get filesystem path for note '%s': %s", note->path, strerror(err));
            return;
        }

        if (remove(buf) == -1) {
            err = errno;
            jb_error("faled to delete note '%s': %s", note->path, strerror(err));
        }
    } else {
        jb_debug("match failed; glob = '%s', path = '%s'", glob, note->path);
    }
}

void db_ls(db_t *db, const char *glob, db_tag_t *filter, size_t filter_len) {
    jb_debug("pattern: %s", glob);
    db_query(db, (void *)glob, filter, filter_len, ls_glob);
}

void db_rm(db_t *db, const char *glob, db_tag_t *filter, size_t filter_len) {
    jb_debug("pattern: %s", glob);
    db_query(db, (void *)glob, filter, filter_len, rm_glob);
    db_gc(db);
}
