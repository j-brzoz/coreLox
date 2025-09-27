/**
 * @file table.h
 * @brief Public interface for the Clox hash table implementation.
 *
 * Defines the data structures (`Entry`, `Table`) and function prototypes for a
 * hash table that maps `ObjectString*` keys to `Value` values.
 */

#ifndef corelox_table_h
#define corelox_table_h

#include "common.h"
#include "value.h"

// A single key-value entry in the hash table.
typedef struct {
    ObjectString* key;
    Value value;
} Entry;

// The main hash table structure.
typedef struct {
    int32_t count;     ///< The number of entries currently in the table.
    int32_t capacity;  ///< The total number of possible entries in the
                       ///< `entries` array.
    Entry* entries;    ///< A dynamic array of `Entry` structs.
} Table;

//-----------------------------------------------------------------------------
//- Table Lifecycle
//-----------------------------------------------------------------------------

/**
 * @brief Initializes a hash table.
 * @param table A pointer to the Table struct to initialize.
 */
void initTable(Table* table);

/**
 * @brief Frees all memory used by a hash table.
 * @param table A pointer to the Table struct to free.
 */
void freeTable(Table* table);

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
bool tableGet(Table* table, const ObjectString* key, Value* value);

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
bool tableSet(Table* table, ObjectString* key, Value value);

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
bool tableDelete(Table* table, const ObjectString* key);

/**
 * @brief Copies all entries from one hash table to another.
 *
 * Used for implementing class inheritance.
 * @param from The source table.
 * @param to The destination table.
 */
void tableAddAll(Table* from, Table* to);

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
                              uint32_t hash);

//-----------------------------------------------------------------------------
//- Garbage Collection Support
//-----------------------------------------------------------------------------

/**
 * @brief Marks all objects in a table for the garbage collector.
 * @param table The table to mark.
 */
void markTable(Table* table);

/**
 * @brief Removes entries from the string table whose keys are unmarked (white).
 *
 * This is a special GC step for the string table, which acts as a set of
 * weak references. If a string is not reachable from anywhere else, it
 * should be collected, and its entry in the intern table must be removed.
 *
 * @param table The string table.
 */
void tableRemoveWhite(Table* table);

#endif