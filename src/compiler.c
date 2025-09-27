/**
 * @file compiler.c
 * @brief The Clox bytecode compiler.
 *
 * This file implements a single-pass compiler that parses a stream of tokens
 * from the scanner and emits bytecode for the Clox VM. It uses a Pratt parser
 * for expressions, which handles operator precedence gracefully. It manages
 * lexical scopes, local variables, and upvalues for closures.
 */

#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "memory.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

//-----------------------------------------------------------------------------
//- Compiler State and Structures
//-----------------------------------------------------------------------------

// Global parser state, holds current/previous tokens and error flags.
Parser parser;
// Points to the Compiler struct for the function currently being compiled.
Compiler* current = NULL;
// Points to the ClassCompiler struct for the class currently being compiled.
ClassCompiler* currentClass = NULL;

/**
 * @brief Retrieves the bytecode chunk for the function currently being
 * compiled.
 * @return Chunk* A pointer to the current chunk.
 */
static Chunk* currentChunk() { return &current->function->chunk; }

//-----------------------------------------------------------------------------
//- Error Handling
//-----------------------------------------------------------------------------

/**
 * @brief Reports an error at a specific token's location.
 * @param token The token where the error occurred.
 * @param message The error message to display.
 */
static void errorAt(const Token* token, const char* message) {
    // in panic mode suppress further errors to avoid cascades
    if (parser.panicMode) return;
    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // for error tokens, the message is already in the token's lexeme
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

/**
 * @brief Reports an error at the location of the current token.
 * @param message The error message.
 */
static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

/**
 * @brief Reports an error at the location of the previous token.
 * @param message The error message.
 */
static void error(const char* message) { errorAt(&parser.previous, message); }

static void advance();  // forward declaration
/**
 * @brief Resynchronizes the parser after an error.
 *
 * Skips tokens until it finds a statement boundary, which helps the parser
 * continue after a syntax error to find subsequent errors.
 */
static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:;  // do nothing
        }
        advance();
    }
}

//-----------------------------------------------------------------------------
//- Parser Primitives
//-----------------------------------------------------------------------------

/**
 * @brief Advances the parser to the next token from the scanner.
 */
static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}

/**
 * @brief Consumes the current token if it has the expected type.
 *
 * If the token type does not match, it reports an error.
 * @param type The expected token type.
 * @param message The error message to show if the type doesn't match.
 */
static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

/**
 * @brief Checks if the current token has the given type without consuming it.
 * @param type The token type to check for.
 * @return bool True if the current token matches the type.
 */
static bool check(TokenType type) { return parser.current.type == type; }

/**
 * @brief Matches the current token against a type. If it matches, consumes it.
 * @param type The token type to match.
 * @return bool True if the token was matched and consumed.
 */
static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

//-----------------------------------------------------------------------------
//- Bytecode Emitter Functions
//-----------------------------------------------------------------------------

/**
 * @brief Emits a single byte to the current chunk.
 * @param byte The byte to write.
 */
static void emitByte(uint8_t byte) {
    Chunk* chunk = currentChunk();
    writeChunk(chunk, byte, parser.previous.line);
}

/**
 * @brief Emits two bytes to the current chunk.
 * @param byte1 The first byte.
 * @param byte2 The second byte.
 */
static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

/**
 * @brief Emits a jump instruction with a placeholder offset.
 *
 * The placeholder will be filled in later by `patchJump`.
 * @param instruction The jump opcode (`OP_JUMP` or `OP_JUMP_IF_FALSE`).
 * @return int32_t The offset of the placeholder in the bytecode.
 */
