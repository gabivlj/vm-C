#include "qw_debug.h"

void dissasemble_chunk(Chunk* chunk, const char* name) {
  printf("=== %s ===\n", name);
  printf("IDX  |   LINE  | OP_CODE    |     INFO\n");
  for (u32 i = 0; i < chunk->count;) {
    i = dissasemble_instruction(chunk, i);
  }
}

static u32 simple_instruction(const char* name, u32 offset) {
  printf("%s\n", name);
  return offset + 1;
}

static u32 constant_instruction(const char* name, Chunk* chunk, u32 offset) {
  u8 constant = chunk->code[offset + 1];
  printf("%-16s %4d @", name, constant);
  // Access the constant and print it
  print_value(chunk->constants.values[constant]);
  printf("\n");
  return offset + 2;
}

static u32 constant_instruction_long(const char* name, Chunk* chunk, u32 offset) {
  u16 constant = (chunk->code[offset + 1] << 8) | (chunk->code[offset + 2]);
  printf("%-16s %4d @", name, constant);
  // Access the constant and print it
  print_value(chunk->constants.values[constant]);
  printf("\n");
  return offset + 3;
}

u32 dissasemble_instruction(Chunk* chunk, u32 offset) {
  printf("%04d | ", offset);
  if (offset > 0 && get_line_from_chunk(chunk, offset) == get_line_from_chunk(chunk, offset - 1)) {
    printf("   |    | ");
  } else {
    printf("  %04d  | ", get_line_from_chunk(chunk, offset));
  }
  u8 instruction = chunk->code[offset];
  switch (instruction) {
    case OP_RETURN: {
      return simple_instruction("OP_RETURN", offset);
    }
    case OP_CONSTANT: {
      return constant_instruction("OP_CONSTANT", chunk, offset);
    }
    case OP_CONSTANT_LONG: {
      return constant_instruction_long("OP_CONSTANT_LONG", chunk, offset);
    }
    case OP_NEGATE: {
      return simple_instruction("OP_NEGATE", offset);
    }
    case OP_ADD: {
      return simple_instruction("OP_ADD", offset);
    }
    case OP_SUBTRACT: {
      return simple_instruction("OP_SUBTRACT", offset);
    }
    case OP_DIVIDE: {
      return simple_instruction("OP_DIVIDE", offset);
    }
    case OP_MULTIPLY: {
      return simple_instruction("OP_MULTIPLY", offset);
    }
    default: {
      printf("unknown opcode: %d\n", instruction);
      return offset + 1;
    }
  }
  return 0;
}