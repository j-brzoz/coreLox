/**
 * @file memory.h
 * @brief Memory management utilities for the Clox VM.
 *
 * This header provides a set of macros for simplifying dynamic memory
 * allocation, reallocation, and deallocation. It also declares the core
 * `reallocate` function that centralizes memory operations and the public
 * interface for the garbage collector's mark phase.
 */

#ifndef corelox_memory_h
#define corelox_memory_h

#include "common.h"
#include "compiler.h"
#include "object.h"

/**
 * @brief Allocates a block of memory for a given type and count.
 *
 * @param type The type of the element of the array to be allocated.
 * @param count The number of elements in the array.
 * @return A pointer to the allocated array.
 */
#define ALLOCATE(type, count) (type*)reallocate(NULL, 0, sizeof(type) * (count))

/**
 * @brief Frees a block of memory for a given type.
 *
 * @param type The type of the block to be freed.
 * @param pointer A pointer to the allocated block.
 * @return A NULL pointer.
 */
#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

/**
 * @brief Calculates a new, larger capacity for a dynamic array.
 *
 * Follows a simple growth strategy: if the current capacity is less than 8,
 * it grows to 8; otherwise, it doubles.
 * @param capacity The current capacity.
 * @return size_t The new, larger capacity.
 */
#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

/**
 * @brief Reallocates a dynamic array to a new size.
 *
 * @param type The element type of the array.
 * @param pointer A pointer to the existing array.
 * @param oldCount The current number of elements in the array.
 * @param newCount The desired number of elements.
 * @return A pointer to the reallocated array.
 */
#define GROW_ARRAY(type, pointer, oldCount, newCount)     \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
                      sizeof(type) * (newCount))

/**
 * @brief Frees a block of memory for an array of a given type.
 *
 * @param type The type of the element of array to be freed.
 * @param pointer A pointer to the allocated block.
 * @param oldCount The current number of elements in the array.
 * @return A NULL pointer.
 */
#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

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
void* reallocate(void* pointer, size_t oldSize, size_t newSize);

//-----------------------------------------------------------------------------
//- Object Lifecycle
//-----------------------------------------------------------------------------

/**
 * @brief Frees all heap-allocated objects in the VM.
 *
 * Called during `freeVM` to clean up any remaining objects at shutdown.
 */
void freeObjects();

//-----------------------------------------------------------------------------
//- Garbage Collector: Mark Phase
//-----------------------------------------------------------------------------

/**
 * @brief Marks a value for garbage collection if it is an object.
 * @param value The value to mark.
 */
void markValue(Value value);

/**
 * @brief Marks an object and adds it to the gray stack for tracing.
 *
 * If the object is already marked, the function does nothing.
 * @param object The object to mark.
 */
void markObject(Object* object);

#endif