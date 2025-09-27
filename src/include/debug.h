/**
 * @file debug.h
 * @brief Public interface for Clox debugging utilities.
 *
 * This header declares the functions used for disassembling bytecode chunks,
 * which is a crucial tool for debugging the compiler and the VM.
 */

#ifndef corelox_debug_h
#define corelox_debug_h

#include "chunk.h"

/**
 * @brief Disassembles all instructions in a given chunk.
 * @param chunk A pointer to the chunk to disassemble.
 * @param name A name for the chunk (e.g., the function name) to be printed
 * in the header.
 */
void disassembleChunk(Chunk* chunk, const char* name);

/**
 * @brief Disassembles a single instruction at a given offset in a chunk.
 * @param chunk The chunk containing the instruction.
 * @param offset The byte offset of the instruction to disassemble.
 * @return int32_t The offset of the next instruction.
 */
int32_t disassembleInstruction(Chunk* chunk, int32_t offset);

#endif