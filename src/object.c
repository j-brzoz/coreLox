/**
 * @file object.c
 * @brief Functions for creating and managing heap-allocated objects.
 *
 * This file contains the implementation for creating all types of dynamic
 * objects in Clox, such as strings, functions, classes, and instances. It also
 * handles string interning to ensure that identical string literals reuse the
 * same memory.
 */

#include "object.h"

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"

// A macro to simplify allocating an object of a specific type
#define ALLOCATE_OBJECT(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

//-----------------------------------------------------------------------------
//- Object Allocation
//-----------------------------------------------------------------------------

/**
 * @brief The base allocator for all heap objects.
 *
 * Allocates a memory block of the given size, initializes the object's header
 * (type and GC mark bit), and links it into the VM's list of all objects.
 *
 * @param size The size of the object to allocate in bytes.
 * @param type The ObjectType of the object being created.
 * @return Object* A pointer to the newly allocated object.
 */
static Object* allocateObject(size_t size, ObjectType type) {
    Object* object = (Object*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = false;
    object->next = vm.objects;  // add to linked list for garbage collection
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

/**
 * @brief Allocates a new ObjectString and interns it in the VM's string table.
 * @param chars The character buffer for the string. This function takes
 * ownership.
 * @param length The length of the string.
 * @param hash The precomputed hash of the string.
 * @return ObjectString* A pointer to the allocated string object.
 */
static ObjectString* allocateString(char* chars, int32_t length,
                                    uint32_t hash) {
    ObjectString* string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    // push/pop to guard against GC during table resizing
    push(OBJECT_VAL(string));
    // interning
    tableSet(&vm.strings, string, NIL_VAL);
    pop();

    return string;
}

//-----------------------------------------------------------------------------
//- String Hashing and Interning
//-----------------------------------------------------------------------------

/**
 * @brief Computes the hash of a string using the FNV-1a algorithm.
 * @param key The character array to hash.
 * @param length The length of the string.
 * @return uint32_t The computed hash value.
 */
static uint32_t hashString(const char* key, int32_t length) {
    uint32_t hash = 2166136261u;
    for (int32_t i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }
    return hash;
}

/**
 * @brief Creates a new ObjectString from an existing character buffer.
 *
 * This function takes ownership of the `chars` buffer and will free it if
 * an identical string is already interned.
 *
 * @param chars The character buffer (heap-allocated).
 * @param length The length of the string.
 * @return ObjectString* The interned string object.
 */
ObjectString* takeString(char* chars, int32_t length) {
    uint32_t hash = hashString(chars, length);

    // check if we already have an identical string in the vm
    ObjectString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocateString(chars, length, hash);
}

/**
 * @brief Creates a new ObjectString by copying from a character buffer.
 *
 * If an identical string is already interned, it is returned without a new
 * allocation.
 *
 * @param chars The character buffer to copy from.
 * @param length The number of characters to copy.
 * @return ObjectString* The interned string object.
 */
ObjectString* copyString(const char* chars, int32_t length) {
    uint32_t hash = hashString(chars, length);

    // check if we already have an identical string in the vm
    ObjectString* interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(heapChars, length, hash);
}

//-----------------------------------------------------------------------------
//- Object Constructors
//-----------------------------------------------------------------------------

/**
 * @brief Creates a new ObjectFunction.
 * @return ObjectFunction* The new function object.
 */
ObjectFunction* newFunction() {
    ObjectFunction* function = ALLOCATE_OBJECT(ObjectFunction, OBJECT_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

/**
 * @brief Creates a new ObjectNative for wrapping a C function.
 * @param function The C function pointer.
 * @return ObjectNative* The new native function object.
 */
ObjectNative* newNative(NativeFunction function) {
    ObjectNative* native = ALLOCATE_OBJECT(ObjectNative, OBJECT_NATIVE);
    native->function = function;
    return native;
}

/**
 * @brief Creates a new ObjectClosure from a function.
 * @param function The function to wrap in a closure.
 * @return ObjectClosure* The new closure object.
 */
ObjectClosure* newClosure(ObjectFunction* function) {
    ObjectUpvalue** upvalues = ALLOCATE(ObjectUpvalue*, function->upvalueCount);
    for (int32_t i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjectClosure* closure = ALLOCATE_OBJECT(ObjectClosure, OBJECT_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

/**
 * @brief Creates a new ObjectUpvalue.
 *
 * An upvalue represents a local variable that is "closed over" by a closure.
 * @param slot A pointer to the local variable on the VM's stack.
 * @return ObjectUpvalue* The new upvalue object.
 */
ObjectUpvalue* newUpvalue(Value* slot) {
    ObjectUpvalue* upvalue = ALLOCATE_OBJECT(ObjectUpvalue, OBJECT_UPVALUE);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

/**
 * @brief Creates a new ObjectClass.
 * @param name The name of the class.
 * @return ObjectClass* The new class object.
 */
ObjectClass* newClass(ObjectString* name) {
    ObjectClass* klass = ALLOCATE_OBJECT(ObjectClass, OBJECT_CLASS);
    initTable(&klass->methods);
    klass->name = name;
    return klass;
}

/**
 * @brief Creates a new ObjectInstance of a class.
 * @param klass The class to instantiate.
 * @return ObjectInstance* The new instance object.
 */
ObjectInstance* newInstance(ObjectClass* klass) {
    ObjectInstance* instance = ALLOCATE_OBJECT(ObjectInstance, OBJECT_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

/**
 * @brief Creates a new ObjectBoundMethod.
 *
 * A bound method is a closure that has captured a specific instance (`this`)
 * as its receiver.
 * @param receiver The instance that the method is bound to.
 * @param method The closure representing the method's code.
 * @return ObjectBoundMethod* The new bound method object.
 */
ObjectBoundMethod* newBoundMethod(Value receiver, ObjectClosure* method) {
    ObjectBoundMethod* bound =
        ALLOCATE_OBJECT(ObjectBoundMethod, OBJECT_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

//-----------------------------------------------------------------------------
//- Object Printing
//-----------------------------------------------------------------------------

/**
 * @brief Helper function to print a function object.
 */
static void printFunction(const ObjectFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

/**
 * @brief Prints a representation of a Clox object to stdout.
 * @param value The object value to print.
 */
void printObject(Value value) {
    switch (OBJECT_TYPE(value)) {
        case OBJECT_BOUND_METHOD: {
            printFunction(AS_BOUND_METHOD(value)->method->function);
            break;
        }
        case OBJECT_INSTANCE: {
            printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
            break;
        }
        case OBJECT_CLASS: {
            printf("%s", AS_CLASS(value)->name->chars);
            break;
        }
        case OBJECT_CLOSURE: {
            printFunction(AS_CLOSURE(value)->function);
            break;
        }
        case OBJECT_FUNCTION: {
            printFunction(AS_FUNCTION(value));
            break;
        }
        case OBJECT_NATIVE: {
            printf("<native fn>");
            break;
        }
        case OBJECT_STRING: {
            printf("%s", AS_CSTRING(value));
            break;
        }
        case OBJECT_UPVALUE: {  // will never actually execute -> users don't
                                // have "access" to upvalues
            printf("upvalue");
            break;
        }
    }
}