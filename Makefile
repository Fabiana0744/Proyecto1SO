CC = gcc
CFLAGS = -Wall -g
INCLUDE = -Iinclude
SRC = src/mypthreads.c src/scheduler.c src/scheduler_rr.c
TEST = examples/test_threads.c

all: test_threads

test_threads:
	$(CC) $(CFLAGS) $(INCLUDE) $(SRC) $(TEST) -o test_threads

clean:
	rm -f test_threads
