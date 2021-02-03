
#include "qw_object.h"

#include "memory.h"
#include "qw_vm.h"

Object* allocate_object(ObjectType type, isize true_size) {
  Object* object = (Object*)reallocate(NULL, 0, true_size);
  object->type = type;
  object->next = vm.objects;
  vm.objects = object;
  return object;
}

// will allocate length + 1 (for \0) + sizeof(ObjectString), put initial hash, or change it later.
ObjectString* allocate_string(u32 length, u32 hash) {
  ObjectString* string =
      (ObjectString*)allocate_object(OBJECT_STRING, sizeof(char) * (length + 1) + sizeof(ObjectString));
  string->length = length;
  string->hash = hash;
  return string;
}

u32 hash_string(char* str, u32 length) {
  u32 hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (u32)str[i];
    hash *= 16777619;
  }
  return hash;
}

ObjectString* copy_string(u32 length, const char* start) {
  u32 hash = hash_string((char*)start, length);
  ObjectString* internal_string = table_find_string(&vm.strings, start, length, hash);
  if (internal_string != NULL) {
    return internal_string;
  }
  ObjectString* string = allocate_string(length, hash);
  for (int i = 0; i < length; i++) {
    string->chars[i] = *(start + i);
  }
  string->chars[length] = 0;
  string->hash = hash;
  table_set(&vm.strings, string, NIL_VAL);
  return string;
}

void print_object(Value value) {
  switch (OBJECT_TYPE(value)) {
    case OBJECT_STRING: {
      printf("`%s`", AS_CSTRING(value));
      break;
    }
    default: {
      return;
    }
  }
}

bool is_truthy(Value* obj) {
  if (obj->type == VAL_BOOL) {
    return obj->as.boolean;
  }
  if (obj->type == VAL_NIL) return false;
  if (obj->type == VAL_NUMBER) return obj->as.number > 0.0;
  // TODO check string
  if (obj->type == VAL_OBJECT) return true;
  return false;
}