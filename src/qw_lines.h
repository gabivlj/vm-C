#ifndef qw_lines
#define qw_lines
#include "qw_common.h"

typedef struct {
  u32 line;
  u32 times;
} Line;

typedef struct {
  u32 capacity;
  u32 count;
  Line* lines;
} Lines;

/// Initializes lines struct
void init_lines(Lines* chunk);

/// Writes a line
void write_line(Lines* line, u32 line_n);

u32 get_line(Lines* lines, u32 op_code_pos);

/// Frees lines
void free_lines(Lines* chunk);

#endif