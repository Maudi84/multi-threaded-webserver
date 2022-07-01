OBJS = httpserver.o util.o io.o threadpool.o
SOURCE = httpserver.c util.c io.c threadpool.c
HEADER = util.h io.h threads.h
OUT = httpserver
CC = clang
CFLAGS = -g -c -Wall -Wextra -Werror -Wpedantic
LFLAGS = -pthread

all:	$(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS)

httpserver:	httpserver.o
	$(CC) -o httpserver httpserver.o util.o io.o threadpool.o -pthread

httpserver.o:	httpserver.c
	$(CC) $(CFLAGS) -c httpserver.c util.c io.c threadpool.c
clean:
	rm -rf httpserver httpserver.o util.o io.o threadpool.o
