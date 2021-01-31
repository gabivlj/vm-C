#ifndef qw_object_h
#define qw_object_h

#include "memory.h"
#include "qw_common.h"
#include "qw_values.h"

struct Object {
  ObjectType type;
  struct Object* next;
};

struct ObjectString {
  Object object;
  u64 hash;
  isize length;
  char chars[];
};

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

static inline bool is_object_type(Value value, ObjectType type) {
  return IS_OBJECT(value) && OBJECT_TYPE(value) == type;
}

void print_object(Value value);

// String utilities
#define IS_STRING(value) (is_object_type(value, OBJECT_STRING))
#define AS_STRING(value) ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString*)AS_OBJECT(value))->chars)

ObjectString* allocate_string(u32 length, u32 hash);
Object* allocate_object(ObjectType type, isize true_size);
#define ALLOCATE_OBJECT(object_type, enum_type) (object_type*)allocate_object(enum_type, sizeof(object_type))

ObjectString* copy_string(u32 length, const char* start);
u32 hash_string(char* str, u32 length);

#endif