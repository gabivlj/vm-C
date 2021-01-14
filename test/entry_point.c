#include "../src/qw_chunk.h"
#include "greatest.h"

TEST test_return_chunks(void) {
  Chunk ch;
  init_chunk(&ch);
  write_chunk(&ch, OP_RETURN, 1);
  write_chunk(&ch, OP_RETURN, 2);
  write_chunk(&ch, OP_RETURN, 3);
  ASSERT_EQm("Expected return op", ch.code[0], OP_RETURN);
  ASSERT_EQ(ch.code[1], OP_RETURN);
  ASSERT_EQ(ch.code[2], OP_RETURN);
  PASS();
}

TEST test_constants_chunks(void) {
  Chunk ch;
  init_chunk(&ch);
  for (int i = 0; i < 300; i++) {
    add_constant_opcode(&ch, 1.2 + i, i);
  }
  for (int i = 0; i < 256 * 2; i += 2) {
    ASSERT_EQ(ch.code[i], OP_CONSTANT);
    ASSERT_EQ(ch.code[i + 1], i / 2);
  }
  for (int i = 256 * 2, j = 256; i < ch.count; i += 3, j++) {
    ASSERT_EQ(ch.code[i], OP_CONSTANT_LONG);
    ASSERT_EQ((ch.code[i + 1] << 8) | ch.code[i + 2], j);
  }
  PASS();
}

TEST test_lines(void) {
  Chunk ch;
  init_chunk(&ch);
  for (int i = 0; i < 128; i++) {
    add_constant_opcode(&ch, 1.2 + i, i);
    add_constant_opcode(&ch, 1.2 + i, i);
  }
  for (int i = 0; i < ch.lines.count; i++) {
    ASSERT_EQ(ch.lines.lines[i].times, 4);
    ASSERT_EQ(ch.lines.lines[i].line, i);
  }
  int prev = ch.lines.count;
  for (int i = 0; i < 128; i++) {
    add_constant_opcode(&ch, 1.2 + i, i + prev);
    add_constant_opcode(&ch, 1.2 + i, i + prev);
  }
  for (int i = prev; i < ch.lines.count; i++) {
    // 6 times because when there is more than 256 constants, we start
    // to use 3 byte width OP_CONSTANTs.
    ASSERT_EQ(ch.lines.lines[i].times, 6);
    ASSERT_EQ(ch.lines.lines[i].line, i);
  }
  PASS();
}

SUITE(chunk_suite) {
  RUN_TEST(test_return_chunks);
  RUN_TEST(test_constants_chunks);
  RUN_TEST(test_lines);
}

/* Add definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int main(int argc, char** argv) {
  GREATEST_MAIN_BEGIN(); /* command-line options, initialization. */

  RUN_SUITE(chunk_suite);

  GREATEST_MAIN_END(); /* display results */
}