#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static Entry* findEntry(Entry* entries, int32_t capacity, ObjectString* key) {
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;

    for(;;) {
        Entry* entry = &entries[index];
        if (entry->key == NULL) {
            // reuse tombstones if possible
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;   // empty entry
            } else {
                if (tombstone == NULL) tombstone = entry;       // tombstone
            }
        } else if (entry->key == key) {
            return entry;                                        // key
        }
        index = (index + 1) % capacity; // linear probing
    }
}

static void adjustCapacity(Table* table, int32_t capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int32_t i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;
    // recaluculate new entries
    for(int32_t i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;
        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}


bool tableGet(Table* table, ObjectString* key, Value* value) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if(entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

bool tableSet(Table* table, ObjectString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int32_t capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);

    bool isNewKey = entry->key == NULL;
    // make sure we increase the count only when the entry was empty, not filled and not a tombstone
    if (isNewKey && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, ObjectString* key) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    // tombstone = null key + true value
    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}

void tableAddAll(Table* from, Table* to) {
    for (int32_t i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjectString* tableFindString(Table* table, const char* chars, int32_t length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;
    for(;;) {
        Entry* entry = &table->entries[index];

        if (entry->key == NULL) {
            // stop looking if we find empty non-tombstone entry 
            if (IS_NIL(entry->value)) return NULL;
        } else if (entry->key->length == length &&
                    entry->key->hash == hash &&
                    memcmp(entry->key->chars, chars, length) == 0) {
            // found it
            return entry->key;
        }
        index = (index + 1) % table->capacity;
    }
}

void tableRemoveWhite(Table* table) {
    for (int32_t i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->object.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

void markTable(Table* table) {
    for (int32_t i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject((Object*)entry->key);
        markValue(entry->value);
    }
}