/**
 * @file compiler.h
 * @brief Data structures and interfaces for the Clox bytecode compiler.
 *
 * This file defines the core components of the compiler, including the
 * `Parser` and `Compiler` structs that maintain state during compilation.
 * It also defines the precedence levels for the Pratt parser and the structures
 * for managing local variables and upvalues.
 */

#ifndef corelox_compiler_h
#define corelox_compiler_h

#include "object.h"
#include "scanner.h"
#include "vm.h"

// Defines the precedence levels for operators in the Pratt parser.
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  ///< =
    PREC_OR,          ///< or
    PREC_AND,         ///< and
    PREC_EQUALITY,    ///< == !=
    PREC_COMPARISON,  ///< < > <= >=
    PREC_TERM,        ///< + -
    PREC_FACTOR,      ///< * /
    PREC_UNARY,       ///< ! -
    PREC_CALL,        ///< . ()
    PREC_PRIMARY
} Precedence;

// A function pointer type for a parsing function in the Pratt parser.
typedef void (*ParseFn)(bool canAssign);

// A rule in the Pratt parser's table, defining how to parse a token.
typedef struct {
    ParseFn prefix;  ///< Function to parse the token as a prefix operator.
    ParseFn infix;   ///< Function to parse the token as an infix operator.
    Precedence precedence;  ///< The precedence of the token when used as an
                            ///< infix operator.
} ParseRule;

// Represents a local variable in the compiler's scope tracking.
typedef struct {
    Token name;       ///< The token containing the variable's name.
    int32_t depth;    ///< The scope depth where the variable was declared.
    bool isCaptured;  ///< True if this local is closed over by a closure.
} Local;

// Represents a captured variable (upvalue) from an enclosing scope.
typedef struct {
    uint8_t
        index;  ///< The index of the variable (local or upvalue) in the parent.
    bool isLocal;  ///< True if it captures a local, false if an upvalue.
} Upvalue;

// The type of function being compiled.
typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT,  // All top-level code is impilictly wrapped in that function
} FunctionType;

// The main state for a single function's compilation process.
typedef struct Compiler {
    struct Compiler*
        enclosing;  ///< Pointer to the compiler for the enclosing function.
    ObjectFunction* function;   ///< The function object being built.
    FunctionType type;          ///< The type of function being compiled.
    Local locals[UINT8_COUNT];  ///< An array to track local variables.
    int32_t localCount;         ///< The number of locals currently in scope.
    Upvalue upvalues[UINT8_COUNT];  ///< An array to track upvalues.
    int32_t scopeDepth;             ///< The current nesting level of scopes.
} Compiler;

// State for compiling a class, linked to the main compiler.
typedef struct ClassCompiler {
    struct ClassCompiler*
        enclosing;  ///< Pointer to the enclosing class's compiler.
    Token name;     ///< Name of the class.
    bool
        hasSuperclass;  ///< Flag indicating if the class inherits from another.
} ClassCompiler;

// The state of the parser, holding tokens and error flags.
typedef struct {
    Token current;   ///< The current token being processed.
    Token previous;  ///< The most recently consumed token.
    bool hadError;   ///< Flag indicating if a compile error has occurred.
    bool panicMode;  ///< Flag to prevent error cascades.
} Parser;

/**
 * @brief Compiles a Clox source string into a function object with bytecode.
 * @param source The null-terminated string of Clox source code.
 * @return ObjectFunction* The compiled top-level script function, or NULL on
 * error.
 */
ObjectFunction* compile(const char* source);

/**
 * @brief Marks all compiler roots for garbage collection.
 *
 * This function is called by the garbage collector to trace all reachable
 * objects that are still being used by the compiler (like function objects
 * under construction).
 */
void markCompilerRoots();

#endif