#include "qw_values.h"
#include "memory.h"

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
  printf("%f", value);
}