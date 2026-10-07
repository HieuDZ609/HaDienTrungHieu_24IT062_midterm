/*
 * listing.h -- reading directories and managing the entry collections.
 */

#ifndef LS_LISTING_H
#define LS_LISTING_H

#include "ls_types.h"

/* Entry collections. */
void ls_list_init(ls_list_t *list);
void ls_list_free(ls_list_t *list);

/*
 * Append one entry.  st may be NULL when stat_ok is false, which happens for
 * an entry that disappeared between readdir() and lstat(); the name is still
 * printed in that case.  Returns false only on allocation failure.
 */
bool ls_list_add(ls_list_t *list, const char *name, const char *path,
                 const struct stat *st, bool stat_ok);

/* Allocate "dir/name", tolerating a trailing slash or an empty dir. */
char *ls_join_path(const char *dir, const char *name);

/*
 * Dot-file policy: hidden names are skipped by default, '.' and '..' are
 * only kept with -a, and the super-user always gets -A behaviour.
 */
bool ls_name_visible(const char *name, const ls_options_t *opt);

/*
 * Fill out with every visible entry of dirpath, in readdir() order and with
 * lstat() data attached.  Returns 0 on success and 1 when the directory could
 * not be opened; the diagnostic has already been printed in that case, and
 * the caller must not emit a header for a listing that never happened.
 */
int ls_read_directory(const char *dirpath, const ls_options_t *opt,
                      ls_list_t *out);

/*
 * Take the program name from argv[0] (everything after the last slash).
 * Called once at start-up; diagnostics fall back to LS_PROGNAME when it is
 * never called or argv[0] is unusable.
 */
void ls_set_program_name(const char *argv0);
const char *ls_program_name(void);

/*
 * Print "myls: cannot <verb> '<path>': <strerror>" on stderr.  Any pending
 * output on stdout is flushed first so the diagnostic lands in the right
 * place when both streams are redirected to the same file.
 */
void ls_report_error(const char *verb, const char *path, int err);

#endif /* LS_LISTING_H */
