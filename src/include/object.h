/**
 * @file object.h
 * @brief Defines all heap-allocated object types in Clox.
 *
 * This file contains the type definitions for all objects that live on the
 * heap, including their structures and the macros used to inspect and cast
 * them. All objects share a common `Object` header for polymorphism and GC
 * integration.
 */

#ifndef corelox_object_h
#define corelox_object_h

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

//-- Macros for inspecting and casting object types --//

// Gets the ObjectType from a Value that is known to be an object.
#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

// Checks if a Value is an ObjectBoundMethod.
#define IS_BOUND_METHOD(value) isObjectType(value, OBJECT_BOUND_METHOD)
// Checks if a Value is an ObjectInstance.
#define IS_INSTANCE(value) isObjectType(value, OBJECT_INSTANCE)
// Checks if a Value is an ObjectClass.
#define IS_CLASS(value) isObjectType(value, OBJECT_CLASS)
// Checks if a Value is an ObjectClosure.
#define IS_CLOSURE(value) isObjectType(value, OBJECT_CLOSURE)
// Checks if a Value is an ObjectFunction.
#define IS_FUNCTION(value) isObjectType(value, OBJECT_FUNCTION)
// Checks if a Value is an ObjectNative.
#define IS_NATIVE(value) isObjectType(value, OBJECT_NATIVE)
// Checks if a Value is an ObjectString.
#define IS_STRING(value) isObjectType(value, OBJECT_STRING)

// Casts an object Value to an ObjectBoundMethod pointer.
#define AS_BOUND_METHOD(value) ((ObjectBoundMethod*)AS_OBJECT(value))
// Casts an object Value to an ObjectInstance pointer.
#define AS_INSTANCE(value) ((ObjectInstance*)AS_OBJECT(value))
// Casts an object Value to an ObjectClass pointer.
#define AS_CLASS(value) ((ObjectClass*)AS_OBJECT(value))
// Casts an object Value to an ObjectClosure pointer.
#define AS_CLOSURE(value) ((ObjectClosure*)AS_OBJECT(value))
// Casts an object Value to an ObjectFunction pointer.
#define AS_FUNCTION(value) ((ObjectFunction*)AS_OBJECT(value))
// Casts an object Value to an ObjectNative pointer.
#define AS_NATIVE(value) (((ObjectNative*)AS_OBJECT(value))->function);
// Casts an object Value to an ObjectString pointer.
#define AS_STRING(value) ((ObjectString*)AS_OBJECT(value))
// Casts an object Value to a C-printable string.
#define AS_CSTRING(value) (((ObjectString*)AS_OBJECT(value))->chars)

// Enum representing all the different types of heap-allocated objects.
typedef enum {
    OBJECT_BOUND_METHOD,
    OBJECT_INSTANCE,
    OBJECT_CLASS,
    OBJECT_CLOSURE,
    OBJECT_UPVALUE,
    OBJECT_FUNCTION,
    OBJECT_NATIVE,
    OBJECT_STRING,
} ObjectType;

// The base struct for all heap-allocated objects.
struct Object {
    ObjectType type;      ///< The type of the object.
    bool isMarked;        ///< Flag used by the garbage collector.
    struct Object* next;  ///< Pointer to the next object in the allocated list.
};

// The object representation for strings.
struct ObjectString {
    Object object;   ///< Base object header.
    int32_t length;  ///< The number of characters in the string.
    char* chars;     ///< The character array.
    uint32_t hash;   ///< The pre-calculated hash of the string.
};

// Represents a local variable that has been "closed over" by a closure.
typedef struct ObjectUpvalue {
    Object object;    ///< Base object header.
    Value* location;  ///< Points to the variable on the stack or in `closed`.
    Value closed;     ///< Stores the value when the variable is closed.
    struct ObjectUpvalue*
        next;  ///< Points to the next upvalue in the open list.
} ObjectUpvalue;

