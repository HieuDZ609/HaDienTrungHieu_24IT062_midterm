/*
 * ls_types.h -- các kiểu dữ liệu dùng chung cho toàn bộ chương trình.
 * Mỗi nhóm option "đè lẫn nhau" chỉ lưu trong một field; vì getopt() trả về
 * option theo thứ tự trên dòng lệnh nên luật "cái cuối thắng" tự nhiên có được.
 */

#ifndef LS_TYPES_H
#define LS_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>

/* Tên dùng trong thông báo lỗi và usage. */
#define LS_PROGNAME "myls"

/*
 * Mã trả về khi có lỗi: man page chỉ đòi > 0; dùng 2 đúng như ls(1)
 * để khớp exit status khi so sánh với /bin/ls.
 */
#define LS_EXIT_FAILURE 2

/* Timestamp dùng cho -t (sắp xếp) và -l (in ra). */
typedef enum {
    TIME_MTIME = 0, /* mặc định: lần sửa cuối    */
    TIME_ATIME,     /* -u: lần truy cập cuối     */
    TIME_CTIME      /* -c: lần đổi trạng thái    */
} ls_time_kind_t;

/* Kiểu in: -l và -n đè lẫn nhau. */
typedef enum {
    FORMAT_SHORT = 0,   /* mặc định: mỗi dòng một tên */
    FORMAT_LONG,        /* -l                         */
    FORMAT_LONG_NUMERIC /* -n: -l với uid/gid dạng số */
} ls_format_t;

/* Cách xử lý operand là thư mục: -R và -d đè lẫn nhau. */
typedef enum {
    LIST_NORMAL = 0, /* duyệt vào thư mục operand                  */
    LIST_RECURSE,    /* -R: còn duyệt mọi thư mục con              */
    LIST_FLAT        /* -d: in thư mục như file thường             */
} ls_list_mode_t;

/* Byte không in được: -q và -w đè lẫn nhau. */
typedef enum {
    RAW_QUESTION = 0, /* -q: hiện '?' (mặc định khi stdout là tty) */
    RAW_LITERAL       /* -w: in nguyên byte (mặc định khi ống/redirect) */
} ls_raw_mode_t;

/* Cách hiện dung lượng: trong -h và -k, cái bên phải thắng. */
typedef enum {
    SIZE_DEFAULT = 0, /* đơn vị thường, xem ls_total_unit() */
    SIZE_HUMAN,       /* -h: 512, 1.0K, 2.5M, ...          */
    SIZE_KILOBYTES    /* -k: làm tròn lên tới kibibyte      */
} ls_size_mode_t;

/* Chiến lược sắp xếp. */
typedef enum {
    SORT_LEX = 0, /* mặc định: theo từ điển */
    SORT_NONE,    /* -f: giữ nguyên thứ tự readdir */
    SORT_TIME,    /* -t                          */
    SORT_SIZE     /* -S                          */
} ls_sort_kind_t;

/* Chính sách file dot: -a và -A đè lẫn nhau. */
typedef enum {
    DOT_HIDE = 0, /* mặc định: bỏ mọi tên bắt đầu '.' */
    DOT_ALMOST,   /* -A: chỉ bỏ '.' và '..'           */
    DOT_ALL       /* -a: giữ tất cả                   */
} ls_dot_mode_t;

/* Bộ option đã phân giải xong, mọi module khác dùng. */
typedef struct {
    ls_dot_mode_t  dot_mode;
    ls_list_mode_t list_mode;
    ls_format_t    format;
    ls_time_kind_t time_kind;
    ls_size_mode_t size_mode;
    ls_sort_kind_t sort_kind;
    ls_raw_mode_t  raw_mode;
    bool           raw_forced;   /* true khi người dùng đã ghi rõ -q hoặc -w */
    bool           print_inode;  /* -i */
    bool           print_blocks; /* -s */
    bool           suffix;       /* -F */
    bool           reverse;      /* -r */
} ls_options_t;

/* Một mục thư mục cùng dữ liệu lstat() thu được. */
typedef struct {
    char       *name; /* tên in ra, đúng như gốc      */
    char       *path; /* đường dẫn truyền cho lstat() */
    struct stat st;   /* chỉ hợp lệ khi stat_ok = true */
    bool        stat_ok;
} ls_entry_t;

/* Mảng động các ls_entry_t. */
typedef struct {
    ls_entry_t *items;
    size_t      len;
    size_t      cap;
} ls_list_t;

#endif /* LS_TYPES_H */