/**
 * @file scanner.h
 * @brief Data structures and public interface for the lexical analyzer
 * (scanner).
 *
 * This file defines the types for tokens (`TokenType`, `Token`) and the
 * `Scanner` state struct. It also declares the functions to initialize and run
 * the scanner.
 */

#ifndef corelox_scanner_h
#define corelox_scanner_h

#include "common.h"

// An enum of all possible token types in the Clox language.
typedef enum {
    // single-character tokens
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_MINUS,
    TOKEN_PLUS,
    TOKEN_SEMICOLON,
    TOKEN_SLASH,
    TOKEN_STAR,

    // one or two character tokens
    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,

    // literals
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,

    // keywords
    TOKEN_AND,
    TOKEN_CLASS,
    TOKEN_ELSE,
    TOKEN_FALSE,
    TOKEN_FOR,
    TOKEN_FUN,
    TOKEN_IF,
    TOKEN_NIL,
    TOKEN_OR,
    TOKEN_PRINT,
    TOKEN_RETURN,
    TOKEN_SUPER,
    TOKEN_THIS,
    TOKEN_TRUE,
    TOKEN_VAR,
    TOKEN_WHILE,
    TOKEN_ERROR,
    TOKEN_EOF,

    // synthetic token
    TOKEN_SYNTHETIC
} TokenType;

// Represents a single token scanned from the source code.
typedef struct {
    TokenType type;     ///< The type of the token.
    const char* start;  ///< A pointer to the beginning of the lexeme.
    int32_t length;     ///< The length of the lexeme.
    int32_t line;       ///< The source line number where the token appears.
} Token;

// The state of the scanner during lexical analysis.
typedef struct {
    const char* start;  ///< A pointer to the start of the current lexeme.
    const char*
        current;   ///< A pointer to the current character being processed.
    int32_t line;  ///< The current line number.
} Scanner;

/**
 * @brief Initializes the scanner with the source code to be tokenized.
 * @param source A null-terminated C string containing the Clox source code.
 */
void initScanner(const char* source);

/**
 * @brief Scans and returns the next token from the source code.
 *
 * This is the main public function of the scanner. It's called repeatedly by
 * the compiler to get the next token in the stream.
 *
 * @return Token The next token in the source code.
 */
Token scanToken();

#endif
