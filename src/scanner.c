/**
 * @file scanner.c
 * @brief Lexical analyzer (scanner) for the Clox language.
 *
 * This component is responsible for taking a raw source code string and
 * converting it into a stream of tokens. It handles whitespace, comments,
 * literals (strings, numbers), and identifiers (including keywords).
 */

#include "scanner.h"

#include <stdio.h>
#include <string.h>

#include "common.h"

// global scanner
Scanner scanner;

/**
 * @brief Initializes the scanner with the source code to be tokenized.
 * @param source A null-terminated C string containing the Clox source code.
 */
void initScanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

//-----------------------------------------------------------------------------
//- Character Helpers
//-----------------------------------------------------------------------------

/**
 * @brief Checks if a character is an alphabet character or an underscore.
 * @param c The character to check.
 * @return bool True if the character is alphabetic or '_', false otherwise.
 */
static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/**
 * @brief Checks if a character is a digit ('0'-'9').
 * @param c The character to check.
 * @return bool True if the character is a digit, false otherwise.
 */
static bool isDigit(char c) { return c >= '0' && c <= '9'; }

/**
 * @brief Checks if the scanner has reached the end of the source code.
 * @return bool True if at the end of the file, false otherwise.
 */
static bool isAtEnd() { return *scanner.current == '\0'; }

//-----------------------------------------------------------------------------
//- Scanner Primitives
//-----------------------------------------------------------------------------

/**
 * @brief Consumes and returns the current character, advancing the scanner.
 * @return char The character that was just consumed.
 */
static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

/**
 * @brief Returns the character at the current scanner position without
 * consuming it.
 * @return char The current character.
 */
static char peek() { return *scanner.current; }

/**
 * @brief Returns the character just after the current scanner position without
 * consuming it.
 * @return char The next character, or '\0' if at the end.
 */
static char peekNext() {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

/**
 * @brief Conditionally consumes the current character if it matches the
 * expected one.
 * @param expected The character to match against.
 * @return bool True if the character was matched and consumed, false otherwise.
 */
static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

//-----------------------------------------------------------------------------
//- Token Creation
//-----------------------------------------------------------------------------

/**
 * @brief Creates a token of the given type.
 *
 * The token's lexeme is defined by the range from `scanner.start` to
 * `scanner.current`.
 * @param type The type of the token to create.
 * @return Token The newly created token.
 */
static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int32_t)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

/**
 * @brief Creates an error token with a specific message.
 * @param message The error message to embed in the token.
 * @return Token The newly created error token.
 */
static Token errorToken(const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;  // reuse for message passing
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

//-----------------------------------------------------------------------------
//- Lexical Analysis Rules
//-----------------------------------------------------------------------------

/**
 * @brief Skips over whitespace and comments.
 *
 * This includes spaces, tabs, carriage returns, newlines, and line comments
 * starting with '//'.
 */
static void skipWhitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {  // skip comments too
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

/**
 * @brief Scans a string literal.
 *
 * Consumes characters until a closing double quote is found or the end of
 * the file is reached.
 * @return Token A `TOKEN_STRING` or `TOKEN_ERROR` if the string is
 * unterminated.
 */
static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    // the closing quote
    advance();
    return makeToken(TOKEN_STRING);
}

/**
 * @brief Scans a number literal.
 *
 * Handles both integer and floating-point numbers.
 * @return Token A `TOKEN_NUMBER`.
 */
static Token number() {
    while (isDigit(peek())) advance();

    // look for a fractional part
    if (peek() == '.' && isDigit(peekNext())) {
        // consume the '.'
        advance();
        // consume the fractional part
        while (isDigit(peek())) advance();
    }

    return makeToken(TOKEN_NUMBER);
}

/**
 * @brief Helper for `identifierType` to check for a specific keyword.
 *
 * Compares a substring of the current lexeme against a potential keyword.
 *
 * @param start The starting offset within the lexeme to check.
 * @param length The length of the keyword to check.
 * @param rest The expected characters of the keyword.
 * @param type The token type to return if it's a match.
 * @return TokenType The keyword's token type or `TOKEN_IDENTIFIER`.
 */
static TokenType checkKeyword(int32_t start, int32_t length, const char* rest,
                              TokenType type) {
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

/**
 * @brief Determines if an identifier is a keyword or a user-defined name.
 *
 * This uses a simple trie-like structure implemented with nested switches to
 * efficiently check for keywords.
 *
 * @return TokenType The appropriate `TokenType` for the identifier.
 */
static TokenType identifierType() {
    switch (scanner.start[0]) {
        case 'a':
            return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'c':
            return checkKeyword(1, 4, "lass", TOKEN_CLASS);
        case 'e':
            return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            // check if it is not just 'f'
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a':
                        return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o':
                        return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'u':
                        return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i':
            return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'n':
            return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o':
            return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p':
            return checkKeyword(1, 4, "rint", TOKEN_PRINT);
        case 'r':
            return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's':
            return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            // check if it is not just 't'
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h':
                        return checkKeyword(2, 2, "is", TOKEN_THIS);
                    case 'r':
                        return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v':
            return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w':
            return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

/**
 * @brief Scans an identifier or keyword.
 *
 * Consumes alphanumeric characters and underscores.
 * @return Token A token of type `TOKEN_IDENTIFIER` or a keyword token type.
 */
static Token identifier() {
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}

//-----------------------------------------------------------------------------
//- Main Scanner Function
//-----------------------------------------------------------------------------

/**
 * @brief Scans and returns the next token from the source code.
 *
 * This is the main public function of the scanner. It's called repeatedly by
 * the compiler to get the next token in the stream.
 *
 * @return Token The next token in the source code.
 */
Token scanToken() {
    skipWhitespace();

    scanner.start = scanner.current;
    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();

    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c) {
        case '(':
            return makeToken(TOKEN_LEFT_PAREN);
        case ')':
            return makeToken(TOKEN_RIGHT_PAREN);
        case '{':
            return makeToken(TOKEN_LEFT_BRACE);
        case '}':
            return makeToken(TOKEN_RIGHT_BRACE);
        case ';':
            return makeToken(TOKEN_SEMICOLON);
        case ',':
            return makeToken(TOKEN_COMMA);
        case '.':
            return makeToken(TOKEN_DOT);
        case '-':
            return makeToken(TOKEN_MINUS);
        case '+':
            return makeToken(TOKEN_PLUS);
        case '/':
            return makeToken(TOKEN_SLASH);
        case '*':
            return makeToken(TOKEN_STAR);
        case '!':
            return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"':
            return string();
    }

    return errorToken("Unexpected character.");
}