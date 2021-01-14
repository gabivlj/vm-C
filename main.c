#include <stdio.h>
#include <stdlib.h>
#include "./src/qw_chunk.h"
#include "./src/qw_debug.h"
#include "./src/qw_vm.h"

int main() {
  initVM();
  Chunk ch;
  init_chunk(&ch);
  add_constant_opcode(&ch, 1.2, 1);
  write_chunk(&ch, OP_RETURN, 1);
  interpret(&ch);
  dissasemble_chunk(&ch, "testing chunks");
  free_chunk(&ch);
  freeVM();
}