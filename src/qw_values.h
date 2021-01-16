#ifndef qw_values
#define qw_values

#include "qw_common.h"

typedef struct Object Object;

typedef enum { OBJECT_STRING } ObjectType;

typedef struct ObjectString ObjectString;

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJECT,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Object* object;
  } as;
} Value;

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJECT(value) ((value).as.object)
#define OBJECT_VAL(obj) ((Value){VAL_OBJECT, {.object = (Object*)obj}})
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

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