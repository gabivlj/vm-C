#ifndef qw_native_functions_h
#define qw_native_functions_h

#include <time.h>

#include "qw_object.h"
#include "qw_values.h"

static Value clock_native(int argCount, Value* args) {
  //
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

#endif