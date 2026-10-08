/*
 * listing.c -- đọc thư mục và quản lý các bộ sưu tập mục.
 */

#include "listing.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Tên dùng trong chẩn đoán, lấy từ argv[0] như setprogname() của NetBSD và
 * program_invocation_short_name của GNU: binary tên "ls" báo "ls:", tên
 * "myls" báo "myls:".
 */
static const char *ls_progname = LS_PROGNAME;

void ls_set_program_name(const char *argv0)
{
    const char *slash;

    if (argv0 == NULL || *argv0 == '\0')
        return;

    slash = strrchr(argv0, '/');
    if (slash != NULL && slash[1] != '\0')
        ls_progname = slash + 1;
    else if (slash == NULL)
        ls_progname = argv0;
}

const char *ls_program_name(void)
{
    return ls_progname;
}

void ls_report_error(const char *verb, const char *path, int err)
{
    fflush(stdout);
    fprintf(stderr, "%s: cannot %s '%s': %s\n", ls_progname, verb, path,
            strerror(err));
}

void ls_list_init(ls_list_t *list)
{
    list->items = NULL;
    list->len = 0;
    list->cap = 0;
}

void ls_list_free(ls_list_t *list)
{
    size_t i;

    for (i = 0; i < list->len; i++) {
        free(list->items[i].name);
        free(list->items[i].path);
    }
    free(list->items);
    ls_list_init(list);
}

bool ls_list_add(ls_list_t *list, const char *name, const char *path,
                 const struct stat *st, bool stat_ok)
{
    ls_entry_t *entry;

    if (list->len == list->cap) {
        size_t want = list->cap == 0 ? 32 : list->cap * 2;
        ls_entry_t *grown = realloc(list->items, want * sizeof(*grown));

        if (grown == NULL)
            return false;
        list->items = grown;
        list->cap = want;
    }

    entry = &list->items[list->len];
    entry->name = strdup(name);
    entry->path = strdup(path);
    if (entry->name == NULL || entry->path == NULL) {
        free(entry->name);
        free(entry->path);
        entry->name = NULL;
        entry->path = NULL;
        return false;
    }

    if (stat_ok)
        entry->st = *st;
    else
        memset(&entry->st, 0, sizeof(entry->st));
    entry->stat_ok = stat_ok;

    list->len++;
    return true;
}

char *ls_join_path(const char *dir, const char *name)
{
    size_t dlen = strlen(dir);
    size_t nlen = strlen(name);
    int separator = (dlen > 0 && dir[dlen - 1] != '/') ? 1 : 0;
    char *joined;

    joined = malloc(dlen + (size_t)separator + nlen + 1);
    if (joined == NULL)
        return NULL;

    memcpy(joined, dir, dlen);
    if (separator)
        joined[dlen] = '/';
    memcpy(joined + dlen + separator, name, nlen + 1);

    return joined;
}

bool ls_name_visible(const char *name, const ls_options_t *opt)
{
    if (name[0] != '.')
        return true;

    switch (opt->dot_mode) {
    case DOT_ALL:
        return true;
    case DOT_ALMOST:
        return strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
    case DOT_HIDE:
    default:
        return false;
    }
}

/*
 * "-f  Output is in directory order (not sorted)." Kernel trả "." và ".."
 * ở vị trí tùy theo chỉ mục, nhưng ls(1) nào đem so cũng hiện chúng trước
 * rồi mới đến phần còn lại theo thứ tự readdir; đưa chúng lên đầu vì dù
 * sao cũng không có sắp xếp.
 */
static void ls_hoist_dots(ls_list_t *list)
{
    ls_entry_t dot;
    ls_entry_t dotdot;
    ls_entry_t *kept;
    size_t i;
    size_t n = 0;
    bool have_dot = false;
    bool have_dotdot = false;

    if (list->len < 2)
        return;

    kept = malloc(list->len * sizeof(*kept));
    if (kept == NULL)
        return;

    for (i = 0; i < list->len; i++) {
        if (strcmp(list->items[i].name, ".") == 0) {
            if (!have_dot) {
                dot = list->items[i];
                have_dot = true;
            }
        } else if (strcmp(list->items[i].name, "..") == 0) {
            if (!have_dotdot) {
                dotdot = list->items[i];
                have_dotdot = true;
            }
        } else {
            kept[n++] = list->items[i];
        }
    }

    if (!have_dot && !have_dotdot) {
        free(kept);
        return;
    }

    i = 0;
    if (have_dot)
        list->items[i++] = dot;
    if (have_dotdot)
        list->items[i++] = dotdot;
    memcpy(list->items + i, kept, n * sizeof(*kept));
    free(kept);
}

int ls_read_directory(const char *dirpath, const ls_options_t *opt,
                      ls_list_t *out)
{
    DIR *dir;
    struct dirent *de;
    int status = 0;

    dir = opendir(dirpath);
    if (dir == NULL) {
        ls_report_error("open directory", dirpath, errno);
        return 1;
    }

    for (;;) {
        struct stat st;
        char *path;
        bool have_stat;

        /* errno phải được xóa ở đây để NULL chỉ có thể là hết thư mục. */
        errno = 0;
        de = readdir(dir);
        if (de == NULL)
            break;

        if (!ls_name_visible(de->d_name, opt))
            continue;

        path = ls_join_path(dirpath, de->d_name);
        if (path == NULL)
            continue;

        have_stat = (lstat(path, &st) == 0);
        if (!ls_list_add(out, de->d_name, path, &st, have_stat))
            status = 1;

        free(path);
    }

    if (errno != 0) {
        ls_report_error("read directory", dirpath, errno);
        status = 1;
    }

    closedir(dir);

    if (opt->sort_kind == SORT_NONE)
        ls_hoist_dots(out);

    return status;
}