static int32_t emitJump(uint8_t instruction) {
    emitByte(instruction);
    // placeholder for the 16-bit jump offset
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

/**
 * @brief Backpatches a jump instruction's offset.
 *
 * Calculates the distance to jump and writes it into the placeholder
 * emitted by `emitJump`.
 * @param offset The bytecode offset of the placeholder to patch.
 */
static void patchJump(int32_t offset) {
    // -2 to adjust for the bytecode for the jump offset itself
    int32_t jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

/**
 * @brief Emits a loop instruction (OP_LOOP).
 * @param loopStart The bytecode offset of the beginning of the loop.
 */
static void emitLoop(int32_t loopStart) {
    emitByte(OP_LOOP);

    int32_t offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

/**
 * @brief Adds a value to the current chunk's constant table.
 * @param value The value to add.
 * @return uint8_t The index of the added constant.
 */
static uint8_t makeConstant(Value value) {
    int32_t constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants ine one chunk.");
        return 0;
    }
    return (uint8_t)constant;
}

/**
 * @brief Emits an OP_CONSTANT instruction.
 * @param value The value to be loaded from the constant table.
 */
static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

/**
 * @brief Emits the instructions to return from the current function.
 *
 * For initializers (`init`), it implicitly returns `this`. For other functions,
 * it implicitly returns `nil`.
 */
static void emitReturn() {
    if (current->type == TYPE_INITIALIZER) {
        emitBytes(OP_GET_LOCAL, 0);
    } else {
        emitByte(OP_NIL);
    }
    emitByte(OP_RETURN);
}

//-----------------------------------------------------------------------------
//- Compiler Management
//-----------------------------------------------------------------------------

/**
 * @brief Initializes a new Compiler instance for a function or script.
 * @param compiler A pointer to the compiler instance to initialize.
 * @param type The type of code being compiled (script, function, method).
 */
static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name =
            copyString(parser.previous.start, parser.previous.length);
    }

    // The first local slot is reserved for internal use by the VM. For methods,
    // it holds 'this'. For functions, it's unnamed.
    Local* local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = 0;
    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

/**
 * @brief Finalizes the compilation of the current function.
 *
 * Emits a return instruction, optionally disassembles the bytecode, and
 * restores the enclosing compiler's state.
 * @return ObjectFunction* The compiled function object.
 */
static ObjectFunction* endCompiler() {
    emitReturn();
    ObjectFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL
                                             ? function->name->chars
                                             : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

//-----------------------------------------------------------------------------
//- Scope and Variable Management
//-----------------------------------------------------------------------------

/**
 * @brief Enters a new lexical scope.
 */
static void beginScope() { current->scopeDepth++; }

/**
 * @brief Exits the current lexical scope.
 *
 * Emits OP_POP or OP_CLOSE_UPVALUE for each local variable declared in the
 * scope.
 */
static void endScope() {
    current->scopeDepth--;

    // walk backwards poping variables from the scope
    while (current->localCount > 0 &&
           current->locals[current->localCount - 1].depth >
               current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

/**
 * @brief Checks if two identifiers are the same.
 */
static bool identifiersEqual(const Token* a, const Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

/**
 * @brief Adds a local variable to the current compiler's scope.
 * @param name The token containing the variable's name.
 */
static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;  // to indicate unintialized state
    local->isCaptured = false;
}

/**
 * @brief Declares a local variable.
 *
 * For local scopes, it adds the variable to the compiler's list of locals
 * and checks for redeclarations within the same scope. Globals are handled
 * at runtime, so this is a no-op for the global scope.
 */
static void declareVariable() {
    // global variables are late bound and implicitly declared
    if (current->scopeDepth == 0) return;

    // redeclaring a local variable is not permitted
    const Token* name = &parser.previous;
    for (int32_t i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        // if a variable is owned by another (higher) scope,
        // we know we've checked all of the existing variables in the scope
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }
        if (identifiersEqual(name, &local->name)) {
            error("Already variable with this name in this scope.");
        }
    }

    addLocal(*name);
}

/**
 * @brief Creates a constant for an identifier's name.
 */
static uint8_t identifierConstant(const Token* name) {
    return makeConstant(OBJECT_VAL(copyString(name->start, name->length)));
}

/**
 * @brief Parses a variable name and adds it to the appropriate scope.
 * @param errorMessage The error message to show if an identifier isn't found.
 * @return uint8_t The constant table index for a global, or 0 for a local.
 */
static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

/**
 * @brief Marks the most recently declared local variable as initialized.
 */
static void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

/**
 * @brief Emits code to define a variable.
 *
 * For globals, emits OP_DEFINE_GLOBAL. For locals, this simply marks the
 * variable as initialized.
 * @param global The constant table index of the global variable's name.
 */
static void defineVariable(uint8_t global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }

    emitBytes(OP_DEFINE_GLOBAL, global);
}

/**
 * @brief Resolves a local variable by name in the current function's scopes.
 * @param compiler The current compiler.
 * @param name The token for the variable's name.
 * @return int32_t The stack slot of the local, or -1 if not found.
 */
static int32_t resolveLocal(Compiler* compiler, const Token* name) {
    for (int32_t i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

/**
 * @brief Adds an upvalue to the current function's list of upvalues.
 *
 * An upvalue is a local variable from an enclosing function that is "closed
 * over" by an inner function.
 *
 * @param compiler The current compiler.
 * @param index The index of the upvalue.
 * @param isLocal True if the upvalue captures a local variable from the
 * enclosing function; false if it captures another upvalue.
 * @return int32_t The index of the new upvalue.
 */
static int32_t addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int32_t upvalueCount = compiler->function->upvalueCount;

    // reuse an existing upvalue if it captures the same variable
    for (int32_t i = 0; i < upvalueCount; i++) {
        const Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

/**
 * @brief Resolves an identifier as an upvalue.
 *
 * Recursively walks up the chain of enclosing functions to find the variable.
 * @param compiler The current compiler.
 * @param name The token for the variable's name.
 * @return int32_t The index of the upvalue, or -1 if not found.
 */
static int32_t resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;

    // try to resolve as a local in the enclosing function
    int32_t local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    // try to resolve as an upvalue in the enclosing function (recursively)
    int32_t upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

//-----------------------------------------------------------------------------
//- Forward Declarations for Parsing
//-----------------------------------------------------------------------------
static void expression();
static void declaration();
static void statement();
static void namedVariable(Token name, bool canAssign);
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

//-----------------------------------------------------------------------------
//- Pratt Parser: Expression Parsing Rules
//-----------------------------------------------------------------------------

/**
 * @brief Main driver for the Pratt parser.
 *
 * It parses an expression with a given precedence level, handling both
 * prefix and infix operators correctly.
 * @param precedence The minimum precedence level to parse.
 */
static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    // the first token is always going to belong to some kind of prefix
    // expression
    if (prefixRule == NULL) {
        error("Expected expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    // after parsing prefixRule(), we look for an infix parser for the next
    // token
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        // this catches cases like `a * b = c`, which is not a valid assignment
        error("Invalid assignment target.");
    }
}

ParseRule rules[];  // forawrd declaration
/**
 * @brief Gets the parsing rule for a given token type from the `rules` table.
 */
static ParseRule* getRule(TokenType type) { return &rules[type]; }

/**
 * @brief Wrapper for `parsePrecedence(PREC_ASSIGNMENT)`.
 */
static void expression() { parsePrecedence(PREC_ASSIGNMENT); }

//-----------------------------------------------------------------------------
//- Expression Parsing Functions
//-----------------------------------------------------------------------------

/**
 * @brief Parses a grouping expression, like `(1 + 2)`.
 */
static void grouping(bool canAssign __attribute__((unused))) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

/**
 * @brief Parses a unary operator expression, like `-x` or `!y`.
 */
static void unary(bool canAssign __attribute__((unused))) {
    TokenType operatorType = parser.previous.type;

    // compile the operand
    parsePrecedence(PREC_UNARY);

    // emit the operator intruction
    switch (operatorType) {
        case TOKEN_BANG:
            emitByte(OP_NOT);
            break;
        case TOKEN_MINUS:
            emitByte(OP_NEGATE);
            break;
        default:
            return;  // unreachable
    }
}

/**
 * @brief Parses a binary operator expression, like `a + b`.
 */
static void binary(bool canAssign __attribute__((unused))) {
    // remember the operator
    TokenType operatorType = parser.previous.type;

    // compile the right operand
    const ParseRule* rule = getRule(operatorType);
    // compile the right operand with one higher precedence to handle
    // left-associativity (5 - 3 - 1 is parsed as (5 - 3) - 1).
    parsePrecedence((Precedence)(rule->precedence + 1));

    // emit the operator instruction
    switch (operatorType) {
        case TOKEN_BANG_EQUAL:
            emitBytes(OP_EQUAL, OP_NOT);
            break;
        case TOKEN_EQUAL_EQUAL:
            emitByte(OP_EQUAL);
            break;
        case TOKEN_GREATER:
            emitByte(OP_GREATER);
            break;
        case TOKEN_GREATER_EQUAL:
            emitBytes(OP_LESS, OP_NOT);
            break;
        case TOKEN_LESS:
            emitByte(OP_LESS);
            break;
        case TOKEN_LESS_EQUAL:
            emitBytes(OP_GREATER, OP_NOT);
            break;
        case TOKEN_PLUS:
            emitByte(OP_ADD);
            break;
        case TOKEN_MINUS:
            emitByte(OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emitByte(OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emitByte(OP_DIVIDE);
            break;
        default:
            return;  // unreachable
    }
}

/**
 * @brief Parses a logical AND expression. Uses short-circuiting.
 */
static void and_(bool canAssign __attribute__((unused))) {
    // if the left side is false, jump over the right side
    int32_t endJump = emitJump(OP_JUMP_IF_FALSE);

    // clean up the stack -> statements are required to have zero stack effect
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

/**
 * @brief Parses a logical OR expression. Uses short-circuiting.
 */
static void or_(bool canAssign __attribute__((unused))) {
    // if the left side is false, jump to evaluate the right side
    int32_t elseJump = emitJump(OP_JUMP_IF_FALSE);
    // if the left side is true, jump over the right side
    int32_t endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    // to clean up the stack
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

/**
 * @brief Parses an argument list for a function call.
 * @return uint8_t The number of arguments parsed.
 */
static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments.");
    return argCount;
}

/**
 * @brief Parses a function call expression, like `myFunc(a, b)`.
 */
static void call(bool canAssign __attribute__((unused))) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

/**
 * @brief Parses a dot operator for property access/assignment or method calls.
 */
static void dot(bool canAssign) {
    consume(TOKEN_IDENTIFIER, "Expected property name after '.'.");
    uint8_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)) {
        // property assignment -> obj.prop = value
        expression();
        emitBytes(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)) {
        // method call -> obj.method(args)
        uint8_t argCount = argumentList();
        emitBytes(OP_INVOKE, name);
        emitByte(argCount);
    } else {
        // property access -> obj.prop
        emitBytes(OP_GET_PROPERTY, name);
    }
}

/**
 * @brief Parses a literal value (`true`, `false`, `nil`).
 */
static void literal(bool canAssign __attribute__((unused))) {
    switch (parser.previous.type) {
        case TOKEN_FALSE:
            emitByte(OP_FALSE);
            break;
        case TOKEN_TRUE:
            emitByte(OP_TRUE);
            break;
        case TOKEN_NIL:
            emitByte(OP_NIL);
            break;
        default:
            return;  // unreachable
    }
}

/**
 * @brief Parses a number literal.
 */
static void number(bool canAssign __attribute__((unused))) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

/**
 * @brief Parses a string literal.
 */
static void string(bool canAssign __attribute__((unused))) {
    emitConstant(OBJECT_VAL(
        copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

/**
 * @brief Compiles a variable access or assignment.
 *
 * Determines if the variable is local, an upvalue, or global, and emits
 * the corresponding get/set instruction.
 * @param name The token of the variable's name.
 * @param canAssign Whether the context allows assignment.
 */
static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int32_t arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(setOp, arg);
    } else {
        emitBytes(getOp, arg);
    }
}

/**
 * @brief Parses a variable expression using the previously consumed identifier.
 */
static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

/**
 * @brief Parses the `this` keyword.
 */
static void this_(bool canAssign __attribute__((unused))) {
    if (currentClass == NULL) {
        error("Can't use 'this' outside of a class.");
        return;
    }
    variable(false);  // treat 'this' as a read-only local variable
}

/**
 * @brief Creates a token with the given text.
 * Used internally for keywords like `this` and `super`.
 */
static Token syntheticToken(const char* text) {
    Token token;
    token.start = text;
    token.length = (int32_t)strlen(text);
    token.type = TOKEN_SYNTHETIC;
    token.line = parser.previous.line;
    return token;
}

/**
 * @brief Parses the `super` keyword for superclass method access.
 */
static void super_(bool canAssign __attribute__((unused))) {
    if (currentClass == NULL) {
        error("Can't use 'super' outside of a class.");
    } else if (!currentClass->hasSuperclass) {
        error("Can't use 'super' in a class with no superclass.");
    }

    consume(TOKEN_DOT, "Expected '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expected superclass method name.");
    uint8_t name = identifierConstant(&parser.previous);

    namedVariable(syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList();
        namedVariable(syntheticToken("super"), false);
        emitBytes(OP_SUPER_INVOKE, name);
        emitByte(argCount);
    } else {
        namedVariable(syntheticToken("super"), false);
        emitBytes(OP_GET_SUPER, name);
    }
}

// Table for Pratt parser. For each token type, it specifies a function to
// handle it as a prefix operator, a function for infix operators, and its
// precedence.

// clang-format off
ParseRule rules[] = {
    // token type             prefix fun.   infix fun.  precedence
    [TOKEN_LEFT_PAREN]      = {grouping,    call,       PREC_CALL},
    [TOKEN_RIGHT_PAREN]     = {NULL,        NULL,       PREC_NONE},
    [TOKEN_LEFT_BRACE]      = {NULL,        NULL,       PREC_NONE},
    [TOKEN_RIGHT_BRACE]     = {NULL,        NULL,       PREC_NONE},
    [TOKEN_COMMA]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_DOT]             = {NULL,        dot,        PREC_CALL},
    [TOKEN_MINUS]           = {unary,       binary,     PREC_TERM},
    [TOKEN_PLUS]            = {NULL,        binary,     PREC_TERM},
    [TOKEN_SEMICOLON]       = {NULL,        NULL,       PREC_NONE},
    [TOKEN_SLASH]           = {NULL,        binary,     PREC_FACTOR},
    [TOKEN_STAR]            = {NULL,        binary,     PREC_FACTOR},
    [TOKEN_BANG]            = {unary,       NULL,       PREC_NONE},
    [TOKEN_BANG_EQUAL]      = {NULL,        binary,     PREC_EQUALITY},
    [TOKEN_EQUAL]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EQUAL_EQUAL]     = {NULL,        binary,     PREC_EQUALITY},
    [TOKEN_GREATER]         = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_LESS]            = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      = {NULL,        binary,     PREC_COMPARISON},
    [TOKEN_IDENTIFIER]      = {variable,    NULL,       PREC_NONE},
    [TOKEN_STRING]          = {string,      NULL,       PREC_NONE},
    [TOKEN_NUMBER]          = {number,      NULL,       PREC_NONE},
    [TOKEN_AND]             = {NULL,        and_,       PREC_AND},
    [TOKEN_CLASS]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ELSE]            = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FALSE]           = {literal,     NULL,       PREC_NONE},
    [TOKEN_FOR]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_FUN]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_IF]              = {NULL,        NULL,       PREC_NONE},
    [TOKEN_NIL]             = {literal,     NULL,       PREC_NONE},
    [TOKEN_OR]              = {NULL,        or_,        PREC_OR},
    [TOKEN_PRINT]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_RETURN]          = {NULL,        NULL,       PREC_NONE},
    [TOKEN_SUPER]           = {super_,      NULL,       PREC_NONE},
    [TOKEN_THIS]            = {this_,       NULL,       PREC_NONE},
    [TOKEN_TRUE]            = {literal,     NULL,       PREC_NONE},
    [TOKEN_VAR]             = {NULL,        NULL,       PREC_NONE},
    [TOKEN_WHILE]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_ERROR]           = {NULL,        NULL,       PREC_NONE},
    [TOKEN_EOF]             = {NULL,        NULL,       PREC_NONE},
};
// clang-format on

