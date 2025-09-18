#ifndef corelox_debug_h
#define corelox_debug_h

#include "chunk.h"

void disassembleChunk(Chunk* chunk, const char* name);
int32_t disassembleInstruction(Chunk* chunk, int32_t offset);

#endif