#include "../src/qw_chunk.h"
#include "../src/qw_scanner.h"
#include "../src/qw_vm.h"
#include "greatest.h"

TEST test_scanner_tokens() {
  const char* line = "/ />= ==!= == =!=123\"nice\"1.234cool_thing_123//";
  init_scanner(line);
  ASSERT_EQ(scan_token().type, TOKEN_SLASH);
  ASSERT_EQ(scan_token().type, TOKEN_SLASH);
  ASSERT_EQ(scan_token().type, TOKEN_GREATER_EQUAL);
  ASSERT_EQ(scan_token().type, TOKEN_EQUAL_EQUAL);
  ASSERT_EQ(scan_token().type, TOKEN_BANG_EQUAL);
  ASSERT_EQ(scan_token().type, TOKEN_EQUAL_EQUAL);
  ASSERT_EQ(scan_token().type, TOKEN_EQUAL);
  ASSERT_EQ(scan_token().type, TOKEN_BANG_EQUAL);
  ASSERT_EQ(scan_token().type, TOKEN_NUMBER);
  ASSERT_EQ(scan_token().type, TOKEN_STRING);
  ASSERT_EQ(scan_token().type, TOKEN_NUMBER);
  ASSERT_EQ(scan_token().type, TOKEN_IDENTIFIER);
  ASSERT_EQ(scan_token().type, TOKEN_EOF);
  ASSERT_EQ(scan_token().type, TOKEN_EOF);
  PASS();
}

TEST test_string_literal() {
  const char* line = "                \n\"TEST TEST TEst!-23-4120-5934259235msfdmsfdm\nt\"";
  init_scanner(line);
  char* target = "\"TEST TEST TEst!-23-4120-5934259235msfdmsfdm\nt\"";
  isize size = (isize)strlen(target);
  Token tok = scan_token();
  ASSERT_EQ(size, tok.length);
  for (int i = 0; i < size; i++) {
    ASSERT_EQ(target[i], *(tok.start + i));
  }
  PASS();
}

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

TEST test_vm_stack(void) {
  init_vm();

  for (int i = 0; i < STACK_MAX; i++) {
    push(i);
  }

  for (int i = STACK_MAX - 1; i >= 0; i--) {
    ASSERT_EQ(pop(), i);
  }

  free_vm();
  PASS();
}

TEST test_vm_ins_negate(void) {
  init_vm();

  Chunk ch;
  init_chunk(&ch);
  add_constant_opcode(&ch, 1, 1);
  write_chunk(&ch, OP_NEGATE, 1);
  write_chunk(&ch, OP_RETURN, 1);
  interpret(&ch);
  ASSERT_EQ(stack_vm()[0], -1);
  free_chunk(&ch);
  free_vm();
  PASS();
}

TEST test_vm_ins_binary_ops(void) {
  typedef struct {
    Value op1;
    Value op2;
    Value expected;
    OpCode code;
  } Test;
  Test tests[5] = {
      {1, 2, 3, OP_ADD}, {-2, 2, 0, OP_ADD}, {2, 3, 6, OP_MULTIPLY}, {2, 2, 0, OP_SUBTRACT}, {2, 2, 1, OP_DIVIDE}};
  for (int i = 0; i < 5; i++) {
    init_vm();
    Chunk ch;
    init_chunk(&ch);
    add_constant_opcode(&ch, tests[i].op1, 1);
    add_constant_opcode(&ch, tests[i].op2, 1);
    write_chunk(&ch, tests[i].code, 1);
    write_chunk(&ch, OP_RETURN, 1);
    interpret(&ch);
    ASSERT_EQ(stack_vm()[0], tests[i].expected);
    free_chunk(&ch);
    free_vm();
  }

  PASS();
}

/// tests 1 + 2 * 3 - 4 * -5
TEST test_large_op(void) {
  init_vm();
  Chunk ch;
  init_chunk(&ch);
  add_constant_opcode(&ch, 2, 1);
  add_constant_opcode(&ch, 3, 1);
  write_chunk(&ch, OP_MULTIPLY, 1);
  add_constant_opcode(&ch, 1, 1);
  write_chunk(&ch, OP_ADD, 1);
  add_constant_opcode(&ch, 4, 1);
  add_constant_opcode(&ch, 5, 1);
  write_chunk(&ch, OP_NEGATE, 1);
  write_chunk(&ch, OP_MULTIPLY, 1);
  write_chunk(&ch, OP_SUBTRACT, 1);
  write_chunk(&ch, OP_RETURN, 1);
  interpret(&ch);
  ASSERT_EQ(stack_vm()[0], 27);
  free_chunk(&ch);
  free_vm();
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

SUITE(vm_suite) {
  RUN_TEST(test_vm_stack);
  RUN_TEST(test_vm_ins_negate);
  RUN_TEST(test_vm_ins_binary_ops);
  RUN_TEST(test_large_op);
}

SUITE(scanner_suite) {
  RUN_TEST(test_scanner_tokens);
  RUN_TEST(test_string_literal);
}

/* Add definitions that need to be in the test runner's main file. */
GREATEST_MAIN_DEFS();

int main(int argc, char** argv) {
  GREATEST_MAIN_BEGIN(); /* command-line options, initialization. */

  RUN_SUITE(chunk_suite);

  RUN_SUITE(vm_suite);

  RUN_SUITE(scanner_suite);

  GREATEST_MAIN_END(); /* display results */
}