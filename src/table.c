/**
 * @file table.c
 * @brief A hash table implementation for the Clox language.
 *
 * This hash table maps string keys (ObjectString*) to Clox values (Value).
 * It is used for storing global variables, class methods, and instance fields.
 * It uses open addressing with linear probing for collision resolution and
 * automatically resizes when its load factor exceeds a threshold.
 */

#include "table.h"

#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

// maximum load factor before the hash table is resized
#define TABLE_MAX_LOAD 0.75

//-----------------------------------------------------------------------------
//- Table Lifecycle
//-----------------------------------------------------------------------------

/**
 * @brief Initializes a hash table.
 * @param table A pointer to the Table struct to initialize.
 */
void initTable(Table* table) {
    table->count = 0;
    table->capacity = -1;
    table->entries = NULL;
}

/**
 * @brief Frees all memory used by a hash table.
 * @param table A pointer to the Table struct to free.
 */
void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity + 1);
    initTable(table);
}

//-----------------------------------------------------------------------------
//- Core Table Operations
//-----------------------------------------------------------------------------

/**
 * @brief Finds the entry for a given key in the hash table.
 *
 * This is the core lookup function. It can return a pointer to an existing
 * entry, an empty entry where a new key can be inserted, or a tombstone.
 *
 * @param entries The array of entries to search.
 * @param capacity The capacity mask (`capacity - 1`).
 * @param key The string key to find.
 * @return Entry* A pointer to the found entry.
 */
static Entry* findEntry(Entry* entries, int32_t capacity,
                        const ObjectString* key) {
    // We can replace that slow modulo operator with a fast subtraction followed
    // by a very fast bitwise 'and' (hash % capacity is the same as hash &
    // (capacity -1)). We can do this, because capacity is a power of two. To
    // make this even faster we change the representation of capacity to
    // capacity - 1, so we are left with only very fast bitwise 'and'.
    uint32_t index = key->hash & capacity;
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            // reuse tombstones if possible
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;  // empty entry
            } else {
                if (tombstone == NULL) tombstone = entry;  // tombstone
            }
        } else if (entry->key == key) {
            return entry;  // key
        }
        index = (index + 1) & capacity;  // linear probing
    }
}

/**
 * @brief Resizes the hash table to a new capacity.
 *
 * This involves allocating a new entry array and rehashing all existing
 * entries from the old array into the new one.
 *
 * @param table The table to resize.
 * @param capacity The new capacity mask (`new_capacity - 1`).
 */
static void adjustCapacity(Table* table, int32_t capacity) {
    Entry* entries = ALLOCATE(Entry, capacity + 1);
    for (int32_t i = 0; i <= capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    // recaluculate new entries
    for (int32_t i = 0; i <= table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;
        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity + 1);
    table->entries = entries;
    table->capacity = capacity;
}

//-----------------------------------------------------------------------------
//- Public API
//-----------------------------------------------------------------------------

/**
 * @brief Retrieves a value from the hash table by its key.
 *
 * @param table The table to search.
 * @param key The key to look for.
 * @param value A pointer where the found value will be stored.
 * @return bool True if the key was found, false otherwise.
 */
bool tableGet(Table* table, const ObjectString* key, Value* value) {
    if (table->count == 0) return false;

    const Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

/**
 * @brief Adds or updates a key-value pair in the hash table.
 *
 * If the key already exists, its value is updated. Otherwise, a new entry is
 * created.
 *
 * @param table The table to modify.
 * @param key The key to set.
 * @param value The value to associate with the key.
 * @return bool True if the key was new, false if it was an update.
 */
bool tableSet(Table* table, ObjectString* key, Value value) {
    if (table->count + 1 > (table->capacity + 1) * TABLE_MAX_LOAD) {
        int32_t capacity = GROW_CAPACITY(table->capacity + 1) - 1;
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);

    bool isNewKey = entry->key == NULL;
    // make sure we increase the count only when the entry was empty, not filled
    // and not a tombstone
    if (isNewKey && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

/**
 * @brief Deletes a key-value pair from the hash table.
 *
 * Deletion is handled by placing a "tombstone" marker in the entry, which
 * allows the linear probing chain to remain intact.
 *
 * @param table The table to modify.
 * @param key The key to delete.
 * @return bool True if the key was found and deleted, false otherwise.
 */
bool tableDelete(Table* table, const ObjectString* key) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // tombstone = null key + true value
    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}

/**
 * @brief Copies all entries from one hash table to another.
 *
 * Used for implementing class inheritance.
 * @param from The source table.
 * @param to The destination table.
 */
void tableAddAll(Table* from, Table* to) {
    for (int32_t i = 0; i <= from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

/**
 * @brief A specialized find function for the string interning table.
 *
 * It looks up a string by its characters, length, and hash, avoiding the
 * need to create an `ObjectString` first.
 *
 * @param table The string table.
 * @param chars The characters of the string to find.
 * @param length The length of the string.
 * @param hash The hash of the string.
 * @return ObjectString* The found string object, or NULL if not found.
 */
ObjectString* tableFindString(Table* table, const char* chars, int32_t length,
                              uint32_t hash) {
    if (table->count == 0) return NULL;

    // look in findEntry for explanation
    uint32_t index = hash & table->capacity;
    for (;;) {
        Entry* entry = &table->entries[index];

        if (entry->key == NULL) {
            // stop looking if we find empty non-tombstone entry
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length && entry->key->hash == hash &&
                   memcmp(entry->key->chars, chars, length) == 0) {
            // found it
            return entry->key;
        }
        index = (index + 1) & table->capacity;
    }
}

//-----------------------------------------------------------------------------
//- Garbage Collection Support
//-----------------------------------------------------------------------------

/**
 * @brief Marks all objects in a table for the garbage collector.
 * @param table The table to mark.
 */
void markTable(Table* table) {
    for (int32_t i = 0; i <= table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject((Object*)entry->key);
        markValue(entry->value);
    }
}

/**
 * @brief Removes entries from the string table whose keys are unmarked (white).
 *
 * This is a special GC step for the string table, which acts as a set of
 * weak references. If a string is not reachable from anywhere else, it
 * should be collected, and its entry in the intern table must be removed.
 *
 * @param table The string table.
 */
void tableRemoveWhite(Table* table) {
    for (int32_t i = 0; i <= table->capacity; i++) {
        const Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->object.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}
