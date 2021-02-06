#ifndef qw_vm_h
#define qw_vm_h

#include "qw_chunk.h"
#include "qw_object.h"
#include "qw_table.h"
#include "qw_values.h"

typedef struct {
  // Points to itself (access to constants)
  ObjectFunction* function;

  // Points to the instruction pointer
  u8* ip;

  // Points to the start of the stack function
  Value* slots;
} CallFrame;

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * 256)

typedef struct {
  CallFrame frames[STACK_MAX];

  u32 frame_count;

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

  ValueArray globals;

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
