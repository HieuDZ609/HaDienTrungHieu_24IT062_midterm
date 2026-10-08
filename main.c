/*
 * main.c -- điều phối cho ls(1) đơn giản hóa.
 *
 * Luồng đi theo mục DESCRIPTION của man page:
 *
 *   1. parse option,
 *   2. tách operand thành không-thư-mục và thư-mục,
 *   3. in operand không-thư-mục trước, tự sắp xếp riêng,
 *   4. in từng operand thư-mục, tự sắp riêng; với -R đi sâu từng cây của
 *      mỗi operand theo chiều sâu trước khi sang operand kế.
 *
 * Cây đi theo stack tường minh thay vì call stack của C, nên cây thư mục
 * bệnh lý đến đâu cũng không thể tràn.
 */

#include "format.h"
#include "listing.h"
#include "options.h"
#include "sort.h"
#include "statinfo.h"

#include <dirent.h>
#include <errno.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Trạng thái dùng chung cho mọi listing thư mục trong một lần chạy. */
typedef struct {
    const ls_options_t *opt;
    bool headers; /* in "path:" trước mỗi listing         */
    bool printed; /* đã có ít nhất một dòng được viết ra    */
    int  status;  /* 0 = thành công, 1 = lỗi nhỏ, 2 = lỗi   */
} ls_run_t;

/*
 * Báo một lỗi và ghi nhận mức tồi tệ nhất. Operand trái lệnh dùng không
 * được là lỗi thật (status 2); thư mục biến mất giữa chừng khi duyệt là
 * lỗi nhỏ (1). Mức tồi nhất thắng, ngoại trừ lỗi nhỏ không hạ cấp lỗi
 * đã là nghiêm trọng.
 */
static void ls_run_fail(ls_run_t *run, bool serious)
{
    if (serious)
        run->status = 2;
    else if (run->status == 0)
        run->status = 1;
}

/* Một mục của hàng đợi -R: đường dẫn và nơi nó đến. */
typedef struct {
    char *path;
    bool  from_operand;
} ls_queue_item_t;

typedef struct {
    ls_queue_item_t *items;
    size_t           len;
    size_t           cap;
} ls_queue_t;

static void ls_queue_init(ls_queue_t *queue)
{
    queue->items = NULL;
    queue->len = 0;
    queue->cap = 0;
}

static bool ls_queue_push(ls_queue_t *queue, const char *path,
                          bool from_operand)
{
    ls_queue_item_t *entry;
    char *copy;

    if (queue->len == queue->cap) {
        size_t want = queue->cap == 0 ? 8 : queue->cap * 2;
        ls_queue_item_t *grown =
            realloc(queue->items, want * sizeof(*grown));

        if (grown == NULL)
            return false;
        queue->items = grown;
        queue->cap = want;
    }

    copy = strdup(path);
    if (copy == NULL)
        return false;

    entry = &queue->items[queue->len++];
    entry->path = copy;
    entry->from_operand = from_operand;
    return true;
}

static void ls_queue_free(ls_queue_t *queue)
{
    size_t i;

    for (i = 0; i < queue->len; i++)
        free(queue->items[i].path);
    free(queue->items);
    ls_queue_init(queue);
}

/*
 * In một thư mục: đọc nó, thông báo nó, dàn trang và xếp các thư mục con.
 * Thư mục được mở TRƯỚC khi viết header, vì listing không mở được thì
 * không được thông báo.
 */
static void ls_show_directory(ls_run_t *run, ls_queue_t *queue,
                              const char *dirpath, bool from_operand)
{
    ls_list_t list;
    size_t i;

    ls_list_init(&list);

    if (ls_read_directory(dirpath, run->opt, &list) != 0) {
        ls_list_free(&list);
        ls_run_fail(run, from_operand);
        return;
    }

    if (run->headers) {
        if (run->printed)
            putchar('\n');
        printf("%s:\n", dirpath);
    }
    run->printed = true;

    ls_sort_entries(&list, run->opt);
    ls_print_entries(&list, run->opt, true);

    if (run->opt->list_mode == LIST_RECURSE) {
        /*
         * Các thư mục con được mở thử một loạt trước khi đi sâu bất kỳ
         * thư mục nào, duyệt listing ngược, để mọi chẩn đoán "cannot open
         * directory" được in ngay sau listing cha chứ không lẫn vào giữa
         * các con. Duyệt ngược cho phép các thăm dò thành công được xếp
         * vào hàng đợi cùng lúc: stack lấy ra sau đó đúng thứ tự listing.
         * "." và ".." không bao giờ được xếp, nếu không một listing hiện
         * chúng sẽ tự đi sâu vào chính nó vô hạn; symlink không được thăm
         * dò vì chỉ thư mục thật mới mở được.
         */
        for (i = list.len; i-- > 0;) {
            const ls_entry_t *entry = &list.items[i];
            DIR *probe;

            if (!entry->stat_ok || !S_ISDIR(entry->st.st_mode))
                continue;
            if (strcmp(entry->name, ".") == 0 || strcmp(entry->name, "..") == 0)
                continue;

            probe = opendir(entry->path);
            if (probe == NULL) {
                ls_report_error("open directory", entry->path, errno);
                ls_run_fail(run, false);
                continue;
            }
            closedir(probe);

            if (!ls_queue_push(queue, entry->path, false))
                ls_run_fail(run, true);
        }
    }

    ls_list_free(&list);
}

