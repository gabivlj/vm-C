#ifndef qw_vm_h
#define qw_vm_h

#include "qw_chunk.h"
#include "qw_table.h"
#include "qw_values.h"

#define STACK_MAX 512

typedef struct {
  /// Stack of values
  Value stack[STACK_MAX];

  /// Instruction pointer points to the next instruction
  u8* ip;

  /// Chunk stores the instructions that the VM is running
  Chunk* chunk;

  /// Points to the memory address that is the top of the stack
  Value* stack_top;

  /// String interning, where the same strings share the same string address
  Table strings;

  /// Objects (as a Linked List) stores all the objects in the VM (so we can access them somehow when GC)
  Object* objects;
} VM;

typedef enum { INTERPRET_OK, INTERPRET_COMPILER_ERROR, INTERPRET_RUNTIME_ERROR } InterpretResult;

extern VM vm;

void init_vm();
void free_vm();
void push(Value value);
Value* stack_vm();
InterpretResult interpret_source(const char* source);

Value pop();

InterpretResult interpret(Chunk* chunk);

#endif
