/*
 * options.h -- command line parsing for ls(1).
 */

#ifndef LS_OPTIONS_H
#define LS_OPTIONS_H

#include "ls_types.h"

/* The exact option string from the manual's SYNOPSIS. */
#define LS_OPTSTRING "AacdFfhiklnqRrSstuw"

/*
 * Fill opt with the defaults described in the manual (one entry per line,
 * lexicographical order, modification time, 512 byte blocks, ...).
 */
void ls_options_init(ls_options_t *opt);

/*
 * Consume the option letters of argv.  Returns optind, i.e. the index of the
 * first operand.  An unknown option is reported on stderr and terminates the
 * program with a non-zero status, as the EXIT STATUS section requires.
 */
int ls_parse_args(int argc, char **argv, ls_options_t *opt);

/*
 * Apply the two rules that cannot be decided while parsing:
 *   - -q/-w default to '?' on a terminal and to raw output elsewhere;
 *   - -A is always in effect for the super-user.
 */
void ls_options_finish(ls_options_t *opt);

/* Print the usage line on stderr. */
void ls_usage(void);

#endif /* LS_OPTIONS_H */
