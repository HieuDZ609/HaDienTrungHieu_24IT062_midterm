# Makefile for the simplified ls(1) implementation.
#
#   make        build ./myls
#   make clean  remove every generated file

CC      := gcc
# -D_NETBSD_SOURCE: with _POSIX_C_SOURCE defined, NetBSD's <sys/types.h>
# otherwise hides major(), minor(), S_IFMT and S_ISVTX.  On glibc the macro
# is simply ignored, so the same flags build on both systems.
CFLAGS  := -std=c11 -Wall -Wextra -Werror -O2 -g -MMD -MP \
           -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -D_NETBSD_SOURCE
LDFLAGS :=

TARGET  := myls
OBJS    := main.o options.o listing.o statinfo.o sort.o format.o
DEPS    := $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS) $(DEPS)

# Header dependencies discovered by -MMD, so editing a .h rebuilds its users.
-include $(DEPS)
