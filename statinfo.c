/*
 * statinfo.c -- turning struct stat into the strings ls(1) prints.
 *
 * Everything that needs a system call (getpwuid, getgrgid, readlink, the
 * BLOCKSIZE environment variable, localtime) is isolated here so the output
 * module can stay purely concerned with layout.
 */

#include "statinfo.h"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__linux__)
#include <sys/sysmacros.h> /* major(), minor() */
#endif

/* The whiteout type is not part of POSIX; overlayfs still creates it. */
#ifndef S_IFWHT
#define S_IFWHT 0160000
#endif

/* Fallback unit when BLOCKSIZE is absent or unusable. */
#define LS_DEFAULT_BLOCKSIZE 512ULL

/*
 * Parse BLOCKSIZE once.  The manual allows a plain count or a k/m/g suffix;
 * anything we cannot understand falls back to the documented default.
 */
static unsigned long long ls_parse_blocksize(void)
{
    const char *raw = getenv("BLOCKSIZE");
    char *end = NULL;
    unsigned long long value;

    if (raw == NULL || *raw == '\0')
        return LS_DEFAULT_BLOCKSIZE;

    value = strtoull(raw, &end, 10);
    if (end == raw)
        return LS_DEFAULT_BLOCKSIZE;

    switch (*end) {
    case 'k':
    case 'K':
        value *= 1024ULL;
        break;
    case 'm':
    case 'M':
        value *= 1024ULL * 1024ULL;
        break;
    case 'g':
    case 'G':
        value *= 1024ULL * 1024ULL * 1024ULL;
        break;
    case '\0':
        break;
    default:
        return LS_DEFAULT_BLOCKSIZE;
    }

    return value == 0 ? LS_DEFAULT_BLOCKSIZE : value;
}

unsigned long long ls_total_unit(void)
{
    static unsigned long long unit; /* 0 == not read yet */

    if (unit == 0)
        unit = ls_parse_blocksize();

    return unit;
}

bool ls_is_whiteout(mode_t mode)
{
    return (mode & S_IFMT) == S_IFWHT;
}

char ls_type_char(mode_t mode)
{
    if (S_ISREG(mode))
        return '-';
    if (S_ISDIR(mode))
        return 'd';
    if (S_ISLNK(mode))
        return 'l';
    if (S_ISCHR(mode))
        return 'c';
    if (S_ISBLK(mode))
        return 'b';
    if (S_ISSOCK(mode))
        return 's';
    if (S_ISFIFO(mode))
        return 'p';
    if (ls_is_whiteout(mode))
        return 'w';
    return '?';
}

char *ls_mode_string(mode_t mode, char *buf)
{
    buf[0] = ls_type_char(mode);

    /* Owner permissions, including the set-user-ID bit. */
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    if (mode & S_ISUID)
        buf[3] = (mode & S_IXUSR) ? 's' : 'S';
    else
        buf[3] = (mode & S_IXUSR) ? 'x' : '-';

    /* Group permissions, including the set-group-ID bit. */
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    if (mode & S_ISGID)
        buf[6] = (mode & S_IXGRP) ? 's' : 'S';
    else
        buf[6] = (mode & S_IXGRP) ? 'x' : '-';

    /* Other permissions, including the sticky bit ('T' / 't'). */
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    if (mode & S_ISVTX)
        buf[9] = (mode & S_IXOTH) ? 't' : 'T';
    else
        buf[9] = (mode & S_IXOTH) ? 'x' : '-';

    buf[10] = '\0';
    return buf;
}

/*
 * Render "sizes ... in a human readable format" (-h): 512, 1.0K, 12M, 2.5G.
 *
 * The arithmetic is done entirely in integers so that no precision is lost
 * on large values and no rounding can drift below the requested precision:
 * the number is scaled with an exact ceiling division and then, below ten,
 * shown with one decimal digit rounded up.  1025 becomes 1.1K rather than
 * 1.0K, and 3076096 bytes becomes 3.0M rather than 2.9M.
 */
static void ls_humanize(unsigned long long bytes, char *buf, size_t buflen)
{
    static const unsigned long long scale[] = {
        1ULL,
        1024ULL,
        1024ULL * 1024ULL,
        1024ULL * 1024ULL * 1024ULL,
        1024ULL * 1024ULL * 1024ULL * 1024ULL,
        1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL,
        1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL
    };
    static const char units[] = { 'B', 'K', 'M', 'G', 'T', 'P', 'E' };
    const size_t unit_count = sizeof(units);
    size_t u = 0;
    unsigned long long whole;
    unsigned long long rest;
    unsigned long long tenths;

    while (u + 1 < unit_count && bytes >= scale[u + 1])
        u++;

    if (u == 0) {
        snprintf(buf, buflen, "%llu", bytes);
        return;
    }

    whole = bytes / scale[u];
    rest = bytes % scale[u];

    if (whole >= 10) {
        /* "1024K", "11M": a whole unit is rounded up when anything is left. */
        snprintf(buf, buflen, "%llu%c", whole + (rest != 0), units[u]);
        return;
    }

    tenths = whole * 10 + (rest * 10 + scale[u] - 1) / scale[u];
    if (tenths >= 100)
        snprintf(buf, buflen, "%llu%c", tenths / 10, units[u]);
    else
        snprintf(buf, buflen, "%llu.%llu%c", tenths / 10, tenths % 10,
                 units[u]);
}