//-----------------------------------------------------------------------------
//- Statement and Declaration Parsing
//-----------------------------------------------------------------------------

/**
 * @brief Parses a block of statements enclosed in `{}`.
 */
static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RIGHT_BRACE, "Expected '}' after block.");
}

/**
 * @brief Parses a function's definition (name, parameters, and body).
 */
static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    // compile the parameter list
    consume(TOKEN_LEFT_PAREN, "Expected '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }

            uint8_t paramConstant = parseVariable("Expected parameter name.");
            defineVariable(paramConstant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after parameters.");

    // compile the body
    consume(TOKEN_LEFT_BRACE, "Expected '{' before function body.");
    block();

    // create the function object and emit OP_CLOSURE
    ObjectFunction* functionObj = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJECT_VAL(functionObj)));

    // emit bytecode for each upvalue captured by the closure
    for (int32_t i = 0; i < functionObj->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

/**
 * @brief Parses a function declaration statement.
 */
static void functionDeclaration() {
    uint8_t global = parseVariable("Expected function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

/**
 * @brief Parses a method definition within a class.
 */
static void method() {
    consume(TOKEN_IDENTIFIER, "Expected method name.");
    uint8_t constant = identifierConstant(&parser.previous);

    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 4 &&
        memcmp(parser.previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }

    function(type);
    emitBytes(OP_METHOD, constant);
}

/**
 * @brief Parses a class declaration.
 */
static void classDeclaration() {
    consume(TOKEN_IDENTIFIER, "Expected class name.");
    Token className = parser.previous;
    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();

    emitBytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    ClassCompiler classCompiler;
    classCompiler.name = parser.previous;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    // handle inheritance
    if (match(TOKEN_LESS)) {
        consume(TOKEN_IDENTIFIER, "Expected superclass name.");
        variable(false);

        if (identifiersEqual(&className, &parser.previous)) {
            error("A class can't inherit from itself.");
        }

        // create a new scope for 'super'
        beginScope();
        addLocal(syntheticToken("super"));
        defineVariable(0);

        namedVariable(className, false);
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false);
    consume(TOKEN_LEFT_BRACE, "Expected '{' before class body.");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expected '}' after class body.");
    emitByte(OP_POP);  // pop the class from the stack

    if (classCompiler.hasSuperclass) {
        endScope();  // close the 'super' scope
    }

    currentClass = currentClass->enclosing;
}

/**
 * @brief Parses a variable declaration (`var name [ = initializer ];`).
 */
static void variableDeclaration() {
    uint8_t global = parseVariable("Expected variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);  // the compiler desugars 'var a;' to 'var a = nil;'
    }
    consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration.");

    defineVariable(global);
}

/**
 * @brief Parses an expression statement.
 */
static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after value.");
    emitByte(OP_POP);
}

/**
 * @brief Parses a `print` statement.
 */
static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expected ';' after value;");
    emitByte(OP_PRINT);
}

/**
 * @brief Parses a `return` statement.
 */
static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        emitReturn();  // implicit nil return
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer.");
        }
        expression();
        consume(TOKEN_SEMICOLON, "Expected ';' after return value.");
        emitByte(OP_RETURN);
    }
}

