#ifndef qw_vm_h
#define qw_vm_h

#include "qw_chunk.h"

typedef struct {
  Chunk* chunk;
  u8* ip;
} VM;

typedef enum { INTERPRET_OK, INTERPRET_COMPILER_ERROR, INTERPRET_RUNTIME_ERROR } InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret(Chunk* chunk);

#endif
