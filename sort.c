/*
 * sort.c -- ordering of the collected entries.
 *
 * A plain top-down merge sort is used instead of qsort() so that the options
 * can be passed to the comparator directly, without resorting to a global
 * variable, and so that the result is stable.
 */

#include "sort.h"

#include <stdlib.h>
#include <string.h>

/*
 * Timestamp selected by -c / -u / -t.  The nanosecond part is kept so that
 * files touched within the same second still land in the right order, which
 * is what ls(1) does as well.
 */
static struct timespec ls_entry_time(const ls_entry_t *entry,
                                     ls_time_kind_t kind)
{
    struct timespec stamp = { 0, 0 };

    if (!entry->stat_ok)
        return stamp;

    switch (kind) {
    case TIME_ATIME:
        return entry->st.st_atim;
    case TIME_CTIME:
        return entry->st.st_ctim;
    case TIME_MTIME:
    default:
        return entry->st.st_mtim;
    }
}

/*
 * Three way comparison: negative when a should be printed first.
 *
 * -t sorts "most recently modified first" with the manual's lexicographical
 * order as the tie breaker, and -S sorts "largest file first" with no tie
 * breaker at all so that equal sizes keep the order they were read in.
 * -r is not handled here: it reverses the finished order, which is what
 * makes equal keys swap places too.
 */
static int ls_compare(const ls_entry_t *a, const ls_entry_t *b,
                      const ls_options_t *opt)
{
    int result = 0;

    switch (opt->sort_kind) {
    case SORT_TIME: {
        struct timespec ta = ls_entry_time(a, opt->time_kind);
        struct timespec tb = ls_entry_time(b, opt->time_kind);

        if (ta.tv_sec != tb.tv_sec)
            result = (ta.tv_sec > tb.tv_sec) ? -1 : 1;
        else if (ta.tv_nsec != tb.tv_nsec)
            result = (ta.tv_nsec > tb.tv_nsec) ? -1 : 1;
        else
            result = strcmp(a->name, b->name);
        break;
    }
    case SORT_SIZE:
        if (!a->stat_ok || !b->stat_ok)
            break;
        if (a->st.st_size > b->st.st_size)
            result = -1;
        else if (a->st.st_size < b->st.st_size)
            result = 1;
        break;
    case SORT_NONE:
    case SORT_LEX:
    default:
        result = strcmp(a->name, b->name);
        break;
    }

    return result;
}

static void ls_merge(ls_entry_t *left, size_t nleft, ls_entry_t *right,
                     size_t nright, ls_entry_t *target, const ls_options_t *opt)
{
    size_t i = 0;
    size_t j = 0;
    size_t k = 0;

    while (i < nleft && j < nright) {
        if (ls_compare(&left[i], &right[j], opt) <= 0)
            target[k++] = left[i++];
        else
            target[k++] = right[j++];
    }
    while (i < nleft)
        target[k++] = left[i++];
    while (j < nright)
        target[k++] = right[j++];
}

static void ls_merge_sort(ls_entry_t *entries, size_t count,
                          ls_entry_t *scratch, const ls_options_t *opt)
{
    size_t middle;

    if (count < 2)
        return;

    middle = count / 2;
    ls_merge_sort(entries, middle, scratch, opt);
    ls_merge_sort(entries + middle, count - middle, scratch, opt);

    ls_merge(entries, middle, entries + middle, count - middle, scratch, opt);
    memcpy(entries, scratch, count * sizeof(*entries));
}

void ls_sort_entries(ls_list_t *list, const ls_options_t *opt)
{
    ls_entry_t *scratch;
    size_t i;
    size_t j;

    if (list->len < 2)
        return;

    /* "-f  Output is not sorted."  Only the reversal below still applies. */
    if (opt->sort_kind != SORT_NONE) {
        scratch = malloc(list->len * sizeof(*scratch));
        if (scratch != NULL) {
            ls_merge_sort(list->items, list->len, scratch, opt);
            free(scratch);
        }
    }

    /*
     * -r reverses the order that was just produced rather than negating the
     * comparator, so entries that compare equal (two files of the same size
     * under -S) swap places instead of staying where they were.
     */
    if (opt->reverse) {
        for (i = 0, j = list->len; i + 1 < j; i++, j--) {
            ls_entry_t swap = list->items[i];

            list->items[i] = list->items[j - 1];
            list->items[j - 1] = swap;
        }
    }
}
