CC=gcc

CFLAGS=-Wall -Wextra
CFLAGS+=-O0 -g
# tracegrind.h lives in the parent dir; it includes "valgrind.h" from include/
CFLAGS+=-I.. -I../../include
CFLAGS+=-Wno-unused-parameter

LDFLAGS=

SRC=$(wildcard *.c)
EXE=$(SRC:.c=.exe)

all: $(EXE)

%.exe: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm -f $(EXE)

re: clean all

install:
