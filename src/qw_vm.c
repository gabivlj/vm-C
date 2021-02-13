#include "qw_vm.h"

#include <stdarg.h>
#include <stdlib.h>

#include "memory.h"
#include "qw_common.h"
#include "qw_compiler.h"
#include "qw_debug.h"
#include "qw_object.h"

#define DEBUG_TRACE_EXECUTION

#define PEEK_STACK(distance) *(vm.stack_top - 1 - distance)

VM vm;

static void reset_stack() {
  vm.stack_top = vm.stack;
  vm.frame_count = 0;
  vm.open_upvalues = NULL;
}

void push(Value value) {
  *vm.stack_top = value;
  ++vm.stack_top;
}

Value pop() {
  --vm.stack_top;
  return *(vm.stack_top);
}
//{ var x = 3; { var z = 2; z = 2; } var c = 3; c = 4; print c; }
void init_vm() {
  reset_stack();
  vm.bytes_allocated = 0;
  vm.next_gc = 1024 * 1024;
  vm.objects = NULL;
  vm.gray_stack_gc.count = 0;
  vm.gray_stack_gc.capacity = 0;
  vm.gray_stack_gc.stack = NULL;
  vm.open_upvalues = NULL;
  vm.globals.values = NULL;
  init_table(&vm.strings);
  vm.init_string = NULL;
  vm.init_string = copy_string(4, "init");
}

void free_vm() {
  free_objects();
  free_table(&vm.strings);
  vm.init_string = NULL;
}

/// When the function/block is gonna go out of scope,
/// it needs to keep a reference to the variables that it captured,
/// so what it does is translating them from the stack to the heap
static void close_upvalues(Value* stack_top) {
  // Locations are always above the stack_top...
  while (vm.open_upvalues != NULL && vm.open_upvalues->location >= stack_top) {
    ObjectUpvalue* upvalue = vm.open_upvalues;
#ifdef DEBUG_TRACE_EXECUTION
    printf("[CLOSE_UPVALUES] CLOSING UPVALUE:");
    print_value(*upvalue->location);
    printf("\n");
#endif
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.open_upvalues = vm.open_upvalues->next;
  }
}

static ObjectUpvalue* capture_upvalue(Value* stack_pointer) {
  ObjectUpvalue* previous_upvalue = NULL;
  ObjectUpvalue* upvalue = vm.open_upvalues;

  while (upvalue != NULL && upvalue->location > stack_pointer) {
    previous_upvalue = upvalue;
    upvalue = upvalue->next;
  }

  // if we found an upvalue == to the stack_pointer, we reuse it
  if (upvalue != NULL && upvalue->location == stack_pointer) {
    return upvalue;
  }

  // if we didn't find a matching upvalue, we create one and add it in
  // the middle of the linked list
  ObjectUpvalue* created_upvalue = new_upvalue(stack_pointer);
  created_upvalue->next = upvalue;
  if (previous_upvalue == NULL) {
    vm.open_upvalues = created_upvalue;
  } else {
    previous_upvalue->next = created_upvalue;
  }
  return created_upvalue;
}

static void runtime_error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
  for (int i = vm.frame_count - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjectFunction* fn = frame->function->function;
    isize ins = frame->ip - frame->function->function->chunk.code - 1;
    fprintf(stderr, "[line %d] in ", (u32)get_line_from_chunk(&fn->chunk, (u32)ins));
    if (fn->name == NULL) {
      fprintf(stderr, "<main>\n");
    } else {
      fprintf(stderr, "%s()\n", fn->name->chars);
    }

    //    u32 line = get_line_from_chunk(&frame->function->function->chunk, ins);
  }
  reset_stack();
}

