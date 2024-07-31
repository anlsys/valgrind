CC=clang
#CC=gcc

CFLAGS=-Wall -Werror -Wextra
CFLAGS+=-fopencilk
CFLAGS+=-O0 -g
CFLAGS+=-Wno-int-to-pointer-cast -Wno-unused-value

LDFLAGS=

SRC=$(shell ls *.c)
EXE=$(SRC:.c=.exe)

all: $(EXE)

%.exe: %.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@

clean:
	rm -rf $(EXE)

re: clean all

install:
