/*
 * main.c -- orchestration for the simplified ls(1).
 *
 * The flow follows the DESCRIPTION section of the manual:
 *
 *   1. parse the options,
 *   2. split the operands into non-directories and directories,
 *   3. print the non-directory operands first, sorted on their own,
 *   4. print every directory operand, sorted on its own, and with -R walk
 *      each operand's tree depth first before moving to the next operand.
 *
 * The depth first walk runs off an explicit stack rather than through the C
 * call stack, so a pathological directory tree cannot overflow it.
 */

#include "format.h"
#include "listing.h"
#include "options.h"
#include "sort.h"
#include "statinfo.h"

#include <dirent.h>
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* State shared by all directory listings of a single run. */
typedef struct {
    const ls_options_t *opt;
    bool headers; /* print "path:" in front of each listing */
    bool printed; /* at least one line has already been written */
    int  status;  /* 0 = success, 1 = minor problem, 2 = failure */
} ls_run_t;

/*
 * Report a failure and remember how bad it was.  A command line operand that
 * could not be used is a real failure (status 2); a directory that only went
 * missing half way through the walk is a minor problem (status 1).  The
 * worst status seen wins, except that a minor problem never downgrades an
 * already serious one.
 */
static void ls_run_fail(ls_run_t *run, bool serious)
{
    if (serious)
        run->status = 2;
    else if (run->status == 0)
        run->status = 1;
}

/* One entry of the -R work queue: the path plus where it came from. */
typedef struct {
    char *path;
    bool  from_operand;
} ls_queue_item_t;

typedef struct {
    ls_queue_item_t *items;
    size_t           len;
    size_t           cap;
} ls_queue_t;

static void ls_queue_init(ls_queue_t *queue)
{
    queue->items = NULL;
    queue->len = 0;
    queue->cap = 0;
}

static bool ls_queue_push(ls_queue_t *queue, const char *path,
                          bool from_operand)
{
    ls_queue_item_t *entry;
    char *copy;

    if (queue->len == queue->cap) {
        size_t want = queue->cap == 0 ? 8 : queue->cap * 2;
        ls_queue_item_t *grown =
            realloc(queue->items, want * sizeof(*grown));

        if (grown == NULL)
            return false;
        queue->items = grown;
        queue->cap = want;
    }

    copy = strdup(path);
    if (copy == NULL)
        return false;

    entry = &queue->items[queue->len++];
    entry->path = copy;
    entry->from_operand = from_operand;
    return true;
}

static void ls_queue_free(ls_queue_t *queue)
{
    size_t i;

    for (i = 0; i < queue->len; i++)
        free(queue->items[i].path);
    free(queue->items);
    ls_queue_init(queue);
}

/*
 * Print one directory: read it, announce it, lay it out and queue its
 * sub-directories.  The directory is opened before the header is written,
 * because a listing that could not be opened must not be announced.
 */
static void ls_show_directory(ls_run_t *run, ls_queue_t *queue,
                              const char *dirpath, bool from_operand)
{
    ls_list_t list;
    size_t i;

    ls_list_init(&list);

    if (ls_read_directory(dirpath, run->opt, &list) != 0) {
        ls_list_free(&list);
        ls_run_fail(run, from_operand);
        return;
    }

    if (run->headers) {
        if (run->printed)
            putchar('\n');
        printf("%s:\n", dirpath);
    }
    run->printed = true;

    ls_sort_entries(&list, run->opt);
    ls_print_entries(&list, run->opt, true);

    if (run->opt->list_mode == LIST_RECURSE) {
        /*
         * The sub-directories are opened once before anything descends into
         * them, walking the listing backwards, so that every "cannot open
         * directory" diagnostic is emitted right after the parent listing
         * rather than in the middle of the children.  Doing the walk in
         * reverse lets the successful probes be queued at the same time:
         * the stack pops them in listing order afterwards.  "." and ".."
         * are never queued, otherwise a listing that shows them would
         * descend into itself forever, and a symbolic link is never probed
         * because only a real directory can be opened.
         */
        for (i = list.len; i-- > 0;) {
            const ls_entry_t *entry = &list.items[i];
            DIR *probe;

            if (!entry->stat_ok || !S_ISDIR(entry->st.st_mode))
                continue;
            if (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0)
                continue;

            probe = opendir(entry->path);
            if (probe == NULL) {
                ls_report_error("open directory", entry->path, errno);
                ls_run_fail(run, false);
                continue;
            }
            closedir(probe);

            if (!ls_queue_push(queue, entry->path, false))
                ls_run_fail(run, true);
        }
    }

    ls_list_free(&list);
}

/*
 * Decide whether an operand names a directory we should descend into.  With
 * -d the operand is always printed as a plain file and symbolic links in the
 * argument list are never indirected through.
 */
