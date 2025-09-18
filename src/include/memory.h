#ifndef corelox_memory_h
#define corelox_memory_h

#include "common.h"
#include "object.h"
#include "compiler.h"

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)

// function for all dynamic memory management
// allocating, freeing and changing size of an existing allocation
// dependent on two size arguments
void* reallocate(void* pointer, size_t oldSize, size_t newSize);
void freeObjects();
void markObject(Object* object);
void markValue(Value value);
void collectGarbage();

#endif