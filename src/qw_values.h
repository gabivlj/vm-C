#ifndef qw_values
#define qw_values

#include "qw_common.h"

typedef double Value;

typedef struct {
  u32 capacity;
  u32 count;
  Value* values;
} ValueArray;

void init_value_array(ValueArray* array);
void push_value(ValueArray* array, Value value);
void free_value_array(ValueArray* array);
void print_value(Value value);

#endif