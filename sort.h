/*
 * sort.h -- ordering of the collected entries.
 */

#ifndef LS_SORT_H
#define LS_SORT_H

#include "ls_types.h"

/*
 * Sort list in place according to opt.  The default order is lexicographical
 * on the name; -t makes the timestamp primary with the name as the tie
 * breaker, -S makes the size primary and keeps the input order for equal
 * sizes, and -f leaves the readdir() order alone.  -r reverses whatever
 * order was produced, equal keys included.
 */
void ls_sort_entries(ls_list_t *list, const ls_options_t *opt);

#endif /* LS_SORT_H */
