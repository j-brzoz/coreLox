/**
 * @file memory.c
 * @brief Memory management and garbage collection for the Clox VM.
 *
 * This file centralizes all dynamic memory operations. It provides a
 * `reallocate` function that is used for all allocations, deallocations, and
 * resizing of memory blocks. It also implements a mark-sweep garbage
 * collector to automatically manage the lifecycle of heap-allocated objects.
 */

#include "memory.h"

#include <stdlib.h>

#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>

#include "debug.h"
#endif

// factor by which the heap grows
#define GC_HEAP_GROW_FACTOR 2

// forward declaration
static void collectGarbage();

//-----------------------------------------------------------------------------
//- Memory Allocation
//-----------------------------------------------------------------------------

/**
 * @brief Central function for all dynamic memory management.
 *
 * This function handles allocating new memory, freeing memory, and changing
 * the size of an existing allocation. It also tracks the total number of
 * bytes allocated and triggers garbage collection when necessary.
 *
 * @param pointer The existing memory block to modify, or NULL to allocate new.
 * @param oldSize The current size of the block.
 * @param newSize The desired new size. If 0, the block is freed.
 * @return void* A pointer to the newly allocated or resized memory block.
 */

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
    vm.bytesAllocated += newSize - oldSize;

    if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif

        if (vm.bytesAllocated > vm.nextGC) {
            collectGarbage();
        }
    }

    if (newSize == 0) {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

//-----------------------------------------------------------------------------
//- Object Lifecycle
//-----------------------------------------------------------------------------

/**
 * @brief Frees the memory for a single heap-allocated object.
 *
 * This function dispatches to the correct deallocation logic based on the
 * object's type.
 * @param object The object to free.
 */
static void freeObject(Object* object) {
#ifdef DEBUG_LOG_GC
    printf("%p free ", (void*)object);
    printValue(OBJECT_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case OBJECT_BOUND_METHOD: {
            FREE(OBJECT_BOUND_METHOD, object);
            break;
        }
        case OBJECT_INSTANCE: {
            ObjectInstance* instance = (ObjectInstance*)object;
            freeTable(&instance->fields);
            FREE(ObjectInstance, object);
            break;
        }
        case OBJECT_CLASS: {
            ObjectClass* klass = (ObjectClass*)object;
            freeTable(&klass->methods);
            FREE(ObjectClass, object);
            break;
        }
        case OBJECT_CLOSURE: {
            ObjectClosure* closure = (ObjectClosure*)object;
            FREE_ARRAY(ObjectUpvalue*, closure->upvalues,
                       closure->upvalueCount);
            FREE(ObjectClosure, object);
            break;
        }
        case OBJECT_FUNCTION: {
            ObjectFunction* function = (ObjectFunction*)object;
            freeChunk(&function->chunk);
            FREE(ObjectFunction, object);
            break;
        }
        case OBJECT_NATIVE: {
            FREE(ObjectNative, object);
            break;
        }
        case OBJECT_STRING: {
            ObjectString* string = (ObjectString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjectString, object);
            break;
        }
        case OBJECT_UPVALUE: {
            FREE(ObjectUpvalue, object);
            break;
        }
    }
}

/**
 * @brief Frees all heap-allocated objects in the VM.
 *
 * Called during `freeVM` to clean up any remaining objects at shutdown.
 */
void freeObjects() {
    Object* object = vm.objects;
    while (object != NULL) {
        Object* next = object->next;
        freeObject(object);
        object = next;
    }
    free(vm.grayStack);
}

//-----------------------------------------------------------------------------
//- Garbage Collector: Mark Phase
//-----------------------------------------------------------------------------

/**
 * @brief Marks a value for garbage collection if it is an object.
 * @param value The value to mark.
 */
void markValue(Value value) {
    if (!IS_OBJECT(value)) return;
    markObject(AS_OBJECT(value));
}

/**
 * @brief Marks an object and adds it to the gray stack for tracing.
 *
 * If the object is already marked, the function does nothing.
 * @param object The object to mark.
 */
void markObject(Object* object) {
    if (object == NULL) return;
    if (object->isMarked) return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJECT_VAL(object));
    printf("\n");
#endif

    object->isMarked = true;

    // worklist for object graph traversal (tri-color abstraction)
    // white = node not reached -> just added to the stack
    // gray = node reached, but its children not reached -> on the stack
    // black = node and its children reached -> isMarked = true and of the stack
    if (vm.grayCapacity < vm.grayCount + 1) {
        vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
        vm.grayStack =
            (Object**)realloc(vm.grayStack, sizeof(Object*) * vm.grayCapacity);
        if (vm.grayStack == NULL) exit(1);
    }
    vm.grayStack[vm.grayCount++] = object;
}

