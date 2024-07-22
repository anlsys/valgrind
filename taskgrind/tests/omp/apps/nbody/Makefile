#CC=mpc_cc -fno-mpc-privatize
#CC=gcc
CC=clang
CFLAGS=-Wall -Werror -Wextra -I src/ -fopenmp -O3
LDFLAGS=-lm
SRC=src/main.c src/body.c

all: sequential parallel-for taskloop task

help:
	@echo "make [all|sequential|parallel-for|taskloop|task]"

sequential:
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) src/sequential/*.c -o sequential

parallel-for:
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) src/parallel-for/*.c -o parallel-for

taskloop:
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) src/taskloop/*.c -o taskloop

task:
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRC) src/task/*.c -o task

clean:
	@-rm -f sequential parallel-for taskloop task

.PHONY: sequential parallel-for taskloop taskwait task
