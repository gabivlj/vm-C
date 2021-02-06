#ifndef qw_values
#define qw_values

#include "qw_common.h"

typedef struct Object Object;

typedef enum {
  OBJECT_STRING,
  OBJECT_FUNCTION,
} ObjectType;

typedef struct ObjectString ObjectString;

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJECT,
  VAL_UNDEFINED,

  /// This means that the value type is an internal type that shouldn't be exposed to the user, only used
  /// in values of hashmaps used in the compiler.
  /// This type means that the value in the symbol table is immutable and should be used as that.
  VAL_INTERNAL_COMPILER_IMMUTABLE,

  /// This means that the value type is an internal type that shouldn't be exposed to the user, only used
  /// in values of hashmaps used in the compiler.
  /// This type means that the value in the symbol table cam be mutable and can be used as a mutable object (Can be
  /// reasigned).
  VAL_INTERNAL_COMPILER_MUTABLE,
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