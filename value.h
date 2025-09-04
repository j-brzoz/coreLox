#ifndef corelox_value_h
#define corelox_value_h

#include "common.h"

// abstraction of C representation
typedef double Value;

// dynamic array storing data
typedef struct {
    int32_t capacity;
    int32_t count;
    Value* values;
} ValueArray;

void initValueArray(ValueArray* array);
void freeValueArray(ValueArray* array);
void writeValueArray(ValueArray* array, Value value);
void printValue(Value value);

#endif