#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"

// operation code (instruction id)
typedef enum {
    OP_RETURN,
} OpCode;

// dynamic array storing instructions
typedef struct {
    int16_t count;
    int16_t capacity;
    uint8_t* code;
} Chunk;
                    
void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte);

#endif