# CoreLox

**CoreLox** is a C implementation of **clox** — the bytecode-based Lox interpreter described in *Crafting Interpreters* by Robert Nystrom.

## Table of Contents

- [CoreLox](#corelox)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
    - [Highlights \& Optimizations](#highlights--optimizations)
    - [Lox Grammar](#lox-grammar)
  - [Getting Started](#getting-started)
    - [Prerequisites](#prerequisites)
    - [Building](#building)
    - [Running](#running)
  - [Usage Examples](#usage-examples)
  - [Project Structure](#project-structure)
  - [Design \& Implementation Details](#design--implementation-details)
    - [Phases \& Data Flow](#phases--data-flow)
    - [Garbage Collection \& Memory](#garbage-collection--memory)
    - [Classes, Methods \& Inheritance](#classes-methods--inheritance)
    - [Error Handling \& Reporting](#error-handling--reporting)
  - [Development Tasks \& Roadmap](#development-tasks--roadmap)

---

## Features

CoreLox implements all of the features presented in *Crafting Interpreters* for clox, including:

- Lexical scanning (tokenization)  
- Parser and Pratt-style precedence handling  
- Bytecode compiler (single pass)  
- Stack-based virtual machine (VM) for executing bytecode  
- Garbage collection (mark & sweep)  
- String interning  
- First-class functions, closures (with upvalues)  
- Control flow: `if`, `while`, `for`, `return`  
- Class support: methods, inheritance, `this` / `super`  
- Built-in functions (e.g. `clock()`), error reporting  
- REPL (interactive mode) and script mode  

These features mirror the progression in *Crafting Interpreters* for clox.  
Nystrom describes the three main phases — scanner → compiler → VM — and the internal data flows.

### Highlights & Optimizations

- Strings are interned, so duplicate strings share memory and comparison is fast.  
- Closures capture nonlocal variables (upvalues) by referencing stack slots, and when those slots go out of scope, the values are “closed” into heap storage.  
- Objects and closure lifetimes are managed via mark-and-sweep GC.  
- Many error and edge-case checks are included to match the book’s behavior.
- Nan-boxing for smaller memory footprint and better cache locality (a technique using unused bits in IEEE 754 floating-point numbers to tag other types like booleans, nil, or pointers).

### Lox Grammar

```txt
program     → declaration* EOF ;

declaration → classDecl
            | funDecl
            | varDecl
            | statement ;
classDecl   → "class" IDENTIFIER ( "<" IDENTIFIER )?
              "{" function* "}" ;
funDecl     → "fun" function ;
varDecl     → "var" IDENTIFIER ( "=" expression )? ";" ;

statement   → exprStmt
            | forStmt
            | ifStmt
            | printStmt
            | returnStmt
            | whileStmt
            | block ;

exprStmt    → expression ";" ;
forStmt     → "for" "(" ( varDecl | exprStmt | ";" )
                        expression? ";"
                        expression? ")" statement ;
ifStmt      → "if" "(" expression ")" statement
              ( "else" statement )? ;
printStmt   → "print" expression ";" ;
returnStmt  → "return" expression? ";" ;
whileStmt   → "while" "(" expression ")" statement ;
block       → "{" declaration* "}" ;

expression  → assignment ;
assignment  → ( call "." )? IDENTIFIER "=" assignment
            | logic_or ;
logic_or    → logic_and ( "or" logic_and )* ;
logic_and   → equality ( "and" equality )* ;
equality    → comparison ( ( "!=" | "==" ) comparison )* ;
comparison  → term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
term        → factor ( ( "-" | "+" ) factor )* ;
factor      → unary ( ( "/" | "*" ) unary )* ;
unary       → ( "!" | "-" ) unary | call ;
call        → primary ( "(" arguments? ")" | "." IDENTIFIER )* ;
primary     → "true" | "false" | "nil" | "this"
            | NUMBER | STRING | IDENTIFIER | "(" expression ")"
            | "super" "." IDENTIFIER ;

function    → IDENTIFIER "(" parameters? ")" block ;
parameters  → IDENTIFIER ( "," IDENTIFIER )* ;
arguments   → expression ( "," expression )* ;

NUMBER      → DIGIT+ ( "." DIGIT+ )? ;
STRING      → "\"" <any char except "\"">* "\"" ;
IDENTIFIER  → ALPHA ( ALPHA | DIGIT )* ;
ALPHA       → "a" ... "z" | "A" ... "Z" | "_" ;
DIGIT       → "0" ... "9" ;
```

## Getting Started

### Prerequisites

- A **C compiler** supporting C99 (e.g. `gcc`)  
- `make` (GNU Make)  
- Tools for linting/formatting: `cppcheck`, `clang-format` (optional)  

### Building

In the root of the project (where the `Makefile` lives), run:

```bash
make
```

This will:

- Compile `.c` files in `src/` to object files under `bin/`
- Link them into the executable `bin/corelox.exe`

If you want to clean build artifacts:

```bash
make clean
```

You can also run linting checks:

```bash
make lint
```

and format the source:

```bash
make format
```

> **Note:** The `Makefile` uses Windows-style commands (`del`, `if exist`, etc.). If you are on Unix/Linux/macOS, you may need to adapt those parts (e.g. using `rm -f bin/*.o`, `mkdir -p bin`, etc.).

### Running

Once built, you can run **CoreLox** in two primary modes:

1. **REPL mode** (no argument):

  ```bash
  bin/corelox.exe
  ```

  Enter Lox statements interactively, see results, etc.

2. **Script mode** (pass a source file):

  ```bash
  bin/corelox.exe path/to/script.lox
  ```

Errors in scanning, parsing, or runtime will be printed to stderr with line numbers and messages.

## Usage Examples

Here are some sample Lox programs:

```lox
fun fib(n) {
    if (n <= 1) return n;
    return fib(n - 2) + fib(n - 1);
}
for (var i = 0; i < 5; i = i + 1) {
    print fib(i);
}
```

```lox
class Shape {
  init(x, y) {
    this.x = x;
    this.y = y;
  }
  area() {
    return 0;
  }
}

class Rect < Shape {
  init(x, y, w, h) {
    super.init(x, y);
    this.w = w;
    this.h = h;
  }
  area() {
    return this.w * this.h;
  }
}

var r = Rect(0, 0, 3, 4);
print r.area();   // should print 12
```

## Project Structure

Here’s a typical layout:

``` txt
├── .gitignore
├── .clang-format
├── Makefile
├── README.md
├───benchmarks/
│   └── hashtable_get.lox
├───bin/
│   ├── .gitkeep
│   ├── corelox.exe
│   └── *.o
└───src/
    ├── include/
    |   └── *.h
    └── *.c
```

* `src/*.c` and `src/include/` hold the implementation and headers.
* `bin/` contains build artifacts (object files) and the final executable.
* `benchamrks/` contains a lox file for testing the speed of retrieving data from hash table.
* The `Makefile` is configured to discover `src/*.c`, generate `bin/*.o`, and link them.

## Design & Implementation Details

### Phases & Data Flow

CoreLox follows the three-phase design described in the book:

1. **Scanner / Lexer** — reads source text and emits tokens
2. **Compiler / Parser** — consumes tokens and emits bytecode instructions, using recursive descent and Pratt parsing
3. **Virtual Machine (VM)** — interprets the bytecode using a stack, managing frames, closures, and control flow

### Garbage Collection & Memory

- All heap-allocated objects (strings, closures, instances, classes, etc.) are tracked in a linked list for GC.
- A mark-and-sweep algorithm is run when allocation crosses a threshold.
- During marking, reachable objects from the VM stack, open upvalues, globals, and interned strings are visited.
- Sweep frees unused objects.
- String interning is used: only one unique copy of a given string lexeme is kept, reducing memory usage and speeding up comparisons.

### Classes, Methods & Inheritance

- Lox supports first-class classes, methods, inheritance (`<` syntax), `this`, and `super`.
- Method calls compile to bytecodes that handle dynamic dispatch.
- `this` is treated as a lexical variable bound to the instance.
- `super` allows invoking methods on the superclass.

### Error Handling & Reporting

- Errors in scanning or parsing (syntax errors) are reported with line number and token context, then recovery attempts are made to continue.
- Runtime errors (e.g. calling a non-function, type mismatches) abort execution of the current script, print an error message, and exit with a failure status.

## Development Tasks & Roadmap

Here are possible enhancements to consider:

- Implement the *challenges* from each chapter (e.g. run-length encoded line table, optimization passes, switch statement)
- Add more built-in library functions (I/O, strings, lists, maps)
- Serialize/deserialize bytecode (allow saving compiled scripts)
- Add a debugging mode (bytecode disassembler, stepping)
- Port the Makefile to support both Windows and Unix (auto-detect OS)
- Add CI (e.g. GitHub Actions) to automatically build, lint, run tests
- Benchmark and profile performance, compare to other Lox implementations