/**
 * @brief Parses an `if` statement.
 */
static void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expected '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition.");

    int32_t thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);  // pop condition
    statement();

    int32_t elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);  // pop condition

    if (match(TOKEN_ELSE)) statement();
    patchJump(elseJump);
}

/**
 * @brief Parses a `while` statement.
 */
static void whileStatement() {
    int32_t loopStart = currentChunk()->count;

    consume(TOKEN_LEFT_PAREN, "Expected '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition.");

    int32_t exitJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    statement();

    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

/**
 * @brief Parses a `for` statement.
 *
 * This is "desugared" into equivalent bytecode using jumps and primitives,
 * effectively behaving like a `while` loop with initializer and increment
 * clauses.
 */
static void forStatement() {
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expected '(' after 'for'.");

    // initializer clause
    if (match(TOKEN_SEMICOLON)) {
        // no intializer
    } else if (match(TOKEN_VAR)) {
        variableDeclaration();
    } else {
        expressionStatement();
    }

    int32_t loopStart = currentChunk()->count;

    // condition clause
    int32_t exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expected ';' after loop condition.");

        // jump out of the loop if the condition is false
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP);  // condition
    }

    // increment clause
    if (!match(TOKEN_RIGHT_PAREN)) {
        int32_t bodyJump = emitJump(OP_JUMP);

        int32_t incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expected ')' after for clauses.");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();

    emitLoop(loopStart);

    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    endScope();
}

