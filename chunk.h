#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// operation code (instruction id)
typedef enum {
    OP_CONSTANT,
    OP_ADD,
    OP_SUBSTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_RETURN,
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