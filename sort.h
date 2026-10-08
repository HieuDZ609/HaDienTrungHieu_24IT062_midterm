/*
 * sort.h -- thứ tự của các mục đã thu thập.
 */

#ifndef LS_SORT_H
#define LS_SORT_H

#include "ls_types.h"

/*
 * Sắp xếp list theo opt. Mặc định theo từ điển trên tên; -t lấy timestamp
 * làm khóa chính còn tên là khóa phụ, -S lấy size làm khóa chính và giữ
 * thứ tự đọc cho size bằng nhau, -f giữ nguyên thứ tự readdir(). -r đảo
 * ngược kết quả, kể cả các phần bằng nhau.
 */
void ls_sort_entries(ls_list_t *list, const ls_options_t *opt);

#endif /* LS_SORT_H */