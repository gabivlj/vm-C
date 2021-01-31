#include "qw_compiler.h"

#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "qw_common.h"
#include "qw_debug.h"
#include "qw_object.h"
#include "qw_scanner.h"

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  // =
  PREC_OR,          // or
  PREC_AND,         // and
  PREC_EQUALITY,    // == !=
  PREC_COMPARISON,  // < > <= >=
  PREC_TERM,        // + -
  PREC_FACTOR,      // * /
  PREC_UNARY,       // ! -
  PREC_CALL,        // . ()
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;
static void binary();
static void unary();
static void grouping();
static void number();
static void ternary();
static void literal();
static void string();
static void statement();
static void declaration();
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_UNARY},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_LESS] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_IDENTIFIER] = {NULL, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_QUESTION] = {NULL, ternary, PREC_ASSIGNMENT},
    [TOKEN_DOUBLE_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static ParseRule* get_rule(TokenType type) { return &rules[type]; }

typedef struct {
  Token current;
  Token previous;
  u8 had_error;
  u8 panic_mode;
} Parser;

Parser parser;
Chunk* chunk;

static Chunk* current_chunk() { return chunk; }

static void error_at(Token* token, const char* message) {
  if (parser.panic_mode) return;
  parser.panic_mode = 1;
  fprintf(stderr, "error line [%d]", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
  } else {
    fprintf(stderr, "at '%.*s'", token->length, token->start);
  }
  fprintf(stderr, ": %s\n", message);
  parser.had_error = 1;
}

static void error_at_previous(const char* message) { error_at(&parser.previous, message); }

static void error_at_current(const char* message) { error_at(&parser.current, message); }

static void advance() {
  parser.previous = parser.current;
  for (;;) {
    parser.current = scan_token();
    if (parser.current.type != TOKEN_ERROR) return;
    error_at_current(parser.current.start);
  }
}

static void expression();

static void parse_precedence(Precedence precedence) {
  advance();
  ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
  if (prefix_rule == NULL) {
    error_at_previous("Expect expression.");
    return;
  }
  prefix_rule();
  while (precedence <= get_rule(parser.current.type)->precedence) {
    advance();
    ParseFn infix_rule = get_rule(parser.previous.type)->infix;
    infix_rule();
  }
}

static void expression() { parse_precedence(PREC_ASSIGNMENT); }

static void assert_current_and_advance(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
    return;
  }
  error_at_current(message);
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}

static inline void emit_byte(u8 byte) { write_chunk(current_chunk(), byte, parser.previous.line); }
static inline void emit_u16(u16 value) { write_chunk_u16(current_chunk(), value, parser.previous.line); }
static inline void emit_u32(u32 value) { write_chunk_u32(current_chunk(), value, parser.previous.line); }
static inline void emit_u64(u64 value) { write_chunk_u64(current_chunk(), value, parser.previous.line); }
static inline void emit_op_u8(u8 op, u8 value) { emit_u16((op << 8) | value); }
static inline void emit_op_u16(u8 op, u16 value) { emit_u32((op << 16) | value); }
static inline void emit_return() { emit_byte(OP_RETURN); }
static inline void end_compiler() {
  emit_return();
#ifdef DEBUG_PRINT_CODE
  if (!parser.had_error) {
    dissasemble_chunk(current_chunk(), "code");
  }
#endif
}

static void print_statement() {
  expression();
  assert_current_and_advance(TOKEN_SEMICOLON, "Expected ';' after value.");
  emit_byte(OP_PRINT);
}

// We use add_constant_opcode (internal logic inside chunk.h) because
// it handles as well which kind of OP_CONSTANT to use
static u32 emit_constant(Value value) { return add_constant_opcode(chunk, value, parser.previous.line); }

static void grouping() {
  expression();
  assert_current_and_advance(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void binary() {
  TokenType operator_type = parser.previous.type;
  ParseRule* rule = get_rule(operator_type);
  parse_precedence((Precedence)(rule->precedence + 1));
  // Emit the operator instruction.
  switch (operator_type) {
    // case TOKEN_EQUAL:
    //   emit_byte(OP_EQUAL);
    //   break;
    case TOKEN_GREATER:
      emit_byte(OP_GREATER);
      break;
    case TOKEN_LESS:
      emit_byte(OP_LESS);
      break;
    case TOKEN_EQUAL_EQUAL:
      emit_byte(OP_EQUAL);
      break;
    case TOKEN_BANG_EQUAL: {
      emit_op_u8(OP_EQUAL, OP_NOT);
      break;
    }
    case TOKEN_GREATER_EQUAL: {
      emit_op_u8(OP_LESS, OP_NOT);
      break;
    }
    case TOKEN_LESS_EQUAL: {
      emit_op_u8(OP_GREATER, OP_NOT);
      break;
    }
    case TOKEN_PLUS:
      emit_byte(OP_ADD);
      break;
    case TOKEN_MINUS:
      emit_byte(OP_SUBTRACT);
      break;
    case TOKEN_STAR:
      emit_byte(OP_MULTIPLY);
      break;
    case TOKEN_SLASH:
      emit_byte(OP_DIVIDE);
      break;
    default:
      return;  // Unreachable.
  }
}

static void literal() {
  switch (parser.previous.type) {
    case TOKEN_FALSE: {
      emit_byte(OP_FALSE);
      break;
    }
    case TOKEN_NIL: {
      emit_byte(OP_NIL);
      break;
    }
    case TOKEN_TRUE: {
      emit_byte(OP_TRUE);
      break;
    }
    default: {
      return;
    }
  }
}

static void string() {
  Token t = parser.previous;
  ObjectString* string = copy_string(t.length - 2, t.start + 1);
  emit_constant(OBJECT_VAL(string));
}

// ::= '-' expression
static void unary() {
  TokenType operator_type = parser.previous.type;
  // Compile the operand
  parse_precedence(PREC_UNARY);
  switch (operator_type) {
    case TOKEN_MINUS:
      emit_byte(OP_NEGATE);
      break;
    case TOKEN_BANG: {
      emit_byte(OP_NOT);
      break;
    }
    default: {
      break;
    }
  }
}

static void ternary() {
  // TODO: Emit code of handling ternary
  parse_precedence(PREC_UNARY);
  assert_current_and_advance(TOKEN_DOUBLE_DOT, "expected doubles");
  parse_precedence(PREC_UNARY);
}

static void number() {
  double value = strtod(parser.previous.start, NULL);
  emit_constant(NUMBER_VAL(value));
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    print_statement();
  }
}

static void declaration() { statement(); }

u8 compile(const char* source, Chunk* chunk_to_add_on) {
  init_scanner(source);
  chunk = chunk_to_add_on;
  parser.had_error = 0;
  parser.panic_mode = 0;
  advance();
  // while (!match(TOKEN_EOF)) {
  //   statement();
  // }
  expression();
  assert_current_and_advance(TOKEN_EOF, "Expected end of expression");
  end_compiler();
  return !parser.had_error;
}