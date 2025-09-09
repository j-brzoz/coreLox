#ifndef corelox_table_h
#define corelox_table_h

#include "common.h"
#include "value.h"

typedef struct {
    ObjectString* key;
    Value value;
} Entry;

typedef struct {
    int32_t count;
    int32_t capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, ObjectString* key, Value* value);
bool tableSet(Table* table, ObjectString* key, Value value);
bool tableDelete(Table* table, ObjectString* key);
void tableAddAll(Table* from, Table* to);
ObjectString* tableFindString(Table* table, const char* chars, int32_t length, uint32_t hash);

#endif