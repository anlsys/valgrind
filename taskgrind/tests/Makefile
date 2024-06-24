CC=clang
#CC=gcc

CFLAGS=-Wall -Werror -Wextra
CFLAGS+=-fopenmp
CFLAGS+=-I ../
CFLAGS+=-O0 -g
CFLAGS+=-Wno-int-to-pointer-cast -Wno-unused-value

LDFLAGS=

SRC=$(wildcard *.c)
EXE=$(SRC:.c=.exe)

all: $(EXE)

%.exe: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm -rf $(EXE)

re: clean all

install:
