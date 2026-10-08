/*
 * options.c -- phân tích dòng lệnh cho ls(1) đơn giản hóa.
 *
 * Mỗi nhóm option đè lẫn nhau (-l/-n, -c/-u, -R/-d, -q/-w, -k/-h) lưu trong
 * một field, nên thứ tự trái-phải của getopt() đã đúng luật "cái cuối thắng".
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
    opt->raw_mode  = RAW_QUESTION; /* tạm, _finish() quyết định sau */
}

int ls_parse_args(int argc, char **argv, ls_options_t *opt)
{
    int ch;

    /* Chẩn đoán tự in để ghi đúng tên chương trình. */
    opterr = 0;

    while ((ch = getopt(argc, argv, LS_OPTSTRING)) != -1) {
        switch (ch) {
        /* Chính sách file dot: -a và -A dùng chung dot_mode. */
        case 'a':
            opt->dot_mode = DOT_ALL;
            break;
        case 'A':
            opt->dot_mode = DOT_ALMOST;
            break;

        /* Chọn timestamp: -c và -u dùng chung time_kind. */
        case 'c':
            opt->time_kind = TIME_CTIME;
            break;
        case 'u':
            opt->time_kind = TIME_ATIME;
            break;

        /* Xử lý thư mục: -R và -d dùng chung list_mode. */
        case 'R':
            opt->list_mode = LIST_RECURSE;
            break;
        case 'd':
            opt->list_mode = LIST_FLAT;
            break;

        /* Hiện dung lượng: -h và -k dùng chung size_mode (bên phải thắng). */
        case 'h':
            opt->size_mode = SIZE_HUMAN;
            break;
        case 'k':
            opt->size_mode = SIZE_KILOBYTES;
            break;

        /* Kiểu in: -l và -n dùng chung format. */
        case 'l':
            opt->format = FORMAT_LONG;
            break;
        case 'n':
            opt->format = FORMAT_LONG_NUMERIC;
            break;

        /* Byte không in được: -q và -w dùng chung raw_mode. */
        case 'q':
            opt->raw_mode    = RAW_QUESTION;
            opt->raw_forced  = true;
            break;
        case 'w':
            opt->raw_mode    = RAW_LITERAL;
            opt->raw_forced  = true;
            break;

        /* Các cờ độc lập. */
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
         * Chiến lược sắp xếp: -f, -t, -S dùng chung sort_kind. -f còn kéo
         * theo -a, nên cái nằm phải nhất trong -f/-A/-a quyết định.
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
     * -q mặc định khi output ra terminal, -w mặc định khi không phải
     * terminal. Hai option đè lẫn nhau: chỉ lựa chọn tường minh khi parse
     * mới giữ được, còn lại áp mặc định.
     */
    if (!opt->raw_forced)
        opt->raw_mode = isatty(STDOUT_FILENO) ? RAW_QUESTION : RAW_LITERAL;

    /* "Luôn đặt cho super-user." -a tường minh vẫn thắng vì nâng dot_mode
     * lên DOT_ALL. */
    if (geteuid() == 0 && opt->dot_mode == DOT_HIDE)
        opt->dot_mode = DOT_ALMOST;
}

void ls_usage(void)
{
    fprintf(stderr, "usage: %s [-%s] [file ...]\n", ls_program_name(),
            LS_OPTSTRING);
}
