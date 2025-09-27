/**
 * @file chunk.c
 * @brief Implementation of bytecode chunks.
 *
 * A chunk represents a sequence of bytecode instructions. It's a dynamic
 * array of bytes that can grow as more instructions are added. Each chunk
 * also contains a constant pool for storing literal values and a list of
 * line numbers corresponding to each instruction for error reporting.
 */

#include "chunk.h"

#include <stdlib.h>

#include "memory.h"
#include "vm.h"

/**
 * @brief Initializes a new, empty chunk.
 * @param chunk A pointer to the Chunk struct to initialize.
 */
void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArray(&chunk->constants);
}

/**
 * @brief Frees all memory associated with a chunk.
 * @param chunk A pointer to the Chunk struct to free.
 */
void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int32_t, chunk->lines, chunk->capacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

/**
 * @brief Appends a byte (instruction or operand) to the end of the chunk.
 *
 * If the chunk's capacity is insufficient, it will be grown automatically.
 *
 * @param chunk A pointer to the chunk.
 * @param byte The byte to write.
 * @param line The source line number corresponding to this byte.
 */
void writeChunk(Chunk* chunk, uint8_t byte, int32_t line) {
    if (chunk->capacity < chunk->count + 1) {
        int32_t oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code =
            GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
        chunk->lines =
            GROW_ARRAY(int32_t, chunk->lines, oldCapacity, chunk->capacity);
    }
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

/**
 * @brief Adds a constant value to the chunk's constant pool.
 *
 * @param chunk A pointer to the chunk.
 * @param value The value to add to the constant pool.
 * @return int32_t The index of the added constant in the pool.
 */
int32_t addConstant(Chunk* chunk, Value value) {
    // push/pop the value to ensure the GC knows it's reachable while the
    // value array might be reallocated during the write
    push(value);
    writeValueArray(&chunk->constants, value);
    pop();
    return chunk->constants.count - 1;
}