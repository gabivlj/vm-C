#ifndef memory
#define memory
#include "qw_common.h"
#include "qw_table.h"
#include "qw_values.h"

#define GROW_CAPACITY(cap) ((cap) < 8 ? 8 : (cap)*2)
#define GROW_ARRAY(type, arr, old_cap, cap) (type*)reallocate(arr, sizeof(type) * old_cap, sizeof(type) * cap)
#define FREE_ARRAY(type, arr, old_cap) reallocate(arr, sizeof(type) * old_cap, 0)
#define FREE(type, object) reallocate(object, sizeof(type), 0)
void* reallocate(void* pointer, isize old_size, isize new_size);
void mark_value(Value);
void mark_object(Object* object);
void mark_array(ValueArray*);
void mark_table(Table* table);
void table_remove_white(Table* table);


void collect_garbage();

#define ALLOCATE(type, length) (type*)reallocate(NULL, 0, sizeof(type) * (length))

void free_objects(void);

#endif
