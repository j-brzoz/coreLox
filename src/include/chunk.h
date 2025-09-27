/**
 * @file chunk.h
 * @brief Defines the structure for bytecode chunks and operation codes.
 *
 * This file contains the `OpCode` enum, which lists all possible instructions
 * for the Clox VM, and the `Chunk` struct, which is a dynamic array that
 * stores these instructions along with associated metadata.
 */

#ifndef corelox_chunk_h
#define corelox_chunk_h

#include "common.h"
#include "value.h"

// An enum of all operation codes (instructions) for the Clox VM.
typedef enum {
    OP_CONSTANT,       //< Push a constant onto the stack.
    OP_NIL,            ///< Push a nil value onto the stack.
    OP_TRUE,           ///< Push a true value onto the stack.
    OP_FALSE,          ///< Push a false value onto the stack.
    OP_POP,            ///< Pop the top value from the stack.
    OP_GET_LOCAL,      ///< Get a local variable.
    OP_SET_LOCAL,      ///< Set a local variable.
    OP_GET_PROPERTY,   ///< Get a property of an object.
    OP_SET_PROPERTY,   ///< Set a property of an object.
    OP_GET_UPVALUE,    ///< Get a variable based on upvalue
    OP_SET_UPVALUE,    ///< Set a variable based on upvalue
    OP_GET_SUPER,      ///< Get a property from a superclass.
    OP_GET_GLOBAL,     ///< Get a global variable.
    OP_SET_GLOBAL,     ///< Set a global variable.
    OP_DEFINE_GLOBAL,  ///< Define a global variable.
    OP_EQUAL,          ///< Check if two values are equal.
    OP_GREATER,        ///< Check if one value is greater than another.
    OP_LESS,           ///< Check if one value is less than another.
    OP_ADD,            ///< Add two values.
    OP_SUBTRACT,       ///< Subtract one value from another.
    OP_MULTIPLY,       ///< Multiply two values.
    OP_DIVIDE,         ///< Divide one value by another.
    OP_NOT,            ///< Negate a boolean value.
    OP_NEGATE,         ///< Negate a value.
    OP_PRINT,          ///< Print the top value on the stack.
    OP_JUMP,           ///< Jump to a different location in the bytecode.s
    OP_JUMP_IF_FALSE,  ///< Jump if the top value on the stack is false.
    OP_LOOP,           ///< Loop back to a previous location in the bytecode.
    OP_CALL,           ///< Call a function.
    OP_INVOKE,         ///< Invoke a method on an object.
    OP_SUPER_INVOKE,   ///< Invoke a method on a superclass.
    OP_CLOSURE,        ///< Create a closure object from a function.
    OP_CLOSE_UPVALUE,  ///< Close an upvalue over local and copy its value to
                       ///< the heap.
    OP_RETURN,         ///< Return from the current function or program.
    OP_CLASS,          ///< Define a new class.
    OP_INHERIT,        ///< Inherit methods from a superclass.
    OP_METHOD,         ///< Define a new method for a class.
} OpCode;

// A dynamic array that stores a sequence of bytecode instructions.
typedef struct {
    int32_t count;     ///< The number of instructions currently in the chunk.
    int32_t capacity;  ///< The allocated capacity of the `code` array.
    uint8_t* code;     ///< The array of bytecode instructions.
    int32_t* lines;  ///< An array storing the source line number for each byte.
    ValueArray
        constants;  ///< A pool of constant values used by the instructions.
} Chunk;

/**
 * @brief Initializes a new, empty chunk.
 * @param chunk A pointer to the Chunk struct to initialize.
 */
void initChunk(Chunk* chunk);

/**
 * @brief Frees all memory associated with a chunk.
 * @param chunk A pointer to the Chunk struct to free.
 */
void freeChunk(Chunk* chunk);

/**
 * @brief Appends a byte (instruction or operand) to the end of the chunk.
 *
 * If the chunk's capacity is insufficient, it will be grown automatically.
 *
 * @param chunk A pointer to the chunk.
 * @param byte The byte to write.
 * @param line The source line number corresponding to this byte.
 */
void writeChunk(Chunk* chunk, uint8_t byte, int32_t line);

/**
 * @brief Adds a constant value to the chunk's constant pool.
 *
 * @param chunk A pointer to the chunk.
 * @param value The value to add to the constant pool.
 * @return int32_t The index of the added constant in the pool.
 */
int32_t addConstant(Chunk* chunk, Value value);

#endif