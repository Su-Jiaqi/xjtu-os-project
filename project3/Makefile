CC=gcc
CFLAGS=-O2 -Wall -Iinclude
SRCS=src/dev.c src/bitmap.c src/inode.c src/dir.c src/file.c src/fs.c src/util.c src/cli.c
OBJS=$(SRCS:.c=.o)
BIN=mini_ext2

$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c include/fs.h include/layout.h include/errors.h include/util.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BIN) disk.img

.PHONY: clean
