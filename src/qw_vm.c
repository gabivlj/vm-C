#include "qw_vm.h"

#include "qw_common.h"
#include "qw_compiler.h"
#include "qw_debug.h"

#define DEBUG_TRACE_EXECUTION

VM vm;

static void reset_stack() { vm.stack_top = vm.stack; }

void push(Value value) {
  *vm.stack_top = value;
  ++vm.stack_top;
}

Value pop() {
  --vm.stack_top;
  return *(vm.stack_top);
}

void init_vm() { reset_stack(); }

void free_vm() {}

static InterpretResult run() {
/// Returns the value of the current instrucction
/// and advances instruction pointer to the next byte
#define READ_BYTE() (*(vm.ip++))
/// Dispatch gets the current IP (should be an instruction)
/// and evaluates that instruction inside the dispatch table,
/// making a goto to that memory location
#define DISPATCH() goto* dispatch_table[READ_BYTE()]

/// Reads the constant from the constant array
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()]);

/// Constant long is able to contain 2 byte constants
#define READ_CONSTANT_LONG() (vm.chunk->constants.values[(READ_BYTE() << 8) | READ_BYTE()])
  static void* dispatch_table[] = {&&do_op_return, &&do_op_constant,  &&do_op_constant_long, &&do_op_negate,
                                   &&do_op_add,    &&do_op_substract, &&do_op_multiply,      &&do_op_divide};

/// BinaryOp does a binary operation on the vm
#define BINARY_OP(_op_)                                                                         \
  /* Equivalent of popping two values and making the operation and then pushing to the stack */ \
  do {                                                                                          \
    double b = pop();                                                                           \
    (*(vm.stack_top - 1)) = (*(vm.stack_top - 1)) _op_ b;                                       \
  } while (false);

#ifdef DEBUG_TRACE_EXECUTION
  /// TODO: I don't wanna copy every version of debug_trace_execution :(
  // Prints the current instruction and it's operands
  dissasemble_instruction(vm.chunk, (u32)(vm.ip - vm.chunk->code));
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
    dissasemble_instruction(vm.chunk, (u32)(vm.ip - vm.chunk->code));
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
    Value _ = pop();
#ifdef DEBUG_TRACE_EXECUTION
    print_value(_);
#endif
    return INTERPRET_OK;
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
    // Equivalent of popping a value from the stack, negating it, and then pushing it again
    *(vm.stack_top - 1) = -(*(vm.stack_top - 1));
    continue;
  }

  do_op_add : {
    BINARY_OP(+);
    continue;
  }

  do_op_substract : {
    BINARY_OP(-);
    continue;
  }

  do_op_multiply : {
    BINARY_OP(*);
    continue;
  }

  do_op_divide : {
    BINARY_OP(/);
    continue;
  }
  }
#undef READ_BYTE
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
  compile(source);
  return INTERPRET_OK;
}