static bool call_value(Value function_stack_pointer_start, u8 arg_count) {
  if (function_stack_pointer_start.type != VAL_OBJECT) {
    runtime_error("can't call non function");
    return false;
  }
  Object* obj = function_stack_pointer_start.as.object;
  if (vm.frame_count == FRAMES_MAX) {
    runtime_error("Stackoverflow...");
    return false;
  }

  switch (obj->type) {
    case OBJECT_BOUND_METHOD: {
      ObjectBoundMethod* method = (ObjectBoundMethod*)obj;
      ObjectClosure* closure = method->method;
      if (closure->function->number_of_parameters != arg_count) {
        runtime_error("expected %d parameters, got: %d on `%s` call", closure->function->number_of_parameters,
                      arg_count, closure->function->name->chars);
        return false;
      }
#ifdef DEBUG_TRACE_EXECUTION
      printf("[CLOSURE] Called '%s'\n[CLOSURE] Adding new frame...\n",
             closure->function->name == NULL ? "main" : closure->function->name->chars);
#endif
      // vm.stack_top[-arg_count - 1] = method->this;
      CallFrame* frame = &vm.frames[vm.frame_count++];
      frame->ip = closure->function->chunk.code;
      frame->function = closure;
      frame->slots = vm.stack_top - arg_count - 1;
      // `this` context
      *(frame->slots) = method->this;
      return true;
    }
    case OBJECT_CLASS: {
      ObjectClass* class = AS_CLASS(function_stack_pointer_start);
      // `this` context
      vm.stack_top[-arg_count - 1] = OBJECT_VAL(new_instance(class));
      Value initializer;
      if (table_get(&class->methods, vm.init_string, &initializer)) {
        ObjectClosure* closure = (ObjectClosure*)initializer.as.object;
        if (closure->function->number_of_parameters != arg_count) {
          runtime_error("expected %d parameters, got: %d on `%s` call", closure->function->number_of_parameters,
                        arg_count, closure->function->name->chars);
          return false;
        }
#ifdef DEBUG_TRACE_EXECUTION
        printf("[CLOSURE] Called '%s'\n[CLOSURE] Adding new frame...\n",
               closure->function->name == NULL ? "main" : closure->function->name->chars);
#endif
        CallFrame* frame = &vm.frames[vm.frame_count++];
        frame->ip = closure->function->chunk.code;
        frame->function = closure;
        frame->slots = vm.stack_top - arg_count - 1;
        return true;
      } else if (arg_count != 0) {
        runtime_error("expected 0 parameters, got: %d on init", arg_count);
        return false;
      }
      return true;
    }
    case OBJECT_CLOSURE: {
      ObjectClosure* closure = (ObjectClosure*)obj;
      if (closure->function->number_of_parameters != arg_count) {
        runtime_error("expected %d parameters, got: %d on `%s` call", closure->function->number_of_parameters,
                      arg_count, closure->function->name->chars);
        return false;
      }
#ifdef DEBUG_TRACE_EXECUTION
      printf("[CLOSURE] Called '%s'\n[CLOSURE] Adding new frame...\n",
             closure->function->name == NULL ? "main" : closure->function->name->chars);
#endif
      CallFrame* frame = &vm.frames[vm.frame_count++];
      frame->ip = closure->function->chunk.code;
      frame->function = closure;
      frame->slots = vm.stack_top - arg_count - 1;
      return true;
    }
    case OBJECT_NATIVE: {
      ObjectNative* native = (ObjectNative*)obj;
      NativeFn fn = native->function;
      Value result = fn(arg_count, vm.stack_top - arg_count);
      vm.stack_top -= (arg_count + 1);
      push(result);
      return true;
    }
    case OBJECT_FUNCTION: {
      ObjectFunction* fn = (ObjectFunction*)obj;
      if (fn->number_of_parameters != arg_count) {
        runtime_error("expected %d parameters, got: %d on `%s` call", fn->number_of_parameters, arg_count,
                      fn->name->chars);
        return false;
      }
#ifdef DEBUG_TRACE_EXECUTION
      printf("[FUNCTION_CALL] Called '%s'\n[FUNCTION_CALL] Adding new frame...\n",
             fn->name == NULL ? "main" : fn->name->chars);
#endif
      CallFrame* frame = &vm.frames[vm.frame_count++];
      frame->ip = fn->chunk.code;
      frame->function = new_closure(fn);
      frame->slots = vm.stack_top - arg_count - 1;
      return true;
      break;
    }
    default: {
      runtime_error("can only call functions and classes");
      return false;
    }
  }
}

static bool invoke_from_class(ObjectClass* klass, ObjectString* name, u8 arg_count) {
  Value method;
  if (!table_get(&klass->methods, name, &method)) {
    runtime_error("Undefined method class '%s'.", name->chars);
    return false;
  }
  return call_value(method, arg_count);
}

