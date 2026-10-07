/*
 * options.c -- command line parsing for the simplified ls(1).
 *
 * Every option group that the manual marks as mutually overriding (-l/-n,
 * -c/-u, -R/-d, -q/-w and -k/-h) is stored in a single field, so the natural
 * left-to-right order of getopt() already implements "the last one specified
 * determines the format used".
 */

#include "options.h"

#include "listing.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void ls_options_init(ls_options_t *opt)
{
    memset(opt, 0, sizeof(*opt));

    opt->dot_mode  = DOT_HIDE;
    opt->list_mode = LIST_NORMAL;
    opt->format    = FORMAT_SHORT;
    opt->time_kind = TIME_MTIME;
    opt->size_mode = SIZE_DEFAULT;
    opt->sort_kind = SORT_LEX;
    opt->raw_mode  = RAW_QUESTION; /* provisional, finalised by _finish() */
}

int ls_parse_args(int argc, char **argv, ls_options_t *opt)
{
    int ch;

    /* Diagnostics are written by hand so that they carry our program name. */
    opterr = 0;

    while ((ch = getopt(argc, argv, LS_OPTSTRING)) != -1) {
        switch (ch) {
        /* Dot-file policy: -a and -A share dot_mode. */
        case 'a':
            opt->dot_mode = DOT_ALL;
            break;
        case 'A':
            opt->dot_mode = DOT_ALMOST;
            break;

        /* Timestamp selection: -c and -u share time_kind. */
        case 'c':
            opt->time_kind = TIME_CTIME;
            break;
        case 'u':
            opt->time_kind = TIME_ATIME;
            break;

        /* Directory handling: -R and -d share list_mode. */
        case 'R':
            opt->list_mode = LIST_RECURSE;
            break;
        case 'd':
            opt->list_mode = LIST_FLAT;
            break;

        /* Size rendering: -h and -k share size_mode (rightmost wins). */
        case 'h':
            opt->size_mode = SIZE_HUMAN;
            break;
        case 'k':
            opt->size_mode = SIZE_KILOBYTES;
            break;

        /* Output layout: -l and -n share format. */
        case 'l':
            opt->format = FORMAT_LONG;
            break;
        case 'n':
            opt->format = FORMAT_LONG_NUMERIC;
            break;

        /* Non-printable bytes: -q and -w share raw_mode. */
        case 'q':
            opt->raw_mode    = RAW_QUESTION;
            opt->raw_forced  = true;
            break;
        case 'w':
            opt->raw_mode    = RAW_LITERAL;
            opt->raw_forced  = true;
            break;

        /* Independent flags. */
        case 'F':
            opt->suffix = true;
            break;
        case 'i':
            opt->print_inode = true;
            break;
        case 's':
            opt->print_blocks = true;
            break;
        case 'r':
            opt->reverse = true;
            break;

        /*
         * Sorting strategy: -f, -t and -S share sort_kind.
         * "-f Output is not sorted" also implies -a in every ls(1) that
         * implements it, so it moves dot_mode just like -a does and the
         * rightmost of -f/-A/-a therefore wins.
         */
        case 'f':
            opt->sort_kind = SORT_NONE;
            opt->dot_mode   = DOT_ALL;
            break;
        case 't':
            opt->sort_kind = SORT_TIME;
            break;
        case 'S':
            opt->sort_kind = SORT_SIZE;
            break;

        case '?':
        default:
            if (optopt != 0)
                fprintf(stderr, "%s: invalid option -- '%c'\n",
                        ls_program_name(), optopt);
            else
                fprintf(stderr, "%s: invalid option\n", ls_program_name());
            ls_usage();
            exit(LS_EXIT_FAILURE);
        }
    }

    return optind;
}

void ls_options_finish(ls_options_t *opt)
{
    /*
     * "-q ... is the default when output is to a terminal" and
     * "-w ... is the default when output is not to a terminal".
     * The two options override each other, so only an explicit choice made
     * during parsing keeps the default from being applied.
     */
    if (!opt->raw_forced)
        opt->raw_mode = isatty(STDOUT_FILENO) ? RAW_QUESTION : RAW_LITERAL;

    /*
     * "Always set for the super-user."  An explicit -a still wins because it
     * moves dot_mode past DOT_HIDE.
     */
    if (geteuid() == 0 && opt->dot_mode == DOT_HIDE)
        opt->dot_mode = DOT_ALMOST;
}

void ls_usage(void)
{
    fprintf(stderr, "usage: %s [-%s] [file ...]\n", ls_program_name(),
            LS_OPTSTRING);
}
