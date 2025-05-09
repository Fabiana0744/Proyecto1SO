CC = gcc
CFLAGS = -Wall -g
INCLUDE = -Iinclude -Iexternal/inih

SRC = src/mypthreads.c src/scheduler.c src/scheduler_rr.c src/scheduler_lottery.c src/scheduler_realtime.c
ANIM_SRC = anim/parser.c
TEST = examples/test_threads.c
INI_SRC = external/inih/ini.c

all: test_threads

test_threads:
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) $(TEST) -o test_threads

test_mutex:
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) examples/test_mutex.c -o test_mutex

test_lottery:
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) examples/test_lottery.c -o test_lottery

parser:
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) $(ANIM_SRC) $(INI_SRC) -o parser

main_anim:
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) anim/main_anim.c anim/parser.c $(INI_SRC) -o main_anim -lncurses

server: src/server_anim.c $(SRC)
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) src/server_anim.c $(INI_SRC) -o server_anim -lpthread

client: src/client_anim.c
	$(CC) $(CFLAGS) $(INCLUDE) src/client_anim.c -o client_anim -lncurses

clean:
	rm -f test_threads test_mutex test_lottery parser main_anim server_anim client_anim