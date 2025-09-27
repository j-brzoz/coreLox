/**
 * @file main.c
 * @brief Main entry point for the Clox interpreter.
 *
 * This file contains the main function that drives the interpreter. It handles
 * command-line arguments, reads source files, and kicks off the execution
 * by either running a script or starting a REPL session.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

static void repl();
static void runFile(const char* path);
static char* readFile(const char* path);

/**
 * @brief The main entry point of the Clox interpreter.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line argument strings.
 * @return int The exit code. Returns 0 on success.
 */
int main(int argc, const char* argv[]) {
    initVM();

    if (argc == 1) {
        // if no arguments are provided, start the REPL
        repl();
    } else if (argc == 2) {
        // if one argument (a file path) is provided, execute the file
        runFile(argv[1]);
    } else {
        // otherwise, show usage information and exit
        fprintf(stderr, "Usage: clox [path]\n");
        exit(64);  // exit code for incorrect command-line usage
    }

    freeVM();
    return 0;
}

/**
 * @brief Runs an interactive Read-Eval-Print Loop (REPL).
 *
 * Reads lines of code from standard input, interprets them, and prints the
 * result in a continuous loop until the user exits.
 */
static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

/**
 * @brief Executes a Clox script from a given file.
 *
 * @param path The path to the Clox source file.
 */
static void runFile(const char* path) {
    char* source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

/**
 * @brief Reads the entire content of a file into a dynamically allocated
 * string.
 *
 * The caller is responsible for freeing the returned buffer.
 * Exits the program if the file cannot be opened or read.
 *
 * @param path The path to the file.
 * @return char* A null-terminated string containing the file's content.
 */
static char* readFile(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);  // exit code for I/O errors
    }

    // seek to the end to determine the file size
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}