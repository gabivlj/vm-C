#ifndef qw_table_h
#define qw_table_h

#include "qw_common.h"
#include "qw_values.h"

typedef struct {
  ObjectString* key;
  Value value;
} Entry;

typedef struct {
  u32 count;
  u32 capacity;
  Entry* entries;
} Table;

void init_table(Table* table);
void free_table(Table* table);
bool table_set(Table* table, ObjectString* key, Value value);
void table_copy(Table* from, Table* to);
bool table_get(Table* table, ObjectString* key, Value* value);
bool table_delete(Table* table, ObjectString* key);
ObjectString* table_find_string(Table* table, const char* chars, u32 length, u32 hash);

#endif