/*
 * format.h -- laying out the entries on stdout.
 */

#ifndef LS_FORMAT_H
#define LS_FORMAT_H

#include "ls_types.h"

/*
 * Print every entry of list.
 *
 * is_dir_listing tells the routine that the entries are the contents of a
 * directory, which is what triggers the "total N" line: always for -l/-n,
 * and for -s only when the output goes to a terminal.
 */
void ls_print_entries(const ls_list_t *list, const ls_options_t *opt,
                      bool is_dir_listing);

#endif /* LS_FORMAT_H */
