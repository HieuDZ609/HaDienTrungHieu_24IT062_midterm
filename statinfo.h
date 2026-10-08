/*
 * statinfo.h -- biến struct stat thành các chuỗi ls(1) in ra.
 */

#ifndef LS_STATINFO_H
#define LS_STATINFO_H

#include "ls_types.h"

/* Độ dài chuỗi kiểu "-rw-r--r--" kèm ký tự kết thúc NUL. */
#define LS_MODE_LEN 11

/*
 * Đơn vị dùng cho cột -s khi không có -h/-k và cho dòng "total N":
 * BLOCKSIZE trong môi trường, hoặc 512 byte khi không đặt (mặc định man page).
 */
unsigned long long ls_total_unit(void);

/* Ký tự loại file trong chuỗi mode ('-', 'd', 'l', 'c', 'b', ...). */
char ls_type_char(mode_t mode);

/* True cho mục whiteout (loại 'w'). */
bool ls_is_whiteout(mode_t mode);

/* Ghi 11 ký tự quyền vào buf (tối thiểu LS_MODE_LEN byte). Trả về buf. */
char *ls_mode_string(mode_t mode, char *buf);

/*
 * Cột -s in ra: số khối thực dùng của file, làm tròn theo đơn vị đã chọn
 * và dạng human readable khi có -h.
 */
void ls_format_blocks(const struct stat *st, const ls_options_t *opt,
                      char *buf, size_t buflen);

/*
 * In một lượng byte với quy tắc như ls_format_blocks(). Dòng "total N" dùng
 * nó để total và cột không bao giờ lệch về -h/-k/BLOCKSIZE.
 */
void ls_format_byte_count(unsigned long long bytes, const ls_options_t *opt,
                          char *buf, size_t buflen);

/* Size của -l, tôn trọng -h; file device in "major, minor". */
void ls_format_size(const struct stat *st, const ls_options_t *opt,
                    char *buf, size_t buflen);

/* Timestamp chọn bởi -c/-u/-t, định dạng "Mon Dd HH:MM". */
void ls_format_time(const struct stat *st, const ls_options_t *opt,
                    char *buf, size_t buflen);

/* Tên chủ sở hữu cho -l, hoặc số khi -n hoặc tên không biết. */
void ls_owner_name(const struct stat *st, const ls_options_t *opt,
                   char *buf, size_t buflen);

/* Tên nhóm cho -l, cùng quy tắc ls_owner_name(). */
void ls_group_name(const struct stat *st, const ls_options_t *opt,
                   char *buf, size_t buflen);

/*
 * Hậu tố gắn bởi -F: '/' thư mục, '*' thực thi, '@' symlink, '%' whiteout,
 * '=' socket, '|' FIFO. Trả về 0 khi không thêm gì.
 */
char ls_suffix_char(const struct stat *st);

#endif /* LS_STATINFO_H */