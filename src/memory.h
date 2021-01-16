#ifndef memory
#define memory
#include "qw_common.h"

#define GROW_CAPACITY(cap) ((cap) < 8 ? 8 : (cap)*2)
#define GROW_ARRAY(type, arr, old_cap, cap) (type*)reallocate(arr, sizeof(type) * old_cap, sizeof(type) * cap)
#define FREE_ARRAY(type, arr, old_cap) reallocate(arr, sizeof(type) * old_cap, 0)
void* reallocate(void* pointer, isize old_size, isize new_size);
#define ALLOCATE(type, length) (type*)reallocate(NULL, 0, sizeof(type) * (length))

#endif