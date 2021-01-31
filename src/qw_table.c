#include "qw_table.h"

#include <string.h>

#include "memory.h"
#include "qw_object.h"
#include "qw_values.h"

void init_table(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void free_table(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  init_table(table);
}

#define TABLE_MAX_LOAD 0.75

static Entry* find_entry(Entry* entries, u32 capacity, ObjectString* key) {
  u32 index = key->hash % capacity;
  Entry* tombstone = NULL;
  for (;;) {
    Entry* entry = &entries[index];
    // Strangely enough this does the work atm
    if (entry->key == key) {
      return entry;
    }
    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) {
        // return tombstone if we found one to reuse, this is useful when this function is called
        // by table_set
        return tombstone == NULL ? entry : tombstone;
      }
      // this is a tombstone
      if (tombstone == NULL) tombstone = entry;
    }
    index = (index + 1) % capacity;
  }
}

static void adjust_capacity(Table* table, u32 capacity) {
  Entry* entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; ++i) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }
  // reset count because we wanna remove tombstone counts
  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key == NULL) continue;
    Entry* destination = find_entry(entries, capacity, entry->key);
    destination->key = entry->key;
    ++table->count;
    destination->value = entry->value;
  }
  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

void table_copy(Table* from, Table* to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];
    if (entry->key != NULL) table_set(to, entry->key, entry->value);
  }
}

/// Returns true if it's a new key
bool table_set(Table* table, ObjectString* key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    u32 capacity = GROW_CAPACITY(table->capacity);
    adjust_capacity(table, capacity);
  }
  Entry* entry = find_entry(table->entries, table->capacity, key);
  bool new_key = entry->key == NULL;
  // We check if it's nil to _really_ make sure that it's a new filled bucket
  // If it's not null the value but the key is, then we found a tombstone,
  // which is a bucket previously filled by a value that was deleted,
  // we consider tombstones as filled buckets for the value count of the hashtable
  if (new_key && IS_NIL(entry->value)) {
    ++table->count;
  }
  entry->key = key;
  entry->value = value;
  return new_key;
}

bool table_get(Table* table, ObjectString* key, Value* value) {
  if (table->count == 0) return false;
  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;
  *(value) = entry->value;
  return true;
}

bool table_delete(Table* table, ObjectString* key) {
  if (table->count == 0) return false;
  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL) {
    return false;
  }
  entry->key = NULL;
  // Put a tombstone in the entry, so we can keep checking in table_get()
  entry->value = BOOL_VAL(true);
  return true;
}

ObjectString* table_find_string(Table* table, const char* chars, u32 length, u32 hash) {
  if (table->count == 0) return NULL;
  u32 index = hash % table->capacity;
  for (;;) {
    Entry* entry = &table->entries[index];
    // If not tombstone and a null key stop.
    if (entry->key == NULL && IS_NIL(entry->value)) {
      return NULL;
    } else if (entry->key->length == length && entry->key->hash == hash &&
               memcmp(entry->key->chars, chars, length) == 0) {
      return entry->key;
    }
    index = (index + 1) % table->capacity;
  }
}