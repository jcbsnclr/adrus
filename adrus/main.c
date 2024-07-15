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

#include "sys/stat.h"
#define _XOPEN_SOURCE 500
#define _GNU_SOURCE

#include <cmdline.h>
#include <db.h>
#include <errno.h>
#include <ftw.h>
#include <jbase.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <util.h>

static void callback(db_t *db, void *state, note_entry_t *note) {
    (void)db;
    (void)state;
    // jb_trace("got note '%s'", note->path);
    fprintf(stdout, "%s\n", note->path);
}

// static jb_res_t test_impl(db_t *db, cmdline_t *cmd) {
//     jb_errno_t err;
//     char path[PATH_MAX + 1];
//     char dname[PATH_MAX + 1];
//     char bname[PATH_MAX + 1];
//     char res[PATH_MAX + 1];

//     JB_ASSERT_PTR(NULL);

//     JB_TRY(path_cat(db->path, cmd->path, path));

//     err = jb_dirname(path, dname, 3);
//     JB_TRY_IO(err, "dirname exceeds buffer size");
//     err = jb_basename(path, bname, PATH_MAX);
//     JB_TRY_IO(err, "basname exceeds buffer size");

//     jb_info("dirname: %s, basename: %s", dname, bname);

//     err = jb_path_cat(dname, bname, res);
//     JB_TRY_IO(err, "failed to concat dname with bname");

//     jb_info("res: %s", res);

//     err = jb_path_cat("foo/", "bar", res);
//     JB_TRY_IO(err, "failed to concat paths");
//     jb_info("res: %s", res);

//     return JB_OK_VAL;
// }

static jb_errno_t make_parent(const char *path) {
    jb_errno_t err;
    char parent[PATH_MAX];

    // get parent dir of path
    err = jb_dirname(path, parent, PATH_MAX);
    if (err) return err;

    // if parent dir is empty (0 length), exit with success
    if (!parent[0]) return 0;

    // attempt to recursively make parent directory
    err = make_parent(parent);
    if (err) return err;

    if (mkdir(parent, S_IRWXU) == -1) {
        if (errno == EEXIST)
            return 0;  // parent directory exists; exit with success
        else
            return errno;  // failed to create parent directory; exit with error
    }
    jb_debug("mkdir %s", parent);

    // success
    return 0;
}

int main(int argc, char *argv[]) {
    (void)callback;
    int code = 0;
    jb_errno_t err;

    jb_res_t res;
    jb_log_init();

    db_t db;
    res = db_init(&db);
    if (res JB_IS_ERR) {
        jb_report_result(res);
        return 1;
    }

    cmdline_t cmd;
    res = cmdline_parse(&cmd, &db, argc, argv);
    if (res JB_IS_ERR) {
        jb_report_result(res);
        return 1;
    }

    char path[PATH_MAX];
    memset(path, 0, PATH_MAX);
    switch (cmd.cmd) {
        case CMD_QUERY: {
            jb_info("querying notebook");
            db_query(&db, NULL, cmd.tags, cmd.len, callback);
        } break;

        case CMD_LS: {
            db_ls(&db, cmd.path, cmd.tags, cmd.len);
        } break;

        case CMD_RM: {
            db_rm(&db, cmd.path, cmd.tags, cmd.len);
        } break;

        case CMD_OPEN: {
            char *editor = getenv("EDITOR");

            if (!editor) {
                jb_error("$EDITOR unset");

                code = 1;
                goto cleanup;
            }

            jb_debug("editor = %s", editor);

            jb_res_t res = path_cat(db.path, cmd.path, path);
            if (res JB_IS_ERR) {
                jb_error("path exceeds PATH_MAX: %s%s", db.path, cmd.path);
                code = 1;
                goto cleanup;
            }

            jb_info("path: %s", path);

            err = make_parent(path);
            if (err) {
                jb_error(
                    "failed to create parent directories for note '%s': %s", path, strerror(err));
                code = 1;
                goto cleanup;
            }

            err = jb_fstat(path, NULL);

            if (err == ENOENT) {
                FILE *f = fopen(path, "w");

                if (!f) {
                    jb_error("failed to open note '%s': %s", cmd.path, strerror(errno));
                    code = 1;
                    goto cleanup;
                }

                fseek(f, 0, SEEK_END);
                int len = ftell(f);

                if (len == -1) {
                    jb_error("failed to open note '%s': %s", cmd.path, strerror(errno));
                    code = 1;
                    goto cleanup;
                }
                rewind(f);

                if (len == 0) {
                    const char *hdr = "adrus\n";
                    fwrite(hdr, 1, strlen(hdr), f);
                }

                fclose(f);
            } else if (err) {
                jb_error("failed to check file '%s': %s", path, strerror(err));
            }

            jb_info("spawning editor");
            pid_t pid = fork();

            if (pid == 0) {
                execlp(editor, editor, path, NULL);
                jb_error("failed to spawn editor '%s': %s", editor, strerror(errno));
                return 1;
            } else if (pid > 1) {
                int status;
                jb_info("waiting on editor");

                waitpid(pid, &status, 0);

                if (status != 0)
                    jb_error("editor exited with status %d", status);
                else
                    jb_info("editor exited successfully");
            } else {
                jb_error("failed to spawn editor: %s", strerror(errno));
                code = 1;
                goto cleanup;
            }

        } break;

        case CMD_MODIFY: {
            // jb_res_t res = path_cat(db.path, cmd.path, path);
            // if (res JB_IS_ERR) {
            //     jb_error("path exceeds PATH_MAX: %s%s", db.path, cmd.path);
            //     code = 1;
            //     goto cleanup;
            // }

            jb_info("path: %s", cmd.path);

            res = db_mutate(&db, cmd.path, cmd.tags, cmd.len);
            if (res JB_IS_ERR) {
                jb_report_result(res);
                code = 1;
                goto cleanup;
            }
        } break;
    }

cleanup:
    db_free(&db);
    return code;
}
