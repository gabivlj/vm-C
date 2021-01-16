#include "qw_object.h"

static ObjectString* allocate_string(char* chars, u32 length) {
  ObjectString* string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING);
  string->length = length;
  string->chars = chars;
  return string;
}

ObjectString* copy_string(u32 length, const char* start) {
  // length -1 because we wanna skip the last `"`
  char* dst_string = ALLOCATE(char, length - 1);
  // -2 because we skip the 2 `"`
  memcpy(dst_string, start + 1, length - 2);
  // fill with 0
  dst_string[length - 1] = 0;
  return allocate_string(dst_string, length - 2);
}

void print_object(Value value) {
  switch (OBJECT_TYPE(value)) {
    case OBJECT_STRING: {
      printf("%s", AS_CSTRING(value));
      break;
    }
    default: {
      return;
    }
  }
}