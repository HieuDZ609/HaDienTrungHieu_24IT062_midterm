/*
 * statinfo.h -- turning struct stat into the strings ls(1) prints.
 */

#ifndef LS_STATINFO_H
#define LS_STATINFO_H

#include "ls_types.h"

/* Length of a mode string such as "-rw-r--r--" plus its NUL terminator. */
#define LS_MODE_LEN 11

/*
 * Unit used by the -s block counts when neither -h nor -k is in effect and
 * for the "total N" line: BLOCKSIZE from the environment, or 512 bytes when
 * BLOCKSIZE is unset (the manual's default).
 */
unsigned long long ls_total_unit(void);

/* Entry type character of the mode string ('-', 'd', 'l', 'c', 'b', ...). */
char ls_type_char(mode_t mode);

/* True for a whiteout entry (type 'w'). */
bool ls_is_whiteout(mode_t mode);

/*
 * Write the eleven character file mode into buf, which must hold at least
 * LS_MODE_LEN bytes.  Returns buf for convenience.
 */
char *ls_mode_string(mode_t mode, char *buf);

/*
 * Column that -s prints: the file system blocks actually used, rounded up to
 * the selected unit and rendered human readable when -h is in effect.
 */
void ls_format_blocks(const struct stat *st, const ls_options_t *opt,
                      char *buf, size_t buflen);

/*
 * Render an arbitrary byte count with the same rule as ls_format_blocks().
 * The "total N" line uses it so that the total and the column can never
 * disagree about -h, -k or BLOCKSIZE.
 */
void ls_format_byte_count(unsigned long long bytes, const ls_options_t *opt,
                          char *buf, size_t buflen);

/* Size shown by -l, honouring -h; device files use "major, minor" instead. */
void ls_format_size(const struct stat *st, const ls_options_t *opt,
                    char *buf, size_t buflen);

/* Timestamp selected by -c/-u/-t, formatted as "Mon Dd HH:MM". */
void ls_format_time(const struct stat *st, const ls_options_t *opt,
                    char *buf, size_t buflen);

/* Owner name for -l, or the numeric id when -n is used or the name is unknown. */
void ls_owner_name(const struct stat *st, const ls_options_t *opt,
                   char *buf, size_t buflen);

/* Group name for -l, following the same rule as ls_owner_name(). */
void ls_group_name(const struct stat *st, const ls_options_t *opt,
                   char *buf, size_t buflen);

/*
 * Suffix appended by -F: '/' directory, '*' executable, '@' symbolic link,
 * '%' whiteout, '=' socket, '|' FIFO.  Returns 0 when nothing is appended.
 */
char ls_suffix_char(const struct stat *st);

#endif /* LS_STATINFO_H */
