/**
 * @file vm.h
 * @brief Data structures and public interface for the Clox Virtual Machine.
 *
 * This file defines the core `VM` struct, which holds all the runtime state
 * of the interpreter, such as the stack, call frames, and global variables.
 * It also defines the main entry point for executing code, `interpret`.
 */

#ifndef corelox_vm_h
#define corelox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

// The maximum number of nested function calls.
#define FRAMES_MAX 64
// The maximum number of values that can be on the stack.
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

// Represents a single active function call.
typedef struct {
    ObjectClosure* closure;  ///< The closure being executed.
    uint8_t*
        ip;  ///< The instruction pointer, pointing to the next instruction.
    Value* slots;  ///< A pointer to the first stack slot used by this function.
} CallFrame;

// The main Virtual Machine struct, holding all runtime state.
typedef struct {
    CallFrame frames[FRAMES_MAX];  ///< The array of active call frames.
    int32_t frameCount;  ///< The number of currently active call frames.

    Value stack[STACK_MAX];  ///< The runtime value stack.
    Value*
        stackTop;  ///< A pointer to the element just past the top of the stack.

    Table globals;                ///< A hash table for global variables.
    Table strings;                ///< The string intern table.
    ObjectUpvalue* openUpvalues;  ///< A linked list of open upvalues.

    Object* objects;  ///< A linked list of all heap-allocated objects.

    int32_t grayCount;      ///< The number of objects in the gray stack.
    int32_t grayCapacity;   ///< The capacity of the gray stack.
    Object** grayStack;     ///< The worklist of gray objects for the GC.
    size_t bytesAllocated;  ///< Total bytes of managed memory allocated.
    size_t nextGC;          ///< The memory threshold for the next GC run.

    ObjectString* initString;  ///< A cached reference to the "init" string.
} VM;

// The possible results of an interpretation attempt.
typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

// A global instance of the VM.
extern VM vm;

//-----------------------------------------------------------------------------
//- VM Initialization and Teardown
//-----------------------------------------------------------------------------

/**
 * @brief Initializes the virtual machine.
 *
 * Sets up the stack, global tables, and defines native functions.
 */
void initVM();

/**
 * @brief Frees all resources used by the VM.
 */
void freeVM();

//-----------------------------------------------------------------------------
//- Public Interpreter Interface
//-----------------------------------------------------------------------------

/**
 * @brief Interprets a string of Clox source code.
 *
 * This function orchestrates the entire process: compiling the source code
 * into bytecode and then executing that bytecode in the VM.
 *
 * @param source The Clox source code to interpret.
 * @return InterpretResult The result of the interpretation.
 */
InterpretResult interpret(const char* source);

//-----------------------------------------------------------------------------
//- Stack Operations
//-----------------------------------------------------------------------------

/**
 * @brief Pushes a value onto the top of the stack.
 * @param value The value to push.
 */
void push(Value value);

/**
 * @brief Pops and returns the value from the top of the stack.
 * @return Value The popped value.
 */
Value pop();

#endif
