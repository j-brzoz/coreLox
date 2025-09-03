#include <stdio.h>

#include "debug.h"

void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);

    for (int32_t offset = 0; offset < chunk->count;) {
        // after disassembling the instruction at the given offset,
        // it return the offset of the next instruction
        offset = disassembleInstruction(chunk, offset);
    }
}

static int32_t simpleInstruction(const char* name, int32_t offset) {
    printf("%s\n", name);
    return offset + 1;
}

int32_t disassembleInstruction(Chunk* chunk, int32_t offset) {
    printf("%04d ", offset);

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}