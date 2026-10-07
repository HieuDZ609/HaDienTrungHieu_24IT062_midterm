/*
 * ls_types.h -- shared data types for the simplified ls(1) implementation.
 *
 * Every option group that the manual declares to "override each other" is
 * stored in exactly one field.  Because getopt() reports the options in the
 * order they appear on the command line, the "last one specified wins" rule
 * falls out of the assignment order for free.
 */

#ifndef LS_TYPES_H
#define LS_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Name reported in diagnostics and in the usage message. */
#define LS_PROGNAME "myls"

/*
 * Status used whenever something failed.  The manual only requires a value
 * greater than zero; 2 is what the system ls(1) returns, so a differential
 * test can compare the exit status as well as the output.
 */
#define LS_EXIT_FAILURE 2

/* Which timestamp -t (sorting) and -l (printing) should use. */
typedef enum {
    TIME_MTIME = 0, /* default: time of last modification */
    TIME_ATIME,     /* -u: time of last access           */
    TIME_CTIME      /* -c: time when status was changed  */
} ls_time_kind_t;

/* Output layout.  -l and -n override each other. */
typedef enum {
    FORMAT_SHORT = 0,   /* default: one name per line  */
    FORMAT_LONG,        /* -l                          */
    FORMAT_LONG_NUMERIC /* -n: -l with numeric uid/gid */
} ls_format_t;

/* How directory operands are handled.  -R and -d override each other. */
typedef enum {
    LIST_NORMAL = 0, /* descend into directory operands            */
    LIST_RECURSE,    /* -R: also descend into every sub-directory  */
    LIST_FLAT        /* -d: print directories as if they were files */
} ls_list_mode_t;

/* Rendering of non-printable bytes.  -q and -w override each other. */
typedef enum {
    RAW_QUESTION = 0, /* -q: show them as '?' (default on a tty) */
    RAW_LITERAL       /* -w: emit them verbatim (default off a tty) */
} ls_raw_mode_t;

/* Size rendering.  The rightmost of -h and -k wins. */
typedef enum {
    SIZE_DEFAULT = 0, /* plain units, see ls_total_unit()  */
    SIZE_HUMAN,       /* -h: 512, 1.0K, 2.5M, ...          */
    SIZE_KILOBYTES    /* -k: rounded up to kilobytes        */
} ls_size_mode_t;

/* Sorting strategy. */
typedef enum {
    SORT_LEX = 0, /* default: lexicographical */
    SORT_NONE,    /* -f: leave readdir order alone */
    SORT_TIME,    /* -t                          */
    SORT_SIZE     /* -S                          */
} ls_sort_kind_t;

/* Dot-file policy.  -a and -A override each other. */
typedef enum {
    DOT_HIDE = 0, /* default: skip every name starting with '.' */
    DOT_ALMOST,   /* -A: skip only '.' and '..'                 */
    DOT_ALL       /* -a: skip nothing                           */
} ls_dot_mode_t;

/* Fully resolved set of flags used by every other module. */
typedef struct {
    ls_dot_mode_t  dot_mode;
    ls_list_mode_t list_mode;
    ls_format_t    format;
    ls_time_kind_t time_kind;
    ls_size_mode_t size_mode;
    ls_sort_kind_t sort_kind;
    ls_raw_mode_t  raw_mode;
    bool           raw_forced;   /* true once -q or -w was given   */
    bool           print_inode;  /* -i */
    bool           print_blocks; /* -s */
    bool           suffix;       /* -F */
    bool           reverse;      /* -r */
} ls_options_t;

/* A directory entry together with the lstat() data collected for it. */
typedef struct {
    char       *name; /* name exactly as it will be printed  */
    char       *path; /* path handed to lstat()              */
    struct stat st;   /* valid only when stat_ok is true     */
    bool        stat_ok;
} ls_entry_t;

/* Growable array of ls_entry_t. */
typedef struct {
    ls_entry_t *items;
    size_t      len;
    size_t      cap;
} ls_list_t;

#endif /* LS_TYPES_H */
