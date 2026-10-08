/*
 * statinfo.c -- biến struct stat thành các chuỗi ls(1) in ra.
 * Mọi thứ cần system call (getpwuid, getgrgid, readlink, BLOCKSIZE,
 * localtime) được gom ở đây để module in chỉ lo dàn trang.
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

/* Kiểu whiteout không thuộc POSIX; overlayfs vẫn có thể tạo nó. */
#ifndef S_IFWHT
#define S_IFWHT 0160000
#endif

/* Đơn vị thay thế khi BLOCKSIZE vắng mặt hoặc không dùng được. */
#define LS_DEFAULT_BLOCKSIZE 512ULL

/* Đọc BLOCKSIZE một lần: man page cho phép số trần hoặc hậu tố k/m/g;
 * thứ không hiểu được thì quay về mặc định đã ghi. */
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
    static unsigned long long unit; /* 0 == chưa đọc */

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

    /* Quyền của chủ, kèm bit set-user-ID. */
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    if (mode & S_ISUID)
        buf[3] = (mode & S_IXUSR) ? 's' : 'S';
    else
        buf[3] = (mode & S_IXUSR) ? 'x' : '-';

    /* Quyền của nhóm, kèm bit set-group-ID. */
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    if (mode & S_ISGID)
        buf[6] = (mode & S_IXGRP) ? 's' : 'S';
    else
        buf[6] = (mode & S_IXGRP) ? 'x' : '-';

    /* Quyền của người khác, kèm sticky bit ('T' / 't'). */
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
 * Hiện khối lượng "theo dạng người đọc" (-h): 512, 1.0K, 12M, 2.5G.
 * Làm toàn bộ bằng số nguyên để không mất độ chính xác: chia thang đo với
 * phép chia trần chính xác rồi, dưới 10, hiện một chữ số thập phân làm tròn
 * lên. 1025 → 1.1K thay vì 1.0K; 3076096 byte → 3.0M thay vì 2.9M.
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
        /* "1024K", "11M": còn dư là làm tròn lên cả đơn vị. */
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
 * Co đếm byte theo yêu cầu -h/-k/BLOCKSIZE hiện tại. Đây là nơi duy nhất
 * biết cả ba cách hiện, nên cột -s và dòng "total N" không bao giờ lệch nhau.
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
    /* st_blocks tính theo đơn vị 512 byte trên mọi nền POSIX. */
    ls_render_bytes((unsigned long long)st->st_blocks * 512ULL, opt, buf,
                    buflen);
}

void ls_format_size(const struct stat *st, const ls_options_t *opt,
                    char *buf, size_t buflen)
{
    /*
     * "Nếu là character special hay block special, in major và minor trong
     * cột size."
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

/* So sánh 3 chiều hai mốc (giây, nano). */
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
     * File trẻ hơn sáu tháng hiện giờ trong ngày, già hơn hiện năm. Ngưỡng
     * cắt là 31556952/2 (nửa số giây của một năm Gregorian), đúng như bản
     * tham chiếu tính, và so sánh kèm cả nano: mốc vừa làm mới vẫn "gần đây",
     * so ở độ chính xác giây sẽ đẩy sang nhánh kia. File định ngày tương lai
     * cũng không coi là gần đây.
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

    /* "-n ... hiện ID của chủ và nhóm dạng số" và chủ không tìm được cũng
     * quay về hiện số. */
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
