#ifndef corelox_vm_h
#define corelox_vm_h

#include "chunk.h"
#include "value.h"
#include "table.h"
#include "object.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjectClosure* closure;
    uint8_t* ip; // instruction pointer - the location of the next to be executed instruction
    Value* slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    int32_t frameCount;
    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table strings; // table storing all unique strings
    ObjectUpvalue* openUpvalues;
    Object* objects;
    int32_t grayCount;
    int32_t grayCapacity;
    Object** grayStack;
    size_t bytesAllocated;
    size_t nextGC;
    ObjectString* initString;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif