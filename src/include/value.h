/**
 * @file value.h
 * @brief Defines the Clox `Value` type and associated data structures.
 *
 * This file specifies how all data is represented at runtime. It includes a
 * `Value` type that can hold booleans, nil, numbers, and pointers to heap
 * objects. It features an optional, advanced optimization called "NaN boxing"
 * to store all of these types within a single 64-bit `double`.
 */

#ifndef corelox_value_h
#define corelox_value_h

#include "common.h"

// Forward declarations for object types used in Value.
typedef struct Object Object;
typedef struct ObjectString ObjectString;

#ifdef NAN_BOXING

//-- NaN Boxing Implementation --//
// This technique uses the bit patterns of IEEE 754 "Not a Number" values
// to encode non-numeric types (nil, bool, objects) into a 64-bit double.

#include <string.h>

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN ((uint64_t)0x7ffc000000000000)  // quiet NaN

#define TAG_NIL 1    // 01
#define TAG_FALSE 2  // 10
#define TAG_TRUE 3   // 11

// With NaN boxing, a Value is just a 64-bit integer.
typedef uint64_t Value;

// Macro for checking if value is of boolean type.
#define IS_BOOL(value) ((value | 1) == TRUE_VAL)
// Macro for checking if value is of nil type.
#define IS_NIL(value) ((value) == NIL_VAL)
// Macro for checking if value is of number type.
#define IS_NUMBER(value) (((value) & QNAN) != QNAN)
// Macro for checking if value is of object type.
#define IS_OBJECT(value) (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

// Macros for converting a Value to a C boolean type.
#define AS_BOOL(value) ((value) == TRUE_VAL)
// Macros for converting a Value to a C double type.
#define AS_NUMBER(value) valueToNum(value)
// Macros for converting a Value to an object type.
#define AS_OBJECT(value) ((Object*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

// Macros for converting a C type to a boolean Value.
#define BOOL_VAL(b) ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL ((Value)(uint64_t)(QNAN | TAG_TRUE))
// Macros for converting a C type to a nil Value.
#define NIL_VAL ((Value)(uint64_t)(QNAN | TAG_NIL))
// Macros for converting a C type to a number Value.
#define NUMBER_VAL(num) numToValue(num)
// Macros for converting a C type to a object Value.
#define OBJECT_VAL(obj) (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

// Helper to convert a Clox Value back to a C double.
static inline double valueToNum(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

// Helper to convert a C double to a Clox Value.
static inline Value numToValue(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else

//-- Tagged Union Implementation (when NaN boxing is disabled) --//

// An enum for the type tag in the Value struct.
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJECT,
} ValueType;

// The structure for a Clox value, using a tagged union.
typedef struct {
    ValueType type;  ///< Type of the value
    union {
        bool boolean;
        double number;
        Object* object;
    } as;  ///< Content of the value.
} Value;

// Macro for checking if value is of boolean type.
#define IS_BOOL(value) ((value).type == VAL_BOOL)
// Macro for checking if value is of nil type.
#define IS_NIL(value) ((value).type == VAL_NIL)
// Macro for checking if value is of number type.
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
// Macro for checking if value is of object type.
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

// Macros for converting a Value to a C boolean type.
#define AS_BOOL(value) ((value).as.boolean)
// Macros for converting a Value to a C double type.
#define AS_NUMBER(value) ((value).as.number)
// Macros for converting a Value to an object type.
#define AS_OBJECT(value) ((value).as.object)

// Macros for converting a C type to a boolean Value.
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
// Macros for converting a C type to a nil Value.
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
// Macros for converting a C type to a number Value.
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
// Macros for converting a C type to a object Value.
#define OBJECT_VAL(obj) ((Value){VAL_OBJECT, {.object = (Object*)obj}})

#endif

// A dynamic array for storing Clox values. Used for constant pools.
typedef struct {
    int32_t capacity;  ///< Number of all slots in the array.
    int32_t count;     ///< Number of used solts in the aray.
    Value* values;     ///< An actual array of values.
} ValueArray;

//-----------------------------------------------------------------------------
//- Value Array Implementation
//-----------------------------------------------------------------------------

/**
 * @brief Initializes a ValueArray.
 * @param array The array to initialize.
 */
void initValueArray(ValueArray* array);

/**
 * @brief Frees all memory used by a ValueArray.
 * @param array The array to free.
 */
void freeValueArray(ValueArray* array);

/**
 * @brief Appends a value to a ValueArray.
 *
 * Automatically grows the array's capacity if needed.
 * @param array The array to write to.
 * @param value The value to append.
 */
void writeValueArray(ValueArray* array, Value value);

//-----------------------------------------------------------------------------
//- Value Operations
//-----------------------------------------------------------------------------

/**
 * @brief Prints a human-readable representation of a Clox value to stdout.
 * @param value The value to print.
 */
void printValue(Value value);

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
bool valuesEqual(Value a, Value b);

#endif
