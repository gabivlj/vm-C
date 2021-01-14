#ifndef qw_debug
#define qw_debug

#include "qw_chunk.h"
#include "qw_values.h"

void dissasemble_chunk(Chunk* chunk, const char* name);
u32 dissasemble_instruction(Chunk* chunk, u32 offset);

#endif