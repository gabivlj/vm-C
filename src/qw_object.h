#ifndef qw_object_h
#define qw_object_h

#include "memory.h"
#include "qw_common.h"
#include "qw_values.h"

struct Object {
  ObjectType type;
};

struct ObjectString {
  Object object;
  isize length;
  char* chars;
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

static Object* allocate_object(ObjectType type, isize true_size) {
  Object* object = (Object*)reallocate(NULL, 0, true_size);
  object->type = type;
  return object;
}
#define ALLOCATE_OBJECT(object_type, enum_type) (object_type*)allocate_object(enum_type, sizeof(object_type))

ObjectString* copy_string(u32 length, const char* start);
#endif