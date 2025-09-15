CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_GNU_SOURCE -Isrc/inc
LDFLAGS =

SRCS = \
    src/container_main.c \
    src/container_ns.c \
    src/container_rootfs.c \
    src/container_utils.c

OBJS = $(SRCS:.c=.o)
TARGET = mylinuxct

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)