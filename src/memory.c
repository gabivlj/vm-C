
#include "memory.h"

#include <stdlib.h>

#include "qw_compiler.h"
#include "qw_object.h"
#include "qw_vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>

#include "qw_debug.h"
#endif

void* reallocate(void* pointer, isize old_size, isize new_size) {
  // only when we allocate
  if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
    collect_garbage();
#endif
  }
  if (new_size == 0) {
    free(pointer);
    return NULL;
  }
  void* res = realloc(pointer, new_size);
  assert_or_exit(res != NULL);
  return res;
}

static void free_object(Object* object) {
#ifdef DEBUG_LOG_GC
  printf("%p free type %d\n", (void*)object, object->type);
#endif
  switch (object->type) {
    case OBJECT_STRING: {
      ObjectString* string = (ObjectString*)object;
      FREE(ObjectString, object);
      break;
    }
    case OBJECT_UPVALUE: {
      FREE(ObjectUpvalue, object);
      break;
    }
    case OBJECT_CLOSURE: {
      ObjectClosure* closure = (ObjectClosure*)object;
      FREE_ARRAY(ObjectUpvalue*, closure->upvalues, closure->upvalue_count);
      FREE(ObjectClosure, object);
      break;
    }
    case OBJECT_FUNCTION: {
      ObjectFunction* fn = (ObjectFunction*)object;
      free_chunk(&fn->chunk);
      // FREE(ObjectString, fn->name);
      FREE(ObjectFunction, object);
      break;
    }
    case OBJECT_NATIVE: {
      FREE(OBJECT_NATIVE, object);
      return;
    }
    default: {
      fprintf(stderr, "[WARNING] Couldn't free object because it's of unkown type");
    }
  }
}

void free_objects() {
  Object* object = vm.objects;
  while (object != NULL) {
    Object* curr = object;
    object = object->next;
    free_object(curr);
  }
  if (vm.gray_stack_gc.capacity != 0) {
    free(vm.gray_stack_gc.stack);
    vm.gray_stack_gc.capacity = 0;
  }
  vm.objects = NULL;
}

void mark_table(Table* table) {
  for (u32 i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    mark_object((Object*)entry->key);
    mark_value(entry->value);
  }
}

void mark_array(ValueArray* arr) {
  if (arr == NULL || arr->values == NULL) return;
  for (u32 i = 0; i < arr->count; ++i) {
    mark_value(arr->values[i]);
  }
}

void mark_object(Object* object) {
  if (object == NULL) return;
  if (object->is_marked) return;
#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void*)object);
  print_value(OBJECT_VAL(object));
  printf("\n");
#endif
  object->is_marked = true;

  if (vm.gray_stack_gc.capacity <= vm.gray_stack_gc.count) {
    vm.gray_stack_gc.capacity = GROW_CAPACITY(vm.gray_stack_gc.capacity);
    vm.gray_stack_gc.stack = (Object**)realloc(vm.gray_stack_gc.stack, sizeof(Object*) * vm.gray_stack_gc.capacity);
    assert_or_exit(vm.gray_stack_gc.stack != NULL);
  }
  // add it into the stack of "marked" objects
  // but that they still need their children to be processed
  vm.gray_stack_gc.stack[vm.gray_stack_gc.count++] = object;
}

void mark_value(Value value) {
  if (value.type != VAL_OBJECT) {
    return;
  }
  mark_object(value.as.object);
}

static void mark_roots() {
  for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
    mark_value(*slot);
  }
  mark_array(&vm.globals);
  for (u32 i = 0; i < vm.frame_count; i++) {
    mark_object((Object*)vm.frames[i].function);
  }
  for (ObjectUpvalue* upvalue = vm.open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
    mark_object((Object*)upvalue);
  }
  mark_compiler_roots();
}

static void blackend_object(Object* object) {
#ifdef DEBUG_LOG_GC
  printf("%p blacken ", (void*)object);
  print_value(OBJECT_VAL(object));
  printf("\n");
#endif
  switch (object->type) {
    case OBJECT_CLOSURE: {
      ObjectClosure* closure = (ObjectClosure*)object;
      mark_object((Object*)closure->function);
      if (closure->upvalues == NULL) break;
      for (u32 i = 0; i < closure->upvalue_count; ++i) {
        mark_object((Object*)closure->upvalues[i]);
      }
      // mark_object((Object*)closure);
      break;
    }

    case OBJECT_FUNCTION: {
      ObjectFunction* function = (ObjectFunction*)object;
      mark_object((Object*)function);
      mark_object((Object*)function->name);
      mark_array(&function->chunk.constants);
      printf(">> constants of func %p\n", (void*)object);
      break;
    }

    case OBJECT_UPVALUE: {
      // If the value is closed, it won't be on the stack, so make sure we track it
      mark_value(((ObjectUpvalue*)object)->closed);
      break;
    }

    case OBJECT_NATIVE: {
      break;
    }

    case OBJECT_STRING: {
      break;
    }
  }
}

static void trace_references() {
  while (vm.gray_stack_gc.count != 0) {
    Object* object = vm.gray_stack_gc.stack[--vm.gray_stack_gc.count];
    blackend_object(object);
  }
}

static void sweep() {
  Object* previous = NULL;
  Object* object = vm.objects;
  while (object != NULL) {
    if (object->is_marked) {
      // object->is_marked = false;
      previous = object;
      object = object->next;
    } else {
      Object* unreached = object;

#ifdef DEBUG_LOG_GC
      printf("freeing %p - ", unreached);
      print_value(OBJECT_VAL(unreached));
      printf("_");
#endif

      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.objects = object;
      }
      free_object(unreached);
    }
  }
  object = vm.objects;
  while (object != NULL) {
    object->is_marked = false;
    object = object->next;
  }
}

void table_remove_white(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key != NULL && !entry->key->object.is_marked) {
      table_delete(table, entry->key);
    }
  }
}

void collect_garbage() {
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
#endif
  mark_roots();
  trace_references();
  table_remove_white(&vm.strings);
  sweep();
#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
#endif
}
