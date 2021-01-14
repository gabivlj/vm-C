
#include "memory.h"
#include <stdlib.h>

void* reallocate(void* pointer, isize old_size, isize new_size) {
  if (new_size == 0) {
    free(pointer);
    return NULL;
  }
  void* res = realloc(pointer, new_size);
  assert_or_exit(res != NULL);
  return res;
}