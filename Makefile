CC = gcc
CFLAGS = -Wall -Wextra -O3 -I./src/include

SRC = $(wildcard src/*.c)
OBJ = $(patsubst src/%.c,bin/%.o,$(SRC))
BIN_DIR = bin
BIN = $(BIN_DIR)/corelox.exe

all: $(BIN)

$(BIN): $(OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(BIN) $(OBJ)

bin/%.o: src/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	@if not exist "$(BIN_DIR)" mkdir "$(BIN_DIR)"

clean:
	@if exist "$(BIN_DIR)" del /Q "$(BIN_DIR)\*.o" 2>nul
	@if exist "$(BIN_DIR)\corelox.exe" del /Q "$(BIN_DIR)\corelox.exe" 2>nul

lint:
	cppcheck --force --enable=all --inconclusive --std=c99 -Isrc/include \
		--suppress=missingIncludeSystem src

format:
	clang-format -i $(shell forfiles /S /M *.c /C "cmd /c echo @relpath") \
	    $(shell forfiles /S /M *.h /C "cmd /c echo @relpath")

.PHONY: all clean lint format