// The raw, compiled representation of a function.
typedef struct {
    Object object;         ///< Base object header.
    int32_t arity;         ///< The number of parameters the function expects.
    int32_t upvalueCount;  ///< The number of upvalues it closes over.
    Chunk chunk;           ///< The bytecode for the function.
    ObjectString* name;    ///< The name of the function.
} ObjectFunction;

// A C function pointer type for native functions.
typedef Value (*NativeFunction)(const int32_t argCount, const Value* args);

// A wrapper object for a native C function.
typedef struct {
    Object object;            ///< Base object header.
    NativeFunction function;  ///< The C function pointer.
} ObjectNative;

// A runtime representation of a function, bundling an ObjectFunction with its
// captured upvalues.
typedef struct {
    Object object;             ///< Base object header.
    ObjectFunction* function;  ///< The raw function being wrapped.
    ObjectUpvalue** upvalues;  ///< Array of pointers to its captured upvalues.
    int32_t upvalueCount;      ///< The number of upvalues.
} ObjectClosure;

// The runtime representation of a class.
typedef struct {
    Object object;       ///< Base object header.
    ObjectString* name;  ///< The name of the class.
    Table methods;       ///< A hash table of the class's methods.
} ObjectClass;

// An instance of a class.
typedef struct {
    Object object;       ///< Base object header.
    ObjectClass* klass;  ///< The class of which this is an instance.
    Table fields;        ///< A hash table of the instance's fields.
} ObjectInstance;

// A pairing of a method closure with the instance it's being called on
// (`this`).
typedef struct {
    Object object;          ///< Base object header.
    Value receiver;         ///< The instance (`this`).
    ObjectClosure* method;  ///< The method closure.
} ObjectBoundMethod;

//-----------------------------------------------------------------------------
//- Object Constructors
//-----------------------------------------------------------------------------

/**
 * @brief Creates a new ObjectBoundMethod.
 *
 * A bound method is a closure that has captured a specific instance (`this`)
 * as its receiver.
 * @param receiver The instance that the method is bound to.
 * @param method The closure representing the method's code.
 * @return ObjectBoundMethod* The new bound method object.
 */
ObjectBoundMethod* newBoundMethod(Value receiver, ObjectClosure* method);

/**
 * @brief Creates a new ObjectInstance of a class.
 * @param klass The class to instantiate.
 * @return ObjectInstance* The new instance object.
 */
ObjectInstance* newInstance(ObjectClass* klass);

/**
 * @brief Creates a new ObjectClass.
 * @param name The name of the class.
 * @return ObjectClass* The new class object.
 */
ObjectClass* newClass(ObjectString* name);

/**
 * @brief Creates a new ObjectClosure from a function.
 * @param function The function to wrap in a closure.
 * @return ObjectClosure* The new closure object.
 */
ObjectClosure* newClosure(ObjectFunction* function);

/**
 * @brief Creates a new ObjectUpvalue.
 *
 * An upvalue represents a local variable that is "closed over" by a closure.
 * @param slot A pointer to the local variable on the VM's stack.
 * @return ObjectUpvalue* The new upvalue object.
 */
ObjectUpvalue* newUpvalue(Value* slot);

/**
 * @brief Creates a new ObjectFunction.
 * @return ObjectFunction* The new function object.
 */
ObjectFunction* newFunction();

/**
 * @brief Creates a new ObjectNative for wrapping a C function.
 * @param function The C function pointer.
 * @return ObjectNative* The new native function object.
 */
ObjectNative* newNative(NativeFunction function);

//-----------------------------------------------------------------------------
//- String Operations
//-----------------------------------------------------------------------------

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
ObjectString* takeString(char* chars, int32_t length);

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
ObjectString* copyString(const char* chars, int32_t length);

//-----------------------------------------------------------------------------
//- Object Printing
//-----------------------------------------------------------------------------

/**
 * @brief Prints a representation of a Clox object to stdout.
 * @param value The object value to print.
 */
void printObject(Value value);

static inline bool isObjectType(Value value, ObjectType type) {
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif