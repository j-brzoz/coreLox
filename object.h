#ifndef corelox_object_h
#define corelox_object_h

#include "common.h"
#include "value.h"
#include "chunk.h"

#define OBJECT_TYPE(value)  (AS_OBJECT(value)->type)

#define IS_CLOSURE(value)   isObjectType(value, OBJECT_CLOSURE)
#define IS_FUNCTION(value)  isObjectType(value, OBJECT_FUNCTION)
#define IS_NATIVE(value)    isObjectType(value, OBJECT_NATIVE)
#define IS_STRING(value)    isObjectType(value, OBJECT_STRING)

#define AS_CLOSURE(value)   ((ObjectClosure*)AS_OBJECT(value))
#define AS_FUNCTION(value)  ((ObjectFunction*)AS_OBJECT(value))
#define AS_NATIVE(value)    (((ObjectNative*)AS_OBJECT(value))->function);
#define AS_STRING(value)    ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value)   (((ObjectString*)AS_OBJECT(value))->chars)

typedef enum {
    OBJECT_CLOSURE,
    OBJECT_UPVALUE,
    OBJECT_FUNCTION,
    OBJECT_NATIVE,
    OBJECT_STRING,
} ObjectType;

struct Object {
    ObjectType type;
    bool isMarked;
    struct Object* next; // pointer to the next allocated object for garbage collection
};

struct ObjectString {
    Object object;
    int32_t length;
    char* chars;
    uint32_t hash;
};

typedef struct ObjectUpvalue {
    Object object;
    Value* location;
    Value closed;
    struct ObjectUpvalue* next;
} ObjectUpvalue;


typedef struct {
    Object object;
    int32_t arity;
    int32_t upvalueCount;
    Chunk chunk;
    ObjectString* name;
} ObjectFunction;

typedef Value (*NativeFunction)(int32_t argCount, Value* args);

typedef struct {
    Object object;
    NativeFunction function;
} ObjectNative;

typedef struct {
    Object object;
    ObjectFunction* function;
    ObjectUpvalue** upvalues;
    int32_t upvalueCount;
} ObjectClosure;

ObjectClosure* newClosure(ObjectFunction* function);
ObjectUpvalue* newUpvalue(Value* slot);
ObjectFunction* newFunction();
ObjectNative* newNative(NativeFunction function);
ObjectString* takeString(char* chars, int32_t length);
ObjectString* copyString(const char* chars, int32_t length);
void printObject(Value value);

static inline bool isObjectType(Value value, ObjectType type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif