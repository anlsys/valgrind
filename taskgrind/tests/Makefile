CC=clang

CFLAGS=-Wall -Werror -Wextra
CFLAGS+=-fopenmp

LDFLAGS=

TARGET=omp

SRC=omp.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) omp.c $(LDFLAGS) -o $(TARGET)

clean:
	rm $(TARGET)

install:
