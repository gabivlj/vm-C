#include "qw_values.h"

#include "memory.h"
#include "qw_object.h"

void init_value_array(ValueArray* array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void push_value(ValueArray* array, Value value) {
  if (array->capacity < array->count + 1) {
    u32 old_capacity = array->capacity;
    array->capacity = GROW_CAPACITY(old_capacity);
    array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
  }
  array->values[array->count++] = value;
}

void free_value_array(ValueArray* array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  init_value_array(array);
}

void print_value(Value value) {
  switch (value.type) {
    case VAL_BOOL: {
      if (value.as.boolean) {
        printf("true");
      } else {
        printf("false");
      }
      break;
    }
    case VAL_NIL: {
      printf("nil");
      break;
    }
    case VAL_NUMBER: {
      printf("%g", AS_NUMBER(value));
      break;
    }
    case VAL_OBJECT:
      print_object(value);
      break;
    default: {
      printf("COULDN'T PRINT VALUE OF TYPE: %d\n", value.type);
      return;
    }
  }
}