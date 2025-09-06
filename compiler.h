#ifndef corelox_compiler_h
#define corelox_compiler_h

#include "vm.h"

bool compile(const char* source, Chunk* chunk);

#endif