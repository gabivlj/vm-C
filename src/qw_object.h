#ifndef qw_object_h
#define qw_object_h

#include "memory.h"
#include "qw_chunk.h"
#include "qw_common.h"
#include "qw_table.h"
#include "qw_values.h"

struct Object {
  bool is_marked;
  ObjectType type;
  struct Object* next;
};

typedef Value (*NativeFn)(int arg_count, Value* args);

typedef struct {
  Object object;
  NativeFn function;
} ObjectNative;

typedef struct ObjectUpvalue {
  Object object;
  // STACK LOCATION OF THE OBJECT
  Value* location;
  struct ObjectUpvalue* next;
  // The copy of the value
  Value closed;
} ObjectUpvalue;

struct ObjectString {
  Object object;
  u64 hash;
  isize length;
  char chars[];
};

typedef struct ObjectClass {
  Object object;
  ObjectString* name;
  Table methods;
} ObjectClass;



typedef struct ObjectInstance {
  Object object;
  ObjectClass* klass;
  Table fields;
} ObjectInstance;

typedef struct {
  Object object;
  u32 number_of_parameters;
  // Compiled bytecode
  Chunk chunk;
  ObjectString* name;

  ValueArray* global_array;
  i32 upvalue_count;
} ObjectFunction;

typedef struct {
  Object object;
  ObjectFunction* function;
  ObjectUpvalue** upvalues;
  u32 upvalue_count;
} ObjectClosure;

// Instance method
typedef struct ObjectBoundMethod {
  Object obj;
  Value this;
  ObjectClosure* method;
} ObjectBoundMethod;

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

static inline bool is_object_type(Value value, ObjectType type) {
  return IS_OBJECT(value) && OBJECT_TYPE(value) == type;
}

void print_object(Value value);

// String utilities
#define IS_STRING(value) (is_object_type(value, OBJECT_STRING))
#define AS_STRING(value) ((ObjectString*)AS_OBJECT(value))
#define AS_CLASS(value) ((ObjectClass*)AS_OBJECT(value))
#define IS_CLASS(value) (is_object_type(value, OBJECT_CLASS))
#define AS_CSTRING(value) (((ObjectString*)AS_OBJECT(value))->chars)
#define IS_INSTANCE(value) (is_object_type(value, OBJECT_INSTANCE))
#define AS_INSTANCE(value) ((ObjectInstance*)AS_OBJECT(value))
#define IS_BOUND_METHOD(value) (is_object_type(value, OBJECT_BOUND_METHOD))
#define AS_BOUND_METHOD(value) ((ObjectBoundMethod*)AS_OBJECT(value))

ObjectString* allocate_string(u32 length, u32 hash);
Object* allocate_object(ObjectType type, isize true_size);
#define ALLOCATE_OBJECT(object_type, enum_type) (object_type*)allocate_object(enum_type, sizeof(object_type))
ObjectClosure* new_closure(ObjectFunction* function);
ObjectFunction* new_function(void);
ObjectUpvalue* new_upvalue(Value* slot);
ObjectNative* new_native_function(NativeFn callback);
ObjectString* copy_string(u32 length, const char* start);
ObjectClass* new_class(ObjectString* name);
ObjectInstance* new_instance(ObjectClass* klass);
ObjectBoundMethod* new_bound_method(Value klass_instance, ObjectClosure* method);
u32 hash_string(char* str, u32 length);
bool is_truthy(Value* obj);

#endif
