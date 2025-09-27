/**
 * @file debug.c
 * @brief Debugging utilities for the Clox interpreter.
 *
 * This file contains functions for disassembling bytecode chunks, which is
 * essential for debugging the compiler and understanding the VM's execution.
 */

#include "debug.h"

#include <stdio.h>

#include "object.h"
#include "value.h"

// forward declarations for static helper functions
static int32_t simpleInstruction(const char* name, int32_t offset);
static int32_t byteInstruction(const char* name, Chunk* chunk, int32_t offset);
static int32_t constantInstruction(const char* name, Chunk* chunk,
                                   int32_t offset);
static int32_t jumpInstruction(const char* name, int32_t sign, Chunk* chunk,
                               int32_t offset);
static int32_t invokeInstruction(const char* name, Chunk* chunk,
                                 int32_t offset);

//-----------------------------------------------------------------------------
//- Disassembler Public Interface
//-----------------------------------------------------------------------------

/**
 * @brief Disassembles all instructions in a given chunk.
 * @param chunk A pointer to the chunk to disassemble.
 * @param name A name for the chunk (e.g., the function name) to be printed
 * in the header.
 */
void disassembleChunk(Chunk* chunk, const char* name) {
    printf("== %s ==\n", name);

    for (int32_t offset = 0; offset < chunk->count;) {
        // after disassembling the instruction at the given offset,
        // it return the offset of the next instruction
        offset = disassembleInstruction(chunk, offset);
    }
}

/**
 * @brief Disassembles a single instruction at a given offset in a chunk.
 * @param chunk The chunk containing the instruction.
 * @param offset The byte offset of the instruction to disassemble.
 * @return int32_t The offset of the next instruction.
 */
int32_t disassembleInstruction(Chunk* chunk, int32_t offset) {
    printf("%04d ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4d ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_NIL:
            return simpleInstruction("OP_NIL", offset);
        case OP_TRUE:
            return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE:
            return simpleInstruction("OP_FALSE", offset);
        case OP_POP:
            return simpleInstruction("OP_POP", offset);
        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_GET_UPVALUE:
            return byteInstruction("OP_GET_UPVALUE", chunk, offset);
        case OP_GET_PROPERTY:
            return constantInstruction("OP_GET_PROPERTY", chunk, offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", chunk, offset);
        case OP_SET_UPVALUE:
            return byteInstruction("OP_SET_UPVALUE", chunk, offset);
        case OP_SET_PROPERTY:
            return constantInstruction("OP_SET_PROPERTY", chunk, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_EQUAL:
            return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER:
            return simpleInstruction("OP_GREATER", offset);
        case OP_LESS:
            return simpleInstruction("OP_LESS", offset);
        case OP_ADD:
            return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT:
            return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY:
            return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE:
            return simpleInstruction("OP_DIVIDE", offset);
        case OP_NOT:
            return simpleInstruction("OP_NOT", offset);
        case OP_NEGATE:
            return simpleInstruction("OP_NEGATE", offset);
        case OP_PRINT:
            return simpleInstruction("OP_PRINT", offset);
        case OP_JUMP:
            return jumpInstruction("OP_JUMP", 1, chunk, offset);
        case OP_JUMP_IF_FALSE:
            return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
        case OP_LOOP:
            return jumpInstruction("OP_LOOP", -1, chunk, offset);
        case OP_CALL:
            return byteInstruction("OP_CALL", chunk, offset);
        case OP_INVOKE:
            return invokeInstruction("OP_INVOKE", chunk, offset);
        case OP_SUPER_INVOKE:
            return invokeInstruction("OP_SUPER_INVOKE", chunk, offset);
        case OP_CLOSURE: {
            offset++;
            uint8_t constant = chunk->code[offset++];
            printf("%-16s %4d ", "OP_CLOSURE", constant);
            printValue(chunk->constants.values[constant]);
            printf("\n");

            const ObjectFunction* function =
                AS_FUNCTION(chunk->constants.values[constant]);
            for (int32_t j = 0; j < function->upvalueCount; j++) {
                int32_t isLocal = chunk->code[offset++];
                int32_t index = chunk->code[offset++];
                printf("%04d      |                     %s %d\n", offset - 2,
                       isLocal ? "local" : "upvalue", index);
            }

            return offset;
        }
        case OP_CLOSE_UPVALUE:
            return simpleInstruction("OP_CLOSE_UPVALUE", offset);
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        case OP_CLASS:
            return constantInstruction("OP_CLASS", chunk, offset);
        case OP_INHERIT:
            return simpleInstruction("OP_INHERIT", offset);
        case OP_METHOD:
            return constantInstruction("OP_METHOD", chunk, offset);
        default:
            printf("Unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

//-----------------------------------------------------------------------------
//- Disassembler Helpers
//-----------------------------------------------------------------------------

/**
 * @brief Disassembles a simple instruction with no operands.
 * @param name The name of the instruction.
 * @param offset The byte offset of the instruction.
 * @return int32_t The offset of the next instruction.
 */
static int32_t simpleInstruction(const char* name, int32_t offset) {
    printf("%s\n", name);
    return offset + 1;
}

/**
 * @brief Disassembles an instruction with a one-byte operand (e.g., a stack
 * slot).
 * @param name The name of the instruction.
 * @param chunk The chunk containing the instruction.
 * @param offset The byte offset of the instruction.
 * @return int32_t The offset of the next instruction.
 */
static int32_t byteInstruction(const char* name, Chunk* chunk, int32_t offset) {
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

/**
 * @brief Disassembles an instruction with a one-byte constant index operand.
 * @param name The name of the instruction.
 * @param chunk The chunk containing the instruction.
 * @param offset The byte offset of the instruction.
 * @return int32_t The offset of the next instruction.
 */
static int constantInstruction(const char* name, Chunk* chunk, int32_t offset) {
    uint8_t index = chunk->code[offset + 1];
    printf("%-16s %4d '", name, index);
    printValue(chunk->constants.values[index]);
    printf("'\n");
    return offset + 2;  // OP_CONSTANT has two bytes, one for the opcode and one
                        // for the operand (constant's index)
}

/**
 * @brief Disassembles an instruction with a two-byte jump offset operand.
 * @param name The name of the instruction.
 * @param sign The sign for the jump offset (+1 for forward, -1 for backward).
 * @param chunk The chunk containing the instruction.
 * @param offset The byte offset of the instruction.
 * @return int32_t The offset of the next instruction.
 */
static int32_t jumpInstruction(const char* name, int32_t sign, Chunk* chunk,
                               int32_t offset) {
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

/**
 * @brief Disassembles an invoke instruction.
 * @param name The name of the instruction.
 * @param chunk The chunk containing the instruction.
 * @param offset The byte offset of the instruction.
 * @return int32_t The offset of the next instruction.
 */
static int32_t invokeInstruction(const char* name, Chunk* chunk,
                                 int32_t offset) {
    uint8_t constant = chunk->code[offset + 1];
    uint8_t argCount = chunk->code[offset + 2];
    printf("%-16s (%d args) %4d '", name, argCount, constant);
    printValue(chunk->constants.values[constant]);
    printf("'\n");
    return offset + 3;
}