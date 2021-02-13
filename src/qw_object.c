
#include "qw_object.h"

#include "memory.h"
#include "qw_vm.h"

Object* allocate_object(ObjectType type, isize true_size) {
  Object* object = ALLOCATE(Object, true_size);
#ifdef DEBUG_LOG_GC
  printf("%p allocate %ld for %d\n", object, true_size, type);
#endif
  object->type = type;
  object->next = vm.objects;
  object->is_marked = false;
  vm.objects = object;
  return object;
}

// will allocate length + 1 (for \0) + sizeof(ObjectString), put initial hash, or change it later.
ObjectString* allocate_string(u32 length, u32 hash) {
  ObjectString* string =
      (ObjectString*)allocate_object(OBJECT_STRING, sizeof(char) * (length + 1) + sizeof(ObjectString));
  string->length = length;
  string->hash = hash;
  return string;
}

u32 hash_string(char* str, u32 length) {
  u32 hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (u32)str[i];
    hash *= 16777619;
  }
  return hash;
}

ObjectString* copy_string(u32 length, const char* start) {
  u32 hash = hash_string((char*)start, length);
  ObjectString* internal_string = table_find_string(&vm.strings, start, length, hash);
  if (internal_string != NULL) {
    return internal_string;
  }
  ObjectString* string = allocate_string(length, hash);
  for (int i = 0; i < length; i++) {
    string->chars[i] = *(start + i);
  }
  string->chars[length] = 0;
  string->hash = hash;
  push(OBJECT_VAL(string));
  table_set(&vm.strings, string, NIL_VAL);
  pop();
  return string;
}

static void print_function(ObjectFunction* fn) {
  if (fn->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<function %s [%zu]>", fn->name->chars, (isize)fn);
}

void print_object(Value value) {
  switch (OBJECT_TYPE(value)) {
    case OBJECT_INSTANCE: {
      ObjectInstance* instance = AS_INSTANCE(value);
      printf("<%s instance %p>", instance->klass->name->chars, instance);
      break;
    }
    case OBJECT_CLASS: {
      ObjectClass* klass = AS_CLASS(value);
      printf("<constructor %s %p>", klass->name->chars, value.as.object);
      break;
    }
    case OBJECT_CLOSURE: {
      printf("!CLOSURE ");
      print_function(((ObjectClosure*)value.as.object)->function);
      break;
    }
    case OBJECT_STRING: {
      printf("`%s`", AS_CSTRING(value));
      break;
    }
    case OBJECT_FUNCTION: {
      print_function((ObjectFunction*)value.as.object);
      break;
    }
    case OBJECT_NATIVE: {
      printf("<native_function [%zu]>\n", (isize)((ObjectNative*)value.as.object)->function);
      break;
    }
    case OBJECT_UPVALUE: {
      printf("upvalue");
      break;
    }
    default: {
      printf("[print_object WARNING] COULDN'T PRINT OBJECT OF TYPE: %d\n", value.as.object->type);
      return;
    }
  }
}

bool is_truthy(Value* obj) {
  if (obj->type == VAL_BOOL) {
    return obj->as.boolean;
  }
  if (obj->type == VAL_NIL) return false;
  if (obj->type == VAL_NUMBER) return obj->as.number > 0.0;
  // TODO check string
  if (obj->type == VAL_OBJECT) return true;
  return false;
}

ObjectFunction* new_function() {
  ObjectFunction* function = ALLOCATE_OBJECT(ObjectFunction, OBJECT_FUNCTION);
  function->name = NULL;
  function->number_of_parameters = 0;
  function->upvalue_count = 0;
  function->global_array = NULL;
  init_chunk(&function->chunk);
  return function;
}

ObjectNative* new_native_function(NativeFn callback) {
  ObjectNative* native = ALLOCATE_OBJECT(ObjectNative, OBJECT_NATIVE);
  native->function = callback;
  return native;
}

ObjectClosure* new_closure(ObjectFunction* function) {
  ObjectUpvalue** upvalues = ALLOCATE(ObjectUpvalue*, function->upvalue_count);
  ObjectClosure* closure = ALLOCATE_OBJECT(ObjectClosure, OBJECT_CLOSURE);
  closure->function = function;
  closure->upvalues = NULL;
  closure->upvalue_count = 0;
  closure->upvalues = upvalues;
  for (u32 i = 0; i < function->upvalue_count; ++i) {
    upvalues[i] = NULL;
  }
  closure->upvalues = upvalues;
  closure->upvalue_count = function->upvalue_count;
  return closure;
}

ObjectUpvalue* new_upvalue(Value* slot) {
  ObjectUpvalue* upvalue = ALLOCATE_OBJECT(ObjectUpvalue, OBJECT_UPVALUE);
  upvalue->location = slot;
  upvalue->closed = NIL_VAL;
  upvalue->next = NULL;
  return upvalue;
}

ObjectClass* new_class(ObjectString* name) {
  ObjectClass* klass = ALLOCATE_OBJECT(ObjectClass, OBJECT_CLASS);
  klass->name = name;
  return klass;
}

ObjectInstance* new_instance(ObjectClass* klass) {
  ObjectInstance* instance = ALLOCATE_OBJECT(ObjectInstance, OBJECT_INSTANCE);
  instance->klass = klass;
  init_table(&instance->fields);
  return instance;
}