
#include "memory.h"

#include <stdlib.h>

#include "qw_object.h"
#include "qw_vm.h"

void* reallocate(void* pointer, isize old_size, isize new_size) {
  if (new_size == 0) {
    free(pointer);
    return NULL;
  }
  void* res = realloc(pointer, new_size);
  assert_or_exit(res != NULL);
  return res;
}

static void free_object(Object* object) {
  switch (object->type) {
    case OBJECT_STRING: {
      ObjectString* string = (ObjectString*)object;
      FREE(ObjectString, string);
      break;
    }
    case OBJECT_UPVALUE: {
      FREE(ObjectUpvalue, (ObjectUpvalue*)object);
      break;
    }
    case OBJECT_CLOSURE: {
      ObjectClosure* closure = (ObjectClosure*)object;
      FREE_ARRAY(ObjectUpvalue*, closure->upvalues, closure->upvalue_count);
      FREE(ObjectClosure, closure);
      break;
    }
    case OBJECT_FUNCTION: {
      ObjectFunction* fn = (ObjectFunction*)object;
      free_chunk(&fn->chunk);
      // FREE(ObjectString, fn->name);
      FREE(ObjectFunction, fn);
      break;
    }
    case OBJECT_NATIVE: {
      FREE(OBJECT_NATIVE, object);
      return;
    }
    default: {
      fprintf(stderr, "[WARNING] Couldn't free object because it's of unkown type");
    }
  }
}

void free_objects() {
  Object* object = vm.objects;
  while (object != NULL) {
    Object* curr = object;
    object = object->next;
    free_object(curr);
  }
}