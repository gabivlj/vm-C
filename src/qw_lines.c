#include "qw_lines.h"
#include "memory.h"

/// Initializes lines struct
void init_lines(Lines* lines) {
  lines->capacity = 0;
  lines->count = 0;
  lines->lines = NULL;
}

/// Writes a line
void write_line(Lines* line, u32 line_n) {
  if (line->count > 0 && line->lines[line->count - 1].line == line_n) {
    line->lines[line->count - 1].times++;
    return;
  }
  if (line->capacity <= line->count) {
    u32 old_cap = line->capacity;
    line->capacity = GROW_CAPACITY(old_cap);
    line->lines = GROW_ARRAY(Line, line->lines, old_cap, line->capacity);
  }
  Line new_line = {.times = 1, .line = line_n};
  line->lines[line->count++] = new_line;
}

u32 get_line(Lines* line, u32 op_code_pos) {
  int sum = 0;
  for (int i = 0; i < line->count; i++) {
    sum += line->lines[i].times;
    if (sum > op_code_pos) {
      return line->lines[i].line;
    }
  }
  return 0;
}

/// Frees lines
void free_lines(Lines* lines) {
  FREE_ARRAY(Line, lines->lines, lines->capacity);
}