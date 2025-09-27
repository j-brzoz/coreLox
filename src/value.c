/**
 * @file value.c
 * @brief Defines the `Value` type and associated utilities for Clox.
 *
 * This file contains the core data representation for all Clox values. It
 * uses a technique called "NaN boxing" when enabled, which packs all possible
 * value types (nil, bool, number, and object pointers) into a single 64-bit
 * double-precision floating-point number format. It also provides an
 * implementation for `ValueArray`, a dynamic array of values.
 */

#include "value.h"

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"

//-----------------------------------------------------------------------------
//- Value Array Implementation
//-----------------------------------------------------------------------------

/**
 * @brief Initializes a ValueArray.
 * @param array The array to initialize.
 */
void initValueArray(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

/**
 * @brief Frees all memory used by a ValueArray.
 * @param array The array to free.
 */
void freeValueArray(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    initValueArray(array);
}

/**
 * @brief Appends a value to a ValueArray.
 *
 * Automatically grows the array's capacity if needed.
 * @param array The array to write to.
 * @param value The value to append.
 */
void writeValueArray(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        int32_t oldCapacity = array->capacity;
        array->capacity = GROW_CAPACITY(oldCapacity);
        array->values =
            GROW_ARRAY(Value, array->values, oldCapacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

//-----------------------------------------------------------------------------
//- Value Operations
//-----------------------------------------------------------------------------

/**
 * @brief Prints a human-readable representation of a Clox value to stdout.
 * @param value The value to print.
 */
void printValue(Value value) {
#ifdef NAN_BOXING
    if (IS_BOOL(value)) {
        printf(AS_BOOL(value) ? "true" : "false");
    } else if (IS_NIL(value)) {
        printf("nil");
    } else if (IS_NUMBER(value)) {
        printf("%g", AS_NUMBER(value));
    } else if (IS_OBJECT(value)) {
        printObject(value);
    }
#else
    switch (value.type) {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_NUMBER:
            printf("%g", AS_NUMBER(value));
            break;
        case VAL_OBJECT:
            printObject(value);
            break;
    }
#endif
}

/**
 * @brief Checks if two Clox values are equal.
 *
 * - Numbers are compared by their numeric value.
 * - `nil` is equal to `nil`.
 * - Booleans are compared by their truthiness.
 * - Objects (strings, functions, etc.) are compared by reference identity.
 *
 * @param a The first value.
 * @param b The second value.
 * @return bool True if the values are equal, false otherwise.
 */
bool valuesEqual(Value a, Value b) {
#ifdef NAN_BOXING
    // for IEEE 754, this ensures NaN != NaN
    if (IS_NUMBER(a) && IS_NUMBER(a)) {
        return AS_NUMBER(a) == AS_NUMBER(b);
    }
    return a == b;
#else
    if (a.type != b.type) return false;

    switch (a.type) {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL:
            return true;
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJECT:
            return AS_OBJECT(a) == AS_OBJECT(b);
        default:
            return false;  // unreachable
    }
#endif
}