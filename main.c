#include <stdio.h>
#include <stdlib.h>
#include "./src/qw_chunk.h"
#include "./src/qw_debug.h"

int main() {
  Chunk ch;
  init_chunk(&ch);
  for (int i = 0; i < 300; i++) {
    add_constant_opcode(&ch, 1.2 + i, i);
    add_constant_opcode(&ch, 1.3 + i, i);
  }
  dissasemble_chunk(&ch, "testing chunks");
  free_chunk(&ch);
}