static bool invoke(ObjectString* name, u8 arg_count) {
  Value this = PEEK_STACK(arg_count);
  if (!IS_INSTANCE(this)) {
    runtime_error("can't call a non-instance method");
    return false;
  }
  ObjectInstance* instance = AS_INSTANCE(this);
  Value value;
  if (table_get(&instance->fields, name, &value)) {
    vm.stack_top[-arg_count - 1] = value;
    return call_value(value, arg_count);
  }

  return invoke_from_class(instance->klass, name, arg_count);
}

static inline bool bind_method(ObjectClass* klass, ObjectString* name) {
  Value method;
  if (!table_get(&klass->methods, name, &method)) {
    return false;
  }
  ObjectBoundMethod* bound_method = new_bound_method(PEEK_STACK(0), (ObjectClosure*)method.as.object);
  // class instance pop
  pop();
  push(OBJECT_VAL(bound_method));
  return true;
}

static inline void concatenate() {
  ObjectString* right = AS_STRING(PEEK_STACK(0));
  ObjectString* left = AS_STRING(PEEK_STACK(1));
  // The hash is done while copying
  u32 hash = 2166136261u;
  ObjectString* new_string = allocate_string((u32)left->length + (u32)right->length, 0);
  push(OBJECT_VAL(new_string));
  for (int i = 0, j = 0; i < left->length; i++, j++) {
    new_string->chars[i] = left->chars[j];
    hash ^= (u32)new_string->chars[i];
    hash *= 16777619;
  }
  // start from the left->length until left->length + right->length copying each char
  for (int i = (int)left->length, j = 0; i < left->length + right->length; i++, j++) {
    new_string->chars[i] = right->chars[j];
    hash ^= (u32)new_string->chars[i];
    hash *= 16777619;
  }
  new_string->chars[new_string->length] = 0;
  new_string->hash = hash;
  // Check if we have an already existing string in the string intern so we reuse its memory address
  ObjectString* intern_string = table_find_string(&vm.strings, new_string->chars, (u32)new_string->length, hash);
  if (intern_string != NULL) {
    // pop();
    // Let the garbage collector do its thing
    // FREE_ARRAY(char, new_string, new_string->length + 1);
    new_string = intern_string;
  }
  pop();
  pop();
  pop();
  push(OBJECT_VAL(new_string));
}

static InterpretResult run() {
  CallFrame* frame = &vm.frames[vm.frame_count - 1];
//  u8* ip = frame->ip;
//  u8* initial = frame->ip;

/// Returns the value of the current instrucction
/// and advances instruction pointer to the next byte
#define READ_BYTE() (*(frame->ip++))
/// Dispatch gets the current IP (should be an instruction)
/// and evaluates that instruction inside the dispatch table,
/// making a goto to that memory location
#define DISPATCH() goto* dispatch_table[READ_BYTE()]

/// Reads the constant from the constant array
#define READ_CONSTANT() (frame->function->function->chunk.constants.values[READ_BYTE()])

/// Constant long is able to contain 2 byte constants
#define READ_CONSTANT_LONG() (frame->function->function->chunk.constants.values[(READ_BYTE() << 8) | READ_BYTE()])

#define READ_STRING() (AS_STRING(READ_CONSTANT_LONG()))

  static void* dispatch_table[] = {&&do_op_return,
                                   &&do_op_constant,
                                   &&do_op_constant_long,
                                   &&do_op_negate,
                                   &&do_op_add,
                                   &&do_op_substract,
                                   &&do_op_multiply,
                                   &&do_op_divide,
                                   &&do_op_nil,
                                   &&do_op_true,
                                   &&do_op_false,
                                   &&do_op_bang,
                                   &&do_op_equal,
                                   &&do_op_greater,
                                   &&do_op_less,
                                   &&do_op_print,
                                   &&do_op_pop,
                                   &&do_op_define_global,
                                   &&do_op_get_global,
                                   &&do_op_set_global,
                                   &&do_op_get_local,
                                   &&do_op_set_local,
                                   &&do_op_jump_if_false,
                                   &&do_op_jump,
                                   &&do_op_jump_back,
                                   &&do_push_again,
                                   &&do_op_assert,
                                   &&do_op_call,
                                   &&do_op_closure,
                                   &&do_op_get_upvalue,
                                   &&do_op_set_upvalue,
                                   &&do_op_close_upvalue,
                                   &&do_op_class,
                                   &&do_op_set_property,
                                   &&do_op_get_property,
                                   &&do_op_set_property_top_stack,
                                   &&do_op_get_property_top_stack,
                                   &&do_op_method,
                                   &&do_op_invoke};

/// BinaryOp does a binary operation on the vm
#define BINARY_OP(value_type, _op_)                                                                  \
  /* Equivalent of popping two values and making the operation and then pushing to the stack */      \
  do {                                                                                               \
    if (!IS_NUMBER(PEEK_STACK(1)) || !IS_NUMBER(PEEK_STACK(0))) {                                    \
      runtime_error("Operands %d, %d, must be numbers", (PEEK_STACK(1)).type, (PEEK_STACK(0)).type); \
      return INTERPRET_RUNTIME_ERROR;                                                                \
    }                                                                                                \
    double b = AS_NUMBER(pop());                                                                     \
    (*(vm.stack_top - 1)) = value_type(AS_NUMBER(*(vm.stack_top - 1)) _op_ b);                       \
  } while (false);

#ifdef DEBUG_TRACE_EXECUTION
  /// TODO: I don't wanna copy every version of debug_trace_execution :(
  // Prints the current instruction and it's operands
  dissasemble_instruction(&frame->function->function->chunk, (u64)(frame->ip - frame->function->function->chunk.code));

  printf("    ** STACK **     ");
  for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
    printf("[ ");
    print_value(*slot);
    printf(" ]");
  }
  printf("\n");
