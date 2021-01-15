#ifndef qw_compiler_h
#define qw_compiler_h
#include "qw_chunk.h"
#include "qw_common.h"
#include "qw_scanner.h"

u8 compile(const char* source, Chunk* chunk);

#endif