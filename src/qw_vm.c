#include "qw_vm.h"
#include "qw_common.h"

VM vm;

void initVM() {}

void freeVM() {}

static InterpretResult run() {
/// Returns the value of the current instrucction
/// and advances instruction pointer to the next byte
#define READ_BYTE() (*(vm.ip++))
/// Dispatch gets the current IP (should be an instruction)
/// and evaluates that instruction inside the dispatch table,
/// making a goto to that memory location
#define DISPATCH() goto* dispatch_table[READ_BYTE()]

#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

#define READ_CONSTANT_LONG() (vm.chunk->constants.values[(READ_BYTE() << 8) | READ_BYTE()])

  static void* dispatch_table[] = {&&do_op_return, &&do_op_constant, &&do_op_constant_long};

  DISPATCH();
  for (;;) {
    u8 instruction;
  do_op_return : {
    printf("[OK]\n");
    return INTERPRET_OK;
  }
  do_op_constant : {
    Value constant = READ_CONSTANT();
    print_value(constant);
    printf("\n");
    continue;
  }
  do_op_constant_long : {
    Value constant = READ_CONSTANT_LONG();
    print_value(constant);
    printf("\n");
    continue;
  }
  }
#undef READ_BYTE
#undef READ_CONSTANT
#undef READ_CONSTANT_LONG
#undef DISPATCH
}

InterpretResult interpret(Chunk* chunk) {
  vm.chunk = chunk;
  vm.ip = vm.chunk->code;
  return run();
}