#endif

#ifdef DEBUG_TRACE_EXECUTION
  /// TODO: I don't wanna copy every version of debug_trace_execution :(
  // Prints the current instruction and it's operands
  // dissasemble_instruction(&frame->function->function->chunk, (u64)(frame->ip -
  // frame->function->function->chunk.code));

  printf("    ** UPVALUES **     ");
  for (u32 i = 0; i < frame->function->upvalue_count; i++) {
    printf("[ ");
    print_value(*(*(frame->function->upvalues + i))->location);
    printf(" ]");
  }
  printf("\n");
#endif

  // Goto current opcode handler
  DISPATCH();
  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    // Prints the current instruction and it's operands
    dissasemble_instruction(&frame->function->function->chunk,
                            (u32)(frame->ip - frame->function->function->chunk.code));
    printf("    ** STACK **     ");
    for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
      printf("[ ");
      print_value(*slot);
      printf(" ]");
    }
    printf("\n");
#endif

#ifdef DEBUG_TRACE_EXECUTION
    /// TODO: I don't wanna copy every version of debug_trace_execution :(
    // Prints the current instruction and it's operands
    // dissasemble_instruction(&frame->function->function->chunk, (u64)(frame->ip -
    // frame->function->function->chunk.code));

    printf("    ** UPVALUES **     ");
    for (u32 i = 0; i < frame->function->upvalue_count; i++) {
      printf("[ ");
      print_value(*(*(frame->function->upvalues + i))->location);
      printf(" ]");
    }
    printf("\n");
#endif

    // Goto current opcode handler
    DISPATCH();

  do_op_invoke : {
    ObjectString* method_name = AS_STRING(READ_CONSTANT_LONG());
    u8 arg_count = READ_BYTE();
    // vm.frame_count++;
    if (!invoke(method_name, arg_count)) {
      return INTERPRET_RUNTIME_ERROR;
    }
    if (run() == INTERPRET_RUNTIME_ERROR) {
      return INTERPRET_RUNTIME_ERROR;
    }
    if (vm.frame_count == 0) {
      return INTERPRET_OK;
    }
    continue;
  }
  do_op_method : {
    Value string = READ_CONSTANT_LONG();
    Value method = PEEK_STACK(0);
    Value class_constructor = PEEK_STACK(1);
    ObjectClass* klass = AS_CLASS(class_constructor);
    table_set(&klass->methods, AS_STRING(string), method);
    // pop method
    pop();
    continue;
  }
  do_op_set_property_top_stack : {
    //
    Value instance = PEEK_STACK(2);
    if (!IS_INSTANCE(instance)) {
      runtime_error("cannot access property on a non instance value: ");
      print_value(PEEK_STACK(2));
      return INTERPRET_RUNTIME_ERROR;
    }
    Value key = PEEK_STACK(1);
    if (!IS_STRING(key)) {
      runtime_error("cannot access property with a non string key: ");
      print_value(PEEK_STACK(1));
      return INTERPRET_RUNTIME_ERROR;
    }
    Value value = PEEK_STACK(0);
    table_set(&AS_INSTANCE(instance)->fields, AS_STRING(key), value);
    pop();
    pop();
    pop();
    push(value);
    continue;
  }

  do_op_get_property_top_stack : {
    //
    Value instance = PEEK_STACK(1);
    if (!IS_INSTANCE(instance)) {
      runtime_error("cannot access property on a non instance value: ");
      print_value(PEEK_STACK(1));
      return INTERPRET_RUNTIME_ERROR;
    }
    Value key = PEEK_STACK(0);
    if (!IS_STRING(key)) {
      runtime_error("cannot access property with a non string key: ");
      print_value(PEEK_STACK(0));
      return INTERPRET_RUNTIME_ERROR;
    }
    ObjectInstance* instance_obj = AS_INSTANCE(instance);
    Value value;
    if (!table_get(&instance_obj->fields, AS_STRING(key), &value)) {
      value = NIL_VAL;
    }
    pop();
    pop();
    push(value);
    continue;
  }

  do_op_get_property : {
    if (!IS_INSTANCE(PEEK_STACK(0))) {
      runtime_error("cannot access property on a non instance value: ");
      print_value(PEEK_STACK(0));
      return INTERPRET_RUNTIME_ERROR;
    }
    ObjectInstance* instance = AS_INSTANCE(PEEK_STACK(0));
    ObjectString* name = AS_STRING(READ_CONSTANT_LONG());
    Value v;
    if (table_get(&instance->fields, name, &v)) {
      // Pop instance
      pop();
      // Push the property value
      push(v);
      continue;
    }
    if (bind_method(instance->klass, name)) {
      continue;
    }
    pop();
    push(NIL_VAL);
    continue;
  }
  do_op_set_property : {
    //
    ObjectInstance* instance = AS_INSTANCE(PEEK_STACK(1));
    Value set = PEEK_STACK(0);
    ObjectString* name = AS_STRING(READ_CONSTANT_LONG());
    table_set(&instance->fields, name, set);
    pop();
    pop();
    push(set);
    continue;
  }
  do_op_class : {
    u16 index = (READ_BYTE() << 8) | READ_BYTE();
    ObjectString* str = (ObjectString*)frame->function->function->chunk.constants.values[index].as.object;
    push(OBJECT_VAL(new_class(str)));
    continue;
  }

  do_op_call : {
    u8 arg_count = READ_BYTE();
    if (!call_value(PEEK_STACK(arg_count), arg_count)) {
      return INTERPRET_RUNTIME_ERROR;
    }
    if (run() == INTERPRET_RUNTIME_ERROR) return INTERPRET_RUNTIME_ERROR;
    if (vm.frame_count == 0) return INTERPRET_OK;
    continue;
  }

  do_op_return : {
    Value result = pop();
    vm.frame_count--;
    if (vm.frame_count == 0) {
      // Pop the frame
      pop();
      return INTERPRET_OK;
    }
    vm.stack_top = frame->slots;
    push(result);
    // Copy all of the open values to their upvalues.
    close_upvalues(frame->slots);

    //    frame = &vm.frames[vm.frame_count - 1];
    return INTERPRET_OK;
  }

  do_op_pop : {
    pop();
    continue;
  }

  do_op_print : {
    print_value(pop());
    printf("\n");
    continue;
  }

  do_op_assert : {
    Value value = pop();
    if (!is_truthy(&value)) {
      runtime_error("[assert failed] non truthy value on assert");
      return INTERPRET_RUNTIME_ERROR;
    }
    continue;
  }

  do_op_constant : {
    Value constant = READ_CONSTANT();
    push(constant);
    continue;
  }

  do_op_constant_long : {
    Value constant = READ_CONSTANT_LONG();
    push(constant);
    continue;
  }

  do_op_negate : {
    Value* v = &PEEK_STACK(0);
    // Equivalent of popping a value from the stack, negating it, and then pushing it again
    if (!IS_NUMBER(*v)) {
      runtime_error("expected a number");
      return INTERPRET_RUNTIME_ERROR;
    }
    v->as.number = -v->as.number;
    continue;
  }

  do_op_add : {
    Value left = PEEK_STACK(1);
    Value right = PEEK_STACK(0);
    if (IS_STRING(left) && IS_STRING(right)) {
      concatenate();
    } else if (IS_NUMBER(left) && IS_NUMBER(right)) {
      right = pop();
      (&PEEK_STACK(0))->as.number = left.as.number + right.as.number;
    } else {
      runtime_error("error, operands either are strings or numbers");
    }
    continue;
  }

  do_op_substract : {
    BINARY_OP(NUMBER_VAL, -);
    continue;
  }

  do_op_multiply : {
    BINARY_OP(NUMBER_VAL, *);
    continue;
  }

  do_op_divide : {
    BINARY_OP(NUMBER_VAL, /);
    continue;
  }

  do_op_true : {
    push(BOOL_VAL(true));
    continue;
  }

  do_op_false : {
    push(BOOL_VAL(false));
    continue;
  }

  do_op_nil : {
    push(NIL_VAL);
    continue;
  }

  do_op_equal : {
    Value right = pop();
    Value* left = &PEEK_STACK(0);
    if (right.type != left->type) {
      left->type = VAL_BOOL;
      left->as.number = 0;
      left->as.boolean = false;
      continue;
    }
    if (IS_STRING(right)) {
      ObjectString* right_s = AS_STRING(right);
      ObjectString* left_s = AS_STRING(*left);
      // we can compare memory addresses because we use the technique string interning (we reuse string mem. addresses)
      left->as.boolean = (left_s == right_s) ||
                         // Put memory 8 bytes to 0
                         ((left->as.object = 0));
      left->type = VAL_BOOL;
      continue;
    }
    if (IS_BOOL(right)) {
      left->as.boolean = right.as.boolean == left->as.boolean;
      continue;
    }
    if (IS_NUMBER(right)) {
      left->type = VAL_BOOL;
      left->as.boolean = (left->as.number == right.as.number) || (left->as.number = 0);
      continue;
    }
    runtime_error("can't evaluate these types with this type");
  }

  do_op_greater : {
    BINARY_OP(BOOL_VAL, >);
    continue;
  }

  do_op_less : {
    BINARY_OP(BOOL_VAL, <);
    continue;
  }

  do_op_bang : {
    Value* top = &PEEK_STACK(0);
    if (top->type == VAL_NIL) {
      top->as.boolean = true;
    } else if (top->type == VAL_BOOL) {
      top->as.boolean = !top->as.boolean;
    } else if (top->type == VAL_NUMBER) {
      top->as.boolean = top->as.number <= 0.0001;
    } else {
      top->as.boolean = false;
    }
    top->type = VAL_BOOL;
    continue;
  }

  do_op_define_global : {
    // ...
    u16 index = (READ_BYTE() << 8) | READ_BYTE();
#ifdef DEBUG_TRACE_EXECUTION
    printf("[DEFINE_GLOBAL] DEFINED GLOBAL: %d\n", index);
    printf("[DEFINE_GLOBAL] VALUE: ");
    print_value(PEEK_STACK(0));
    printf("\n");
#endif
    // push_value(frame->function->function->chunk.constants)
    vm.frames[0].function->function->global_array->values[index] = pop();
    // table_set(&vm.globals, name, PEEK_STACK(0));
    continue;
  }

  do_op_get_global : {
    u16 index = (READ_BYTE() << 8) | READ_BYTE();
#ifdef DEBUG_TRACE_EXECUTION
    // printf("[GET_GLOBAL] GOT GLOBAL: %s\n", name->chars);
    // if (ok) {
    //   printf("[GET_GLOBAL] VALUE:");
    //   print_value(value);
    // } else
    //   printf("[GET_GLOBAL] VALUE NOT FOUND");
#endif
    if (index >= vm.frames[0].function->function->global_array->count) {
      runtime_error("undefined variable %d", index);
      return INTERPRET_RUNTIME_ERROR;
    }
    push(vm.frames[0].function->function->global_array->values[index]);
    continue;
  }

  do_op_set_global : {
    // ...
    u16 index = (READ_BYTE() << 8) | READ_BYTE();
    if (index >= frame->function->function->chunk.constants.count) {
      runtime_error("undefined variable");
      return INTERPRET_RUNTIME_ERROR;
    }
    vm.frames[0].function->function->global_array->values[index] =
        PEEK_STACK(0);  // << holy fuck, be careful, this SHOULD be popped by the
                        // variable declaration or expression_stmt Not by this operation
    continue;
  }

  do_op_get_local : {
    //
    u16 slot = (READ_BYTE() << 8) | READ_BYTE();
    push(frame->slots[slot]);
    continue;
  }
  do_op_set_local : {
    u16 slot = (READ_BYTE() << 8) | READ_BYTE();
    frame->slots[slot] = PEEK_STACK(0);
    continue;
  }

  do_op_jump_if_false : {
#ifdef DEBUG_TRACE_EXECUTION
    // TODO
#endif
    Value value = PEEK_STACK(0);
    u16 offset = ((READ_BYTE() << 8) | READ_BYTE());
    frame->ip += offset * !is_truthy(&value);
    // #if DEBUG_TRACE_EXECUTION
    //     printf("[OP_JUMP_IF_FALSE] Jumping %d -> %d by %d\n", (frame->ip - initial) - offset - 3, (frame->ip -
    //     initial),
    //            offset + 3);
    // #endif
    continue;
  }

  do_op_jump : {
    u16 offset = ((READ_BYTE() << 8) | READ_BYTE());
    frame->ip += offset;
    // #if DEBUG_TRACE_EXECUTION
    //     printf("[OP_JUMP] Jumping %d -> %d by %d\n", (frame->ip - initial) - offset - 3, (frame->ip - initial),
    //     offset + 3);
    //#endif
    continue;
  }
    // var x = 2; when x { 4 | 3 | 1-> print "really good"; 3 | 10 | 32 ->print "cool"; nothing -> print "bad"; }
  do_op_jump_back : {
    u16 offset = ((READ_BYTE() << 8) | READ_BYTE());
    frame->ip -= offset;
    continue;
  }
  do_push_again : {
    push(PEEK_STACK(0));
    continue;
  }

  do_op_close_upvalue : {
    close_upvalues(vm.stack_top - 1);
    pop();
    continue;
  }

  do_op_get_upvalue : {
    push(*frame->function->upvalues[(READ_BYTE() << 8) | READ_BYTE()]->location);
    continue;
  }

  do_op_set_upvalue : {
    u16 slot = (READ_BYTE() << 8) | READ_BYTE();
    *frame->function->upvalues[slot]->location = PEEK_STACK(0);
    continue;
  }

  // Transforms a function (in the constants array)
  // to a closure (Captures all of the variables)
  do_op_closure : {
    Value constant = READ_CONSTANT_LONG();

    ObjectFunction* function = (ObjectFunction*)constant.as.object;
    ObjectClosure* closure = new_closure(function);
    push(OBJECT_VAL(closure));
#ifdef DEBUG_TRACE_EXECUTION
    printf("[OP_CLOSURE] Capturing upvalues: %d\n", closure->upvalue_count);
#endif
    for (u32 i = 0; i < closure->upvalue_count; ++i) {
      // Check if it's just above this
      u8 is_local = READ_BYTE();
      // Index (maybe stack, maybe upvalues)
      u8 index = READ_BYTE();
#ifdef DEBUG_TRACE_EXECUTION
      printf("[OP_CLOSURE] Capturing upvalue is_local: %d, index: %d\n", is_local, index);
#endif
      if (is_local) {
        closure->upvalues[i] = capture_upvalue(index + frame->slots);
      } else {
        closure->upvalues[i] = frame->function->upvalues[index];
      }
    }
    continue;
  }
  }
#undef READ_BYTE
#undef READ_STRING
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef BINARY_OP
#undef DISPATCH
}

InterpretResult interpret(Chunk* chunk) {
  vm.chunk = chunk;
  vm.ip = vm.chunk->code;
  return run();
}

Value* stack_vm() { return vm.stack; }

InterpretResult interpret_source(const char* source) {
  ObjectFunction* obj;
  init_vm();
  if (!(obj = compile(source))) {
    return INTERPRET_COMPILER_ERROR;
  }
  vm.globals = *obj->global_array;
  vm.stack_top = vm.stack;
  push(OBJECT_VAL(obj));
  ObjectClosure* closure = new_closure(obj);
  pop();
  push(OBJECT_VAL(closure));
  call_value(OBJECT_VAL(closure), 0);
  InterpretResult ok = run();
  free_vm();
  free_value_array(obj->global_array);
  return ok;
}