/*
 * Scale a byte count the way the current -h/-k/BLOCKSIZE setting asks for.
 * This is the single place that knows the three renderings, so the -s column
 * and the "total N" line can never disagree.
 */
static void ls_render_bytes(unsigned long long bytes, const ls_options_t *opt,
                            char *buf, size_t buflen)
{
    switch (opt->size_mode) {
    case SIZE_HUMAN:
        ls_humanize(bytes, buf, buflen);
        break;
    case SIZE_KILOBYTES:
        snprintf(buf, buflen, "%llu", (bytes + 1023ULL) / 1024ULL);
        break;
    case SIZE_DEFAULT:
    default:
        snprintf(buf, buflen, "%llu",
                 (bytes + ls_total_unit() - 1ULL) / ls_total_unit());
        break;
    }
}

void ls_format_byte_count(unsigned long long bytes, const ls_options_t *opt,
                          char *buf, size_t buflen)
{
    ls_render_bytes(bytes, opt, buf, buflen);
}

void ls_format_blocks(const struct stat *st, const ls_options_t *opt,
                      char *buf, size_t buflen)
{
    /* st_blocks is counted in 512 byte units on every POSIX platform. */
    ls_render_bytes((unsigned long long)st->st_blocks * 512ULL, opt, buf,
                    buflen);
}

void ls_format_size(const struct stat *st, const ls_options_t *opt,
                    char *buf, size_t buflen)
{
    /*
     * "If the file is a character special or block special file, the major
     * and minor device numbers for the file are displayed in the size field."
     */
    if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
        snprintf(buf, buflen, "%llu, %llu",
                 (unsigned long long)major(st->st_rdev),
                 (unsigned long long)minor(st->st_rdev));
        return;
    }

    if (opt->size_mode == SIZE_HUMAN)
        ls_humanize((unsigned long long)st->st_size, buf, buflen);
    else
        snprintf(buf, buflen, "%llu", (unsigned long long)st->st_size);
}

/* Three way comparison of two (second, nanosecond) stamps. */
static int ls_timespec_cmp(time_t asec, long ansec, time_t bsec, long bnsec)
{
    if (asec != bsec)
        return (asec < bsec) ? -1 : 1;
    if (ansec != bnsec)
        return (ansec < bnsec) ? -1 : 1;
    return 0;
}

void ls_format_time(const struct stat *st, const ls_options_t *opt,
                    char *buf, size_t buflen)
{
    struct timespec now;
    struct timespec when;
    struct tm result;
    const char *fmt;

    switch (opt->time_kind) {
    case TIME_ATIME:
        when = st->st_atim;
        break;
    case TIME_CTIME:
        when = st->st_ctim;
        break;
    case TIME_MTIME:
    default:
        when = st->st_mtim;
        break;
    }

    /*
     * A file younger than six months shows the time of day, an older one
     * shows the year.  The cut off is half of 31556952, the number of
     * seconds in a Gregorian year, exactly as the reference implementation
     * computes it, and the comparison carries the nanoseconds as well: an
     * entry whose stamp was refreshed a moment ago is still "recent", and a
     * whole second comparison would push it into the other branch.  A file
     * dated in the future is not recent either.
     */
    clock_gettime(CLOCK_REALTIME, &now);
    fmt = (ls_timespec_cmp(now.tv_sec - 31556952 / 2, now.tv_nsec,
                           when.tv_sec, when.tv_nsec) < 0 &&
           ls_timespec_cmp(when.tv_sec, when.tv_nsec, now.tv_sec,
                           now.tv_nsec) < 0)
              ? "%b %e %H:%M"
              : "%b %e  %Y";

    if (localtime_r(&when.tv_sec, &result) == NULL ||
        strftime(buf, buflen, fmt, &result) == 0)
        snprintf(buf, buflen, "?");
}

void ls_owner_name(const struct stat *st, const ls_options_t *opt,
                   char *buf, size_t buflen)
{
    struct passwd *pw;

    if (opt->format != FORMAT_LONG_NUMERIC) {
        pw = getpwuid(st->st_uid);
        if (pw != NULL) {
            snprintf(buf, buflen, "%s", pw->pw_name);
            return;
        }
    }

    /* "-n ... owner and group IDs are displayed numerically" and unknown
     * owners fall back to the numeric id as well. */
    snprintf(buf, buflen, "%u", (unsigned int)st->st_uid);
}

void ls_group_name(const struct stat *st, const ls_options_t *opt,
                   char *buf, size_t buflen)
{
    struct group *gr;

    if (opt->format != FORMAT_LONG_NUMERIC) {
        gr = getgrgid(st->st_gid);
        if (gr != NULL) {
            snprintf(buf, buflen, "%s", gr->gr_name);
            return;
        }
    }

    snprintf(buf, buflen, "%u", (unsigned int)st->st_gid);
}

char ls_suffix_char(const struct stat *st)
{
    mode_t mode = st->st_mode;

    if (S_ISDIR(mode))
        return '/';
    if (S_ISLNK(mode))
        return '@';
    if (S_ISSOCK(mode))
        return '=';
    if (S_ISFIFO(mode))
        return '|';
    if (ls_is_whiteout(mode))
        return '%';
    if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
        return '*';

    return 0;
}
