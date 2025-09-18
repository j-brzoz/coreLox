#ifndef corelox_compiler_h
#define corelox_compiler_h

#include "vm.h"
#include "object.h"

ObjectFunction* compile(const char* source);
void markCompilerRoots();

#endif