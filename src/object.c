#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJECT(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

static Object* allocateObject(size_t size, ObjectType type) {
    Object* object = (Object*)reallocate(NULL, 0, size);
    object->type = type;
    object->isMarked = false;
    object->next = vm.objects; // add to linked list for garbage collection
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

static ObjectString* allocateString(char* chars, int32_t length, uint32_t hash) {
    ObjectString* string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    push(OBJECT_VAL(string));
    tableSet(&vm.strings, string, NIL_VAL);
    pop();

    return string;
}

static uint32_t hashString(const char* key, int32_t length) {
    uint32_t hash = 2166136261u;
    for (int32_t i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }
    return hash;
}

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

ObjectBoundMethod* newBoundMethod(Value receiver, ObjectClosure* method) {
    ObjectBoundMethod* bound = ALLOCATE_OBJECT(ObjectBoundMethod, OBJECT_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjectInstance* newInstance(ObjectClass* klass) {
    ObjectInstance* instance = ALLOCATE_OBJECT(ObjectInstance, OBJECT_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

ObjectClass* newClass(ObjectString* name) {
    ObjectClass* klass = ALLOCATE_OBJECT(ObjectClass, OBJECT_CLASS);
    initTable(&klass->methods);
    klass->name = name;
    return klass;
}

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

ObjectUpvalue* newUpvalue(Value* slot) {
    ObjectUpvalue* upvalue = ALLOCATE_OBJECT(ObjectUpvalue, OBJECT_UPVALUE);
    upvalue->closed = NIL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

ObjectFunction* newFunction() {
    ObjectFunction* function = ALLOCATE_OBJECT(ObjectFunction, OBJECT_FUNCTION);
    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

ObjectNative* newNative(NativeFunction function) {
    ObjectNative* native = ALLOCATE_OBJECT(ObjectNative, OBJECT_NATIVE);
    native->function = function;
    return native;
}

static void printFunction(ObjectFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }
    printf("<fn %s>", function->name->chars);
}

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
        case OBJECT_UPVALUE: { // will never actually execute -> users don't have "access" to upvalues
            printf("upvalue");
            break;
        }
    }
}