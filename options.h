/*
 * options.h -- phân tích dòng lệnh cho ls(1).
 */

#ifndef LS_OPTIONS_H
#define LS_OPTIONS_H

#include "ls_types.h"

/* Đúng chuỗi option trong SYNOPSIS của man page. */
#define LS_OPTSTRING "AacdFfhiklnqRrSstuw"

/* Điền các mặc định trong man page (mỗi dòng một tên, theo từ điển, ...). */
void ls_options_init(ls_options_t *opt);

/*
 * Tiêu thụ các chữ option của argv, trả về optind (chỉ số operand đầu tiên).
 * Option lạ được báo ra stderr và kết thúc chương trình với mã khác 0,
 * đúng như mục EXIT STATUS.
 */
int ls_parse_args(int argc, char **argv, ls_options_t *opt);

/*
 * Hai luật không quyết định được lúc parse:
 *   - -q/-w mặc định '?' khi là terminal, raw khi không phải;
 *   - -A luôn có hiệu lực với super-user.
 */
void ls_options_finish(ls_options_t *opt);

/* In dòng usage ra stderr. */
void ls_usage(void);

#endif /* LS_OPTIONS_H */