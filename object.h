#ifndef corelox_object_h
#define corelox_object_h

#include "common.h"
#include "value.h"

#define OBJECT_TYPE(value)  (AS_OBJECT(value)->type)

#define IS_STRING(value)    isObjectType(value, OBJECT_STRING)

#define AS_STRING(value)    ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value)   (((ObjectString*)AS_OBJECT(value))->chars)

typedef enum {
    OBJECT_STRING,
} ObjectType;

struct Object {
    ObjectType type;
};

struct ObjectString {
    Object object;
    int32_t length;
    char* chars;
};

ObjectString* takeString(char* chars, int32_t length);
ObjectString* copyString(const char* chars, int32_t length);
void printObject(Value value);

static inline bool isObjectType(Value value, ObjectType type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif