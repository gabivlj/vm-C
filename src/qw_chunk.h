#ifndef qw_chunk_h
#define qw_chunk_h

#include <stdio.h>

#include "qw_common.h"
#include "qw_lines.h"
#include "qw_values.h"

/// OpCode enum values. Order definition matters because in the VM we are doing a technique
/// that depends on the value of each enum
typedef enum {
  OP_RETURN,
  OP_CONSTANT,
  // 1 byte opcode, 3 bytes long data
  OP_CONSTANT_LONG,
  OP_NEGATE,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_NOT,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,

  OP_PRINT,

  OP_POP,

  OP_DEFINE_GLOBAL,
  OP_GET_GLOBAL,
  OP_SET_GLOBAL,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_JUMP_IF_FALSE,
  OP_JUMP,
  OP_JUMP_BACK,
  // Pushes again the same element to the stack
  OP_PUSH_TOP,
  OP_ASSERT
} OpCode;

/// Chunk is a sequence of bytecode
typedef struct {
  u32 count;
  u32 capacity;
  u8* code;
  /// Contain the line number for the i'th opcode
  Lines lines;
  ValueArray constants;
} Chunk;

/// Initializes a chunk
void init_chunk(Chunk* chunk);

/// Writes byte into chunk
void write_chunk(Chunk* chunk, u8 byte, u32 line);

/// Frees a chunk
void free_chunk(Chunk* chunk);

/// Adds a constant to the chunks and returns its position
u32 add_constant(Chunk* chunk, Value value);

/// Adds a constant OP_CODE_LONG
u32 add_constant_opcode(Chunk* chunk, Value value, int line);

void write_chunk_u16(Chunk* chunk, u16 bytes, u32 line);
void write_chunk_u24(Chunk* chunk, u32 bytes, u32 line);
void write_chunk_u32(Chunk* chunk, u32 bytes, u32 line);
void write_chunk_u64(Chunk* chunk, u64 bytes, u32 line);

/// Gets the line corresponding to the `op_code_index`
u32 get_line_from_chunk(Chunk* lines, u32 op_code_index);

#endif