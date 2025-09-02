CC = gcc
CFLAGS = -Wall -Wextra -O3
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
BIN = program.exe

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	del /Q $(OBJ) $(BIN) 2>nul || true

.PHONY: all clean