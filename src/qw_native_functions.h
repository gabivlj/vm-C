#ifndef qw_native_functions_h
#define qw_native_functions_h

#include <time.h>

#include "qw_object.h"
#include "qw_values.h"

static Value clock_native(int argCount, Value* args) {
  //
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static Value push_array(int arg_count, Value* args) {
  if (arg_count != 2) {
    // TODO Create ERROR value
    return NIL_VAL;
  }
  if (!IS_ARRAY(*args)) {
    return NIL_VAL;
  }
  ObjectArray* arr = AS_ARRAY(*args);
  push_value(&arr->array, args[1]);
  return args[1];
}

static Value pop_array(int arg_count, Value* args) {
  if (arg_count != 1) {
    // TODO Create ERROR value
    return NIL_VAL;
  }
  if (!IS_ARRAY(*args)) {
    return NIL_VAL;
  }
  ObjectArray* arr = AS_ARRAY(*args);
  if (arr->array.count == 0) {
    return NIL_VAL;
  }
  arr->array.count--;
  return arr->array.values[arr->array.count];
}

static Value len_array(int arg_count, Value* args) {
  if (arg_count != 1) {
    // TODO Create ERROR value
    return NIL_VAL;
  }
  if (!IS_ARRAY(*args)) {
    return NIL_VAL;
  }
  ObjectArray* arr = AS_ARRAY(*args);
  Value v;
  v.type = VAL_NUMBER;
  v.as.number = arr->array.count;
  return v;
}

#endif