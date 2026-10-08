/*
 * listing.h -- đọc thư mục và quản lý các bộ sưu tập mục.
 */

#ifndef LS_LISTING_H
#define LS_LISTING_H

#include "ls_types.h"

/* Bộ sưu tập mục. */
void ls_list_init(ls_list_t *list);
void ls_list_free(ls_list_t *list);

/*
 * Thêm một mục. st có thể là NULL khi stat_ok = false (mục biến mất giữa
 * readdir() và lstat()): tên vẫn được in. Trả về false chỉ khi thiếu bộ nhớ.
 */
bool ls_list_add(ls_list_t *list, const char *name, const char *path,
                 const struct stat *st, bool stat_ok);

/* Cấp "dir/name", chấp nhận slash đuôi hoặc dir rỗng. */
char *ls_join_path(const char *dir, const char *name);

/*
 * Chính sách file dot: tên ẩn bị bỏ mặc định, '.'/'..' chỉ giữ với -a,
 * super-user luôn được hành vi -A.
 */
bool ls_name_visible(const char *name, const ls_options_t *opt);

/*
 * Điền out với mọi mục hiện của dirpath, theo thứ tự readdir() và kèm dữ
 * liệu lstat(). Trả về 0 khi ok, 1 khi không mở được thư mục (đã in chẩn
 * đoán; caller không được in header cho listing không xảy ra).
 */
int ls_read_directory(const char *dirpath, const ls_options_t *opt,
                      ls_list_t *out);

/*
 * Lấy tên chương trình từ argv[0] (phần sau dấu / cuối). Gọi một lần lúc
 * khởi động; khi chưa gọi, chẩn đoán dùng LS_PROGNAME.
 */
void ls_set_program_name(const char *argv0);
const char *ls_program_name(void);

/*
 * In "myls: cannot <verb> '<path>': <strerror>" ra stderr. stdout được flush
 * trước để thông báo nằm đúng chỗ khi hai luồng cùng về một file.
 */
void ls_report_error(const char *verb, const char *path, int err);

#endif /* LS_LISTING_H */