/**
 * @brief Helper to mark all values in a ValueArray.
 */
static void markArray(const ValueArray* array) {
    for (int32_t i = 0; i < array->count; i++) {
        markValue(array->values[i]);
    }
}

/**
 * @brief Starts the mark phase by identifying and marking all root objects.
 *
 * Roots are objects that are directly accessible by the VM without going
 * through another object.
 */
static void markRoots() {
    // mark values sitting on the stack
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
        markValue(*slot);
    }

    // mark closures pointed by CallFrames
    for (int32_t i = 0; i < vm.frameCount; i++) {
        markObject((Object*)vm.frames[i].closure);
    }

    // mark upvalues
    for (ObjectUpvalue* upvalue = vm.openUpvalues; upvalue != NULL;
         upvalue = upvalue->next) {
        markObject((Object*)upvalue);
    }

    // mark global variables
    markTable(&vm.globals);

    // mark memory allocated by compiler
    markCompilerRoots();

    // string for class initilizer
    markObject((Object*)vm.initString);
}

/**
 * @brief Traces through an object's fields and marks all reachable objects.
 *
 * This process is called "blackening" an object in the tri-color abstraction.
 * @param object The object to trace.
 */
static void blackenObject(Object* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJECT_VAL(object));
    printf("\n");
#endif

    switch (object->type) {
        case OBJECT_BOUND_METHOD: {
            ObjectBoundMethod* bound = (ObjectBoundMethod*)object;
            markValue(bound->receiver);
            markObject((Object*)bound->method);
            break;
        }
        case OBJECT_INSTANCE: {
            ObjectInstance* instance = (ObjectInstance*)object;
            markObject((Object*)instance->klass);
            markTable(&instance->fields);
            break;
        }
        case OBJECT_CLASS: {
            ObjectClass* klass = (ObjectClass*)object;
            markObject((Object*)klass->name);
            markTable(&klass->methods);
            break;
        }
        case OBJECT_CLOSURE: {
            ObjectClosure* closure = (ObjectClosure*)object;
            markObject((Object*)closure->function);
            for (int32_t i = 0; i < closure->upvalueCount; i++) {
                markObject((Object*)closure->upvalues[i]);
            }
            break;
        }
        case OBJECT_UPVALUE: {
            markValue(((ObjectUpvalue*)object)->closed);
            break;
        }
        case OBJECT_FUNCTION: {
            ObjectFunction* function = (ObjectFunction*)object;
            markObject((Object*)function->name);
            markArray(&function->chunk.constants);
            break;
        }
        // no outgoing references
        case OBJECT_NATIVE:
        case OBJECT_STRING:
            break;
    }
}

/**
 * @brief Performs the tracing phase of the GC.
 *
 * It repeatedly takes an object from the gray stack, blackens it (marks all
 * objects it refers to), and continues until the gray stack is empty.
 */
static void traceReferences() {
    while (vm.grayCount > 0) {
        Object* object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

//-----------------------------------------------------------------------------
//- Garbage Collector: Sweep Phase
//-----------------------------------------------------------------------------

/**
 * @brief The sweep phase of the GC.
 *
 * Iterates through the linked list of all allocated objects. Any object that
 * was not marked during the mark phase is unreachable and is freed.
 */
static void sweep() {
    Object* previous = NULL;
    Object* object = vm.objects;

    // delete all unmarked objects
    while (object != NULL) {
        if (object->isMarked) {
            object->isMarked = false;
            previous = object;
            object = object->next;
        } else {
            Object* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }
            freeObject(unreached);
        }
    }
}

//-----------------------------------------------------------------------------
//- Main Garbage Collector Function
//-----------------------------------------------------------------------------

/**
 * @brief Runs a full garbage collection cycle.
 */
static void collectGarbage() {
#ifdef DEBUG_LOG_GC
    printf("-- GC begin\n");
    size_t before = vm.bytesAllocated;
#endif

    markRoots();
    traceReferences();
    // string table must be handled specially to remove weak references to
    // dead strings, preventing dangling pointers
    tableRemoveWhite(&vm.strings);

    sweep();

    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- GC end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm.bytesAllocated, before, vm.bytesAllocated, vm.nextGC);
#endif
}