/**
 * @brief Parses a statement.
 */
static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else {
        expressionStatement();
    }
}

/**
 * @brief Parses a declaration.
 */
static void declaration() {
    if (match(TOKEN_CLASS)) {
        classDeclaration();
    } else if (match(TOKEN_FUN)) {
        functionDeclaration();
    } else if (match(TOKEN_VAR)) {
        variableDeclaration();
    } else {
        statement();
    }
    if (parser.panicMode) synchronize();
}

//-----------------------------------------------------------------------------
//- Main Compiler Entry Point
//-----------------------------------------------------------------------------

/**
 * @brief Compiles a Clox source string into a function object with bytecode.
 * @param source The null-terminated string of Clox source code.
 * @return ObjectFunction* The compiled top-level script function, or NULL on
 * error.
 */
ObjectFunction* compile(const char* source) {
    initScanner(source);

    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }

    ObjectFunction* functionObj = endCompiler();
    return parser.hadError ? NULL : functionObj;
}

/**
 * @brief Marks all compiler roots for garbage collection.
 *
 * This function is called by the garbage collector to trace all reachable
 * objects that are still being used by the compiler (like function objects
 * under construction).
 */
void markCompilerRoots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        markObject((Object*)compiler->function);
        compiler = compiler->enclosing;
    }
}