/* Quyết định một operand có phải thư mục để đi xuống. Với -d operand luôn
 * được in như file thường, và symlink trong danh sách đối số không bao giờ
 * được thông qua.
 */
/*
 * Symlink được nêu trên dòng lệnh chỉ được "thông qua" khi listing không
 * dài (-l/-n), không phải thư mục trần (-d), và không gắn loại (-F); còn
 * lại thì in chính cái link. NetBSD ghi điều này trong comment riêng ("If
 * not -F, -d or -l options, follow any symbolic links listed on the command
 * line") và GNU suy cùng điều kiện từ `format == long_format ||
 * indicator_style == classify || immediate_dirs'.
 */
static bool ls_follow_operand(const ls_options_t *opt)
{
    return opt->format == FORMAT_SHORT && opt->list_mode != LIST_FLAT &&
           !opt->suffix;
}

static void ls_classify_operand(const char *path, const ls_options_t *opt,
                                ls_list_t *files, ls_list_t *dirs,
                                ls_run_t *run)
{
    struct stat st;

    if (opt->list_mode == LIST_FLAT) {
        if (lstat(path, &st) != 0) {
            ls_report_error("access", path, errno);
            ls_run_fail(run, true);
            return;
        }
        if (!ls_list_add(files, path, path, &st, true))
            ls_run_fail(run, true);
        return;
    }

    /*
     * Chỉ theo symlink khi nó thực sự dẫn tới thư mục. Link tới file thường
     * được in như link (với inode và số khối của chính nó cho -i và -s), và
     * link treo rơi xuống lstat() bên dưới để được liệt kê chứ không báo lỗi.
     */
    if (ls_follow_operand(opt) && stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            if (!ls_list_add(dirs, path, path, &st, true))
                ls_run_fail(run, true);
            return;
        }
    }

    if (lstat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            if (!ls_list_add(dirs, path, path, &st, true))
                ls_run_fail(run, true);
            return;
        }
        if (!ls_list_add(files, path, path, &st, true))
            ls_run_fail(run, true);
        return;
    }

    ls_report_error("access", path, errno);
    ls_run_fail(run, true);
}

int main(int argc, char **argv)
{
    ls_options_t opt;
    ls_list_t files;
    ls_list_t dirs;
    ls_queue_t queue;
    ls_run_t run = { NULL, false, false, 0 };
    int first_operand;
    int operand_count;
    int i;

    /* Chẩn đoán gọi tên chương trình theo đúng argv[0]. */
    ls_set_program_name(argc > 0 ? argv[0] : NULL);

    /* Phải tôn trọng locale (tên tháng, ngày) và biến TZ. */
    setlocale(LC_ALL, "");

    ls_options_init(&opt);
    first_operand = ls_parse_args(argc, argv, &opt);
    ls_options_finish(&opt);

    operand_count = argc - first_operand;

    ls_list_init(&files);
    ls_list_init(&dirs);

    if (operand_count == 0) {
        /*
         * "Nếu không có operand nào, hiện nội dung thư mục hiện tại."
         * Với -d chính thư mục hiện tại là mục cần in.
         */
        if (opt.list_mode == LIST_FLAT) {
            struct stat st;

            if (lstat(".", &st) == 0) {
                if (!ls_list_add(&files, ".", ".", &st, true))
                    ls_run_fail(&run, true);
            } else {
                ls_report_error("access", ".", errno);
                ls_run_fail(&run, true);
            }
        } else {
            struct stat st;

            if (stat(".", &st) == 0 && S_ISDIR(st.st_mode)) {
                if (!ls_list_add(&dirs, ".", ".", &st, true))
                    ls_run_fail(&run, true);
            } else {
                ls_report_error("access", ".", ENOTDIR);
                ls_run_fail(&run, true);
            }
        }
    } else {
        for (i = 0; i < operand_count; i++) {
            ls_classify_operand(argv[first_operand + i], &opt, &files, &dirs,
                                &run);
        }
    }

    /*
     * "Operand không-thư-mục in trước; operand thư-mục và không-thư-mục
     * sắp riêng." Cả hai nhóm đều tham gia cách sắp đã chọn, nên -t và -S
     * sắp xếp các operand hệt như sắp các mục trong thư mục.
     */
    if (files.len > 0) {
        ls_sort_entries(&files, &opt);
        ls_print_entries(&files, &opt, false);
        run.printed = true;
    }
    ls_sort_entries(&dirs, &opt);

    /* Chỉ cần header khi nhiều operand được đưa ra, hoặc -R sinh nhiều
     * listing. Còn lại in trần, là chế độ mặc định của ls(1). */
    run.headers = (operand_count > 1) || (opt.list_mode == LIST_RECURSE);
    run.opt = &opt;

    /*
     * Duyệt bằng stack tường minh: operand vào trước, theo thứ tự đảo, và
     * mỗi listing đẩy các thư mục con của nó lên trên. Nhờ đó có được duyệt
     * theo chiều sâu, xong một operand mới sang operand kế.
     */
    ls_queue_init(&queue);
    for (i = (int)dirs.len; i-- > 0;) {
        if (!ls_queue_push(&queue, dirs.items[i].path, true))
            ls_run_fail(&run, true);
    }

    while (queue.len > 0) {
        ls_queue_item_t item = queue.items[--queue.len];

        ls_show_directory(&run, &queue, item.path, item.from_operand);
        free(item.path);
    }

    ls_list_free(&files);
    ls_list_free(&dirs);
    ls_queue_free(&queue);

    return run.status;
}
