#include <stdlib.h>

#include "memory.h"
#include "qw_chunk.h"
#include "qw_values.h"

/// Initializes a chunk
void init_chunk(Chunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  init_lines(&chunk->lines);
  init_value_array(&chunk->constants);
}

void write_chunk(Chunk* chunk, u8 byte, u32 line) {
  write_line(&chunk->lines, line);
  if (chunk->capacity < chunk->count + 1) {
    u32 old_cap = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(old_cap);
    chunk->code = GROW_ARRAY(u8, chunk->code, old_cap, chunk->capacity);
  }
  chunk->code[chunk->count++] = byte;
}

/// Big Endian
void write_chunk_u64(Chunk* chunk, u64 bytes, u32 line) {
  write_chunk_u32(chunk, bytes >> 32, line);
  write_chunk_u32(chunk, bytes & 0xFFFFFFFF, line);
}

/// Big Endian
void write_chunk_u32(Chunk* chunk, u32 bytes, u32 line) {
  write_chunk(chunk, (bytes >> 24) & 0xFF, line);
  write_chunk(chunk, (bytes >> 16) & 0xFF, line);
  write_chunk(chunk, (bytes >> 8) & 0xFF, line);
  write_chunk(chunk, bytes & 0xFF, line);
}

/// Big Endian
void write_chunk_u16(Chunk* chunk, u16 bytes, u32 line) {
  write_chunk(chunk, bytes >> 8, line);
  write_chunk(chunk, bytes & 0xFF, line);
}

void free_chunk(Chunk* chunk) {
  FREE_ARRAY(u8, chunk->code, chunk->capacity);
  free_lines(&chunk->lines);
  free_value_array(&chunk->constants);
}

u32 add_constant(Chunk* chunk, Value value) {
  push_value(&chunk->constants, value);
  return chunk->constants.count - 1;
}

u32 get_line_from_chunk(Chunk* chunk, u32 op_code_index) {
  return get_line(&chunk->lines, op_code_index);
}

/// Adds a constant OP_CODE_LONG
u32 add_constant_opcode(Chunk* chunk, Value value, int line) {
  u32 idx = add_constant(chunk, value);
  // if it's less than one byte, just use 2 bytes
  if (idx < 256) {
    write_chunk_u16(chunk, (OP_CONSTANT << 8) | (u8)idx, line);
  } else {  // else use 3 bytes
    write_chunk(chunk, OP_CONSTANT_LONG, line);
    write_chunk_u16(chunk, (u16)idx, line);
  }
  return 1;
}