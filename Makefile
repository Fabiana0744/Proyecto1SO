CC = gcc
CFLAGS = -Wall -g
INCLUDE = -Iinclude

SRC = src/mypthreads.c src/scheduler.c src/scheduler_rr.c src/scheduler_lottery.c
ANIM_SRC = anim/parser.c
TEST = examples/test_threads.c

all: test_threads

test_threads:
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) $(TEST) -o test_threads

test_mutex:
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) examples/test_mutex.c -o test_mutex

test_lottery:
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) examples/test_lottery.c -o test_lottery

parser:
	$(CC) $(CFLAGS) $(INCLUDE) $(ANIM_SRC) -o parser

clean:
	rm -f test_threads test_mutex test_lottery parser
