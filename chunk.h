#ifndef corelox_chunk_h
#define corelox_chunk_h

#include "common.h"
#include "value.h"

// operation code (instruction id)
typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP, // for discarding expression statement result, like {foo("cat");} and removing variables after exiting a scope
    OP_GET_LOCAL,
    OP_GET_UPVALUE,
    OP_GET_PROPERTY,
    OP_GET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_SET_LOCAL,
    OP_SET_UPVALUE,
    OP_SET_PROPERTY,
    OP_SET_GLOBAL,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_ADD,
    OP_SUBSTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_CLOSE_UPVALUE,
    OP_RETURN,
    OP_CLASS,
    OP_METHOD,
} OpCode;

// dynamic array storing instructions
typedef struct {
    int32_t count;
    int32_t capacity;
    uint8_t* code;
    int32_t* lines;
    ValueArray constants;
} Chunk;
                    
void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int32_t line);
int32_t addConstant(Chunk* chunk, Value value);

#endif