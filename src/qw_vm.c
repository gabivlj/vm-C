#include "qw_vm.h"

#include <stdarg.h>

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
  vm.objects = NULL;
  init_table(&vm.strings);
  // init_value_array(&vm.globals);
}

void free_vm() {
  free_objects();
  free_table(&vm.strings);
  free_value_array(&vm.globals);
}

static void runtime_error(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
  CallFrame* frame = &vm.frames[vm.frame_count - 1];
  isize ins = frame->ip - frame->function->chunk.code - 1;
  u32 line = get_line_from_chunk(&frame->function->chunk, ins);
  reset_stack();
}

static inline void concatenate() {
  ObjectString* right = AS_STRING(pop());
  ObjectString* left = AS_STRING(pop());
  // The hash is done while copying
  u32 hash = 2166136261u;
  ObjectString* new_string = allocate_string(left->length + right->length, 0);
  for (int i = 0, j = 0; i < left->length; i++, j++) {
    new_string->chars[i] = left->chars[j];
    hash ^= (u32)new_string->chars[i];
    hash *= 16777619;
  }
  // start from the left->length until left->length + right->length copying each char
  for (int i = left->length, j = 0; i < left->length + right->length; i++, j++) {
    new_string->chars[i] = right->chars[j];
    hash ^= (u32)new_string->chars[i];
    hash *= 16777619;
  }
  new_string->chars[new_string->length] = 0;
  new_string->hash = hash;
  // Check if we have an already existing string in the string intern so we reuse its memory address
  ObjectString* intern_string = table_find_string(&vm.strings, new_string->chars, new_string->length, hash);
  if (intern_string != NULL) {
    FREE_ARRAY(char, new_string, new_string->length + 1);
    new_string = intern_string;
  }
  push(OBJECT_VAL(new_string));
}

static InterpretResult run() {
  CallFrame* frame = &vm.frames[vm.frame_count - 1];
  u8* initial = frame->ip;

/// Returns the value of the current instrucction
/// and advances instruction pointer to the next byte
#define READ_BYTE() (*(frame->ip++))
/// Dispatch gets the current IP (should be an instruction)
/// and evaluates that instruction inside the dispatch table,
/// making a goto to that memory location
#define DISPATCH() goto* dispatch_table[READ_BYTE()]

/// Reads the constant from the constant array
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])

/// Constant long is able to contain 2 byte constants
#define READ_CONSTANT_LONG() (frame->function->chunk.constants.values[(READ_BYTE() << 8) | READ_BYTE()])

#define READ_STRING() (AS_STRING(READ_CONSTANT_LONG()))

  static void* dispatch_table[] = {&&do_op_return,    &&do_op_constant,      &&do_op_constant_long, &&do_op_negate,
                                   &&do_op_add,       &&do_op_substract,     &&do_op_multiply,      &&do_op_divide,
                                   &&do_op_nil,       &&do_op_true,          &&do_op_false,         &&do_op_bang,
                                   &&do_op_equal,     &&do_op_greater,       &&do_op_less,          &&do_op_print,
                                   &&do_op_pop,       &&do_op_define_global, &&do_op_get_global,    &&do_op_set_global,
                                   &&do_op_get_local, &&do_op_set_local,     &&do_op_jump_if_false, &&do_op_jump,
                                   &&do_op_jump_back, &&do_push_again,       &&do_op_assert};

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
  dissasemble_instruction(&frame->function->chunk, (u64)(frame->ip - frame->function->chunk.code));

  printf("    ** STACK **     ");
  for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
    printf("[ ");
    print_value(*slot);
    printf(" ]");
  }
  printf("\n");
#endif

  // Goto current opcode handler
  DISPATCH();
  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    // Prints the current instruction and it's operands
    dissasemble_instruction(&frame->function->chunk, (u32)(frame->ip - frame->function->chunk.code));
    printf("    ** STACK **     ");
    for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
      printf("[ ");
      print_value(*slot);
      printf(" ]");
    }
    printf("\n");
#endif

    // Goto current opcode handler
    DISPATCH();

  do_op_return : {
    //     Value _ = pop();
    // #ifdef DEBUG_TRACE_EXECUTION
    //     print_value(_);
    // #endif
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

    top->as.boolean = top->type == VAL_NIL || (top->type == VAL_BOOL && !top->as.boolean) ||
                      (top->type == VAL_NUMBER && top->as.number > 0.000001 && !(top->as.number = 0));
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
    // push_value(frame->function->chunk.constants)
    vm.frames[0].function->global_array->values[index] = pop();
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
    if (index >= vm.frames[0].function->global_array->count) {
      runtime_error("undefined variable %d", index);
      return INTERPRET_RUNTIME_ERROR;
    }
    push(vm.frames[0].function->global_array->values[index]);
    continue;
  }

  do_op_set_global : {
    // ...
    u16 index = (READ_BYTE() << 8) | READ_BYTE();
    if (index >= frame->function->chunk.constants.count) {
      runtime_error("undefined variable");
      return INTERPRET_RUNTIME_ERROR;
    }
    vm.frames[0].function->global_array->values[index] =
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
  if (!(obj = compile(source))) {
    return INTERPRET_COMPILER_ERROR;
  }
  init_vm();
  push(OBJECT_VAL(obj));
  CallFrame* frame = &vm.frames[vm.frame_count++];
  frame->function = obj;
  frame->ip = obj->chunk.code;
  frame->slots = vm.stack;
  vm.globals = *obj->global_array;
  vm.stack_top = vm.stack;
  InterpretResult ok = run();
  return ok;
}