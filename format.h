/*
 * format.h -- dàn mục in ra stdout.
 */

#ifndef LS_FORMAT_H
#define LS_FORMAT_H

#include "ls_types.h"

/*
 * In mọi mục của list.
 *
 * is_dir_listing = true nghĩa là các mục là nội dung một thư mục, kích hoạt
 * dòng "total N": luôn với -l/-n, và với -s chỉ khi output ra terminal.
 */
void ls_print_entries(const ls_list_t *list, const ls_options_t *opt,
                      bool is_dir_listing);

#endif /* LS_FORMAT_H */