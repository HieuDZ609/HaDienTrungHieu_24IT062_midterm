/*
 * format.c -- laying out the entries on stdout.
 *
 * Column widths are derived from the entries that are actually printed, so
 * the numeric fields of the long format line up without fixed guesswork.
 */

#include "format.h"

#include "statinfo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* Width of every optional column, measured over the current listing. */
typedef struct {
    unsigned int inode;
    unsigned int blocks;
    unsigned int links;
    unsigned int owner;
    unsigned int group;
    unsigned int size;
} ls_widths_t;

/* Number of decimal digits needed to print value. */
static unsigned int ls_digits(unsigned long long value)
{
    unsigned int digits = 1;

    while (value >= 10) {
        value /= 10;
        digits++;
    }
    return digits;
}

/* A byte is printable unless it is a C0 control character or DEL. */
static int ls_is_printable(unsigned char c)
{
    return c >= 0x20 && c != 0x7f;
}

/*
 * Emit a file name honouring -q and -w.  Bytes above 0x7f are passed through
 * so that UTF-8 names stay intact when the output is a terminal.
 */
static void ls_print_name(const char *name, ls_raw_mode_t raw)
{
    const unsigned char *p;

    if (raw == RAW_LITERAL) {
        fputs(name, stdout);
        return;
    }

    for (p = (const unsigned char *)name; *p != '\0'; p++)
        putchar(ls_is_printable(*p) ? *p : '?');
}

/* Read the target of a symbolic link, growing the buffer as needed. */
static char *ls_read_link(const char *path)
{
    size_t capacity = 128;

    for (;;) {
        char *buffer = malloc(capacity);
        ssize_t length;

        if (buffer == NULL)
            return NULL;

        length = readlink(path, buffer, capacity);
        if (length < 0) {
            free(buffer);
            return NULL;
        }
        if ((size_t)length < capacity) {
            buffer[length] = '\0';
            return buffer;
        }

        free(buffer);
        capacity *= 2;
    }
}

static void ls_compute_widths(const ls_list_t *list, const ls_options_t *opt,
                              ls_widths_t *widths)
{
    char buffer[64];
    size_t i;

    memset(widths, 0, sizeof(*widths));

    for (i = 0; i < list->len; i++) {
        const ls_entry_t *entry = &list->items[i];
        unsigned int digits;

        if (!entry->stat_ok)
            continue;

        if (opt->print_inode) {
            digits = ls_digits((unsigned long long)entry->st.st_ino);
            if (digits > widths->inode)
                widths->inode = digits;
        }

        if (opt->print_blocks) {
            ls_format_blocks(&entry->st, opt, buffer, sizeof(buffer));
            digits = (unsigned int)strlen(buffer);
            if (digits > widths->blocks)
                widths->blocks = digits;
        }

        if (opt->format == FORMAT_SHORT)
            continue;

        digits = ls_digits((unsigned long long)entry->st.st_nlink);
        if (digits > widths->links)
            widths->links = digits;

        ls_owner_name(&entry->st, opt, buffer, sizeof(buffer));
        digits = (unsigned int)strlen(buffer);
        if (digits > widths->owner)
            widths->owner = digits;

        ls_group_name(&entry->st, opt, buffer, sizeof(buffer));
        digits = (unsigned int)strlen(buffer);
        if (digits > widths->group)
            widths->group = digits;

        ls_format_size(&entry->st, opt, buffer, sizeof(buffer));
        digits = (unsigned int)strlen(buffer);
        if (digits > widths->size)
            widths->size = digits;
    }

    /* Never emit a zero width conversion; it would drop the column. */
    if (opt->print_inode && widths->inode == 0)
        widths->inode = 1;
    if (opt->print_blocks && widths->blocks == 0)
        widths->blocks = 1;
    if (opt->format != FORMAT_SHORT) {
        if (widths->links == 0)
            widths->links = 1;
        if (widths->owner == 0)
            widths->owner = 1;
        if (widths->group == 0)
            widths->group = 1;
        if (widths->size == 0)
            widths->size = 1;
    }
}

/* One row: [-i] [-s] [-l] name [-F suffix] [-l " -> target"]. */
static void ls_print_one(const ls_entry_t *entry, const ls_options_t *opt,
                         const ls_widths_t *widths, bool newline)
{
    bool have_stat = entry->stat_ok;
    bool long_format = (opt->format != FORMAT_SHORT) && have_stat;
    char buffer[256];

    if (opt->print_inode && have_stat)
        printf("%*llu ", (int)widths->inode,
               (unsigned long long)entry->st.st_ino);

    if (opt->print_blocks && have_stat) {
        ls_format_blocks(&entry->st, opt, buffer, sizeof(buffer));
        printf("%*s ", (int)widths->blocks, buffer);
    }

    if (long_format) {
        char mode[LS_MODE_LEN];
        char stamp[64];

        ls_mode_string(entry->st.st_mode, mode);
        ls_format_time(&entry->st, opt, stamp, sizeof(stamp));
        ls_owner_name(&entry->st, opt, buffer, sizeof(buffer));

        printf("%s %*llu %-*s ", mode, (int)widths->links,
               (unsigned long long)entry->st.st_nlink,
               (int)widths->owner, buffer);

        ls_group_name(&entry->st, opt, buffer, sizeof(buffer));
        printf("%-*s ", (int)widths->group, buffer);

        ls_format_size(&entry->st, opt, buffer, sizeof(buffer));
        printf("%*s %s ", (int)widths->size, buffer, stamp);
    }

    ls_print_name(entry->name, opt->raw_mode);

    if (opt->suffix && have_stat) {
        char suffix = ls_suffix_char(&entry->st);

        if (suffix != '\0')
            putchar(suffix);
    }

    /*
     * "If the file is a symbolic link the pathname of the linked-to file is
     * preceded by '->'."
     */
    if (long_format && S_ISLNK(entry->st.st_mode)) {
        char *target = ls_read_link(entry->path);

        if (target != NULL) {
            fputs(" -> ", stdout);
            ls_print_name(target, opt->raw_mode);
            free(target);
        }
    }

    if (newline)
        putchar('\n');
}

/* Width of one entry exactly as ls_print_one() writes it. */
static size_t ls_entry_width(const ls_entry_t *entry, const ls_options_t *opt,
                             const ls_widths_t *widths)
{
    size_t width = 0;

    if (entry->stat_ok) {
        if (opt->print_inode)
            width += widths->inode + 1;
        if (opt->print_blocks)
            width += widths->blocks + 1;
    }

    width += strlen(entry->name);

    if (opt->suffix && entry->stat_ok && ls_suffix_char(&entry->st) != '\0')
        width += 1;

    return width;
}

/* The width of the terminal the listing is being written to. */
static unsigned int ls_term_width(void)
{
    struct winsize ws;
    const char *env;
    char *end;
    long value;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return (unsigned int)ws.ws_col;

    env = getenv("COLUMNS");
    if (env != NULL && *env != '\0') {
        value = strtol(env, &end, 10);
        if (*end == '\0' && value > 0 && value < 100000)
            return (unsigned int)value;
    }

    return 80;
}

/*
 * A short listing written to a terminal: entries fill the columns from the
 * top down, every column is padded to the widest entry it holds, and two
 * spaces separate the columns.  The manual does not describe the layout, so
 * it follows ls(1) itself; anything that is not a terminal keeps the plain
 * one-entry-per-line listing above.
 */
static void ls_print_columns(const ls_list_t *list, const ls_options_t *opt,
                             const ls_widths_t *widths)
{
    size_t n = list->len;
    size_t *cellw;
    size_t *colw;
    size_t ncol = 1;
    size_t nrow;
    size_t ncol_try;
    size_t col;
    size_t row;
    size_t i;
    unsigned int screen;

    if (n == 0)
        return;

    cellw = malloc(n * sizeof(*cellw));
    if (cellw == NULL) {
        for (i = 0; i < n; i++)
            ls_print_one(&list->items[i], opt, widths, true);
        return;
    }

    for (i = 0; i < n; i++)
        cellw[i] = ls_entry_width(&list->items[i], opt, widths);

    screen = ls_term_width();

    /* Widen by one column at a time for as long as the listing fits. */
    for (ncol_try = 1; ncol_try <= n; ncol_try++) {
        size_t nrow_try = (n + ncol_try - 1) / ncol_try;
        size_t total = 0;

        for (col = 0; col < ncol_try; col++) {
            size_t first = col * nrow_try;
            size_t last = first + nrow_try;
            size_t width = 0;

            if (last > n)
                last = n;
            for (i = first; i < last; i++)
                if (cellw[i] > width)
                    width = cellw[i];
            total += width + 2;
        }
        if (total > screen)
            break;
        ncol = ncol_try;
    }

    nrow = (n + ncol - 1) / ncol;

    colw = malloc(ncol * sizeof(*colw));
    if (colw == NULL) {
        free(cellw);
        for (i = 0; i < n; i++)
            ls_print_one(&list->items[i], opt, widths, true);
        return;
    }

    for (col = 0; col < ncol; col++) {
        size_t first = col * nrow;
        size_t last = first + nrow;

        colw[col] = 0;
        if (last > n)
            last = n;
        for (i = first; i < last; i++)
            if (cellw[i] > colw[col])
                colw[col] = cellw[i];
    }

    for (row = 0; row < nrow; row++) {
        for (col = 0; col < ncol; col++) {
            size_t index = col * nrow + row;

            if (index >= n)
                continue;

            ls_print_one(&list->items[index], opt, widths, false);

            /* The last cell of a row is never padded. */
            if ((col + 1) * nrow + row < n) {
                size_t pad = colw[col] - cellw[index] + 2;

                while (pad-- > 0)
                    putchar(' ');
            }
        }
        putchar('\n');
    }

    free(colw);
    free(cellw);
}

/* The "total N" line that precedes a directory listing. */
static void ls_print_total(const ls_list_t *list, const ls_options_t *opt)
{
    unsigned long long bytes = 0;
    char buffer[64];
    size_t i;

    for (i = 0; i < list->len; i++) {
        if (list->items[i].stat_ok)
            bytes += (unsigned long long)list->items[i].st.st_blocks * 512ULL;
    }

    ls_format_byte_count(bytes, opt, buffer, sizeof(buffer));
    printf("total %s\n", buffer);
}

void ls_print_entries(const ls_list_t *list, const ls_options_t *opt,
                      bool is_dir_listing)
{
    ls_widths_t widths;
    bool want_total = false;
    size_t i;

    ls_compute_widths(list, opt, &widths);

    if (is_dir_listing) {
        /* -l always reports the block total of a directory it prints. */
        if (opt->format != FORMAT_SHORT)
            want_total = true;
        /* "-s ... If the output is to a terminal, a total sum ..." */
        else if (opt->print_blocks && isatty(STDOUT_FILENO))
            want_total = true;
    }

    if (want_total)
        ls_print_total(list, opt);

    if (opt->format == FORMAT_SHORT && isatty(STDOUT_FILENO)) {
        ls_print_columns(list, opt, &widths);
        return;
    }

    for (i = 0; i < list->len; i++)
        ls_print_one(&list->items[i], opt, &widths, true);
}
