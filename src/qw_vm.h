#ifndef qw_vm_h
#define qw_vm_h

#include "qw_chunk.h"
#include "qw_values.h"

#define STACK_MAX 512

typedef struct {
  Chunk* chunk;
  u8* ip;
  Value stack[STACK_MAX];
  Value* stack_top;
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
