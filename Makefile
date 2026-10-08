# Makefile cho bản cài đặt ls(1) đơn giản hóa.
#
#   make        dựng ./myls
#   make clean  xóa mọi file đã sinh

CC      := gcc
# -D_NETBSD_SOURCE: khi đã khai báo _POSIX_C_SOURCE, NetBSD <sys/types.h>
# còn giấu major(), minor(), S_IFMT và S_ISVTX. Trên glibc macro này chỉ
# bị bỏ qua, nên cùng cờ này dựng được trên cả hai hệ.
CFLAGS  := -std=c11 -Wall -Wextra -Werror -O2 -g -MMD -MP \
           -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -D_NETBSD_SOURCE
LDFLAGS :=

TARGET  := myls
OBJS    := main.o options.o listing.o statinfo.o sort.o format.o
DEPS    := $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS) $(DEPS)

# Phụ thuộc header do -MMD sinh ra, nên sửa một .h sẽ dựng lại các nguồn
# dùng nó. (bmake trên NetBSD không có $^, nên rule link viết tường minh
# $(OBJS) thay vì $^.)
-include $(DEPS)