/*
 * A symbolic link named on the command line is indirected through only when
 * the listing is neither long (-l/-n), nor a plain directory listing (-d),
 * nor typed (-F); otherwise the link itself is what gets printed.  NetBSD's
 * ls states this in a comment of its own ("If not -F, -d or -l options,
 * follow any symbolic links listed on the command line") and GNU ls derives
 * the same condition from `format == long_format || indicator_style ==
 * classify || immediate_dirs'.
 */
static bool ls_follow_operand(const ls_options_t *opt)
{
    return opt->format == FORMAT_SHORT && opt->list_mode != LIST_FLAT &&
           !opt->suffix;
}

static void ls_classify_operand(const char *path, const ls_options_t *opt,
                                ls_list_t *files, ls_list_t *dirs,
                                ls_run_t *run)
{
    struct stat st;

    if (opt->list_mode == LIST_FLAT) {
        if (lstat(path, &st) != 0) {
            ls_report_error("access", path, errno);
            ls_run_fail(run, true);
            return;
        }
        if (!ls_list_add(files, path, path, &st, true))
            ls_run_fail(run, true);
        return;
    }

    /*
     * Follow the link only when it really leads to a directory.  A link to a
     * regular file is printed as a link (with its own inode and block count
     * for -i and -s), and a dangling link falls through to the lstat() below
     * so that it is listed rather than reported as an error.
     */
    if (ls_follow_operand(opt) && stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            if (!ls_list_add(dirs, path, path, &st, true))
                ls_run_fail(run, true);
            return;
        }
    }

    if (lstat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            if (!ls_list_add(dirs, path, path, &st, true))
                ls_run_fail(run, true);
            return;
        }
        if (!ls_list_add(files, path, path, &st, true))
            ls_run_fail(run, true);
        return;
    }

    ls_report_error("access", path, errno);
    ls_run_fail(run, true);
}

int main(int argc, char **argv)
{
    ls_options_t opt;
    ls_list_t files;
    ls_list_t dirs;
    ls_queue_t queue;
    ls_run_t run = { NULL, false, false, 0 };
    int first_operand;
    int operand_count;
    int i;

    /* Diagnostics name the program the way argv[0] does. */
    ls_set_program_name(argc > 0 ? argv[0] : NULL);

    /* Dates, month names and the TZ variable have to be honoured. */
    setlocale(LC_ALL, "");

    ls_options_init(&opt);
    first_operand = ls_parse_args(argc, argv, &opt);
    ls_options_finish(&opt);

    operand_count = argc - first_operand;

    ls_list_init(&files);
    ls_list_init(&dirs);

    if (operand_count == 0) {
        /*
         * "If no operands are given, the contents of the current directory
         * are displayed."  With -d the current directory itself is the entry.
         */
        if (opt.list_mode == LIST_FLAT) {
            struct stat st;

            if (lstat(".", &st) == 0) {
                if (!ls_list_add(&files, ".", ".", &st, true))
                    ls_run_fail(&run, true);
            } else {
                ls_report_error("access", ".", errno);
                ls_run_fail(&run, true);
            }
        } else {
            struct stat st;

            if (stat(".", &st) == 0 && S_ISDIR(st.st_mode)) {
                if (!ls_list_add(&dirs, ".", ".", &st, true))
                    ls_run_fail(&run, true);
            } else {
                ls_report_error("access", ".", ENOTDIR);
                ls_run_fail(&run, true);
            }
        }
    } else {
        for (i = 0; i < operand_count; i++) {
            ls_classify_operand(argv[first_operand + i], &opt, &files, &dirs,
                                &run);
        }
    }

    /*
     * "Non-directory operands are displayed first; directory and non-directory
     * operands are sorted separately."  Both groups take part in the selected
     * sort, so -t and -S order the operands just as they order the entries
     * inside a directory.
     */
    if (files.len > 0) {
        ls_sort_entries(&files, &opt);
        ls_print_entries(&files, &opt, false);
        run.printed = true;
    }
    ls_sort_entries(&dirs, &opt);

    /*
     * A header is only needed when more than one operand was given, or when
     * -R produces several listings.  Otherwise the single listing is printed
     * bare, which is the default mode of ls(1).
     */
    run.headers = (operand_count > 1) || (opt.list_mode == LIST_RECURSE);
    run.opt = &opt;

    /*
     * The walk uses an explicit stack: operands go on first, in reverse, and
     * every listing pushes its own sub-directories on top.  That yields a
     * depth first traversal which finishes one operand before the next.
     */
    ls_queue_init(&queue);
    for (i = (int)dirs.len; i-- > 0;) {
        if (!ls_queue_push(&queue, dirs.items[i].path, true))
            ls_run_fail(&run, true);
    }

    while (queue.len > 0) {
        ls_queue_item_t item = queue.items[--queue.len];

        ls_show_directory(&run, &queue, item.path, item.from_operand);
        free(item.path);
    }

    ls_list_free(&files);
    ls_list_free(&dirs);
    ls_queue_free(&queue);

    return run.status;
}
