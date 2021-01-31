#include "qw_compiler.h"

#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "qw_common.h"
#include "qw_debug.h"
#include "qw_object.h"
#include "qw_scanner.h"
#include "qw_table.h"

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

typedef void (*ParseFn)(bool);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

static void binary(bool);
static void unary(bool);
static void grouping(bool);
static void number(bool);
static void ternary(bool);
static void literal(bool);
static void string(bool);
static void statement(bool);
static void declaration(bool);
static void variable(bool);
static u16 identifier_constant(Token* name);
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
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
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
Table symbol_table;

// We use add_constant_opcode (internal logic inside chunk.h) because
// it handles as well which kind of OP_CONSTANT to use
static u32 emit_constant(Value value) { return add_constant_opcode(chunk, value, parser.previous.line); }

static Chunk* current_chunk() { return chunk; }

static inline void emit_byte(u8 byte) { write_chunk(current_chunk(), byte, parser.previous.line); }
static inline void emit_u16(u16 value) { write_chunk_u16(current_chunk(), value, parser.previous.line); }
static inline void emit_u24(u32 value) { write_chunk_u24(current_chunk(), value, parser.previous.line); }
static inline void emit_u32(u32 value) { write_chunk_u32(current_chunk(), value, parser.previous.line); }
static inline void emit_u64(u64 value) { write_chunk_u64(current_chunk(), value, parser.previous.line); }
static inline void emit_op_u8(u8 op, u8 value) { emit_u16((op << 8) | value); }
static inline void emit_op_u16(u8 op, u16 value) { emit_u24((op << 16) | value); }
// static inline void emit_op_u32(u8 op, u32 value) { emit_u((op << 24) | value); }
static inline void emit_return() { emit_byte(OP_RETURN); }

static u32 make_constant(Value val) {
  //
  return add_constant(current_chunk(), val);
}

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

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}

static void synchronize() {
  parser.panic_mode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;

    switch (parser.current.type) {
      case TOKEN_CLASS:
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_FOR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
        return;

      default:
          // Do nothing.
          ;
    }

    advance();
  }
}

static void expression();

/// This is a function that parses a identifier ['=' <expression>]
static void named_variable(Token name, bool can_assign) {
  u16 arg = identifier_constant(&name);
  // This is an assignment
  if (can_assign && match(TOKEN_EQUAL)) {
    expression();
    emit_op_u16(OP_SET_GLOBAL, arg);
  } else {
    emit_op_u16(OP_GET_GLOBAL, arg);
  }
}

/// This is a function that parses a <variable> = <expression>
static void variable(bool can_assign) {
  if (parser.previous.type == TOKEN_IDENTIFIER) {
    named_variable(parser.previous, can_assign);
  } else {
    error_at_previous("Expected named variable for assignment");
    return;
  }
}

static void parse_precedence(Precedence precedence) {
  advance();
  ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
  if (prefix_rule == NULL) {
    error_at_previous("Expect expression.");
    return;
  }
  // We do this because if we are in the middle of an operation,
  // we can't allow that operation to be "assigned", so we pass this can_assign
  // to possible assignment parse function
  // this way we won't allow : a * b = 1;
  // because PREC_FACTOR | PREC_TERM > ASSIGNMENT..
  bool can_assign = precedence <= PREC_ASSIGNMENT;
  prefix_rule(can_assign);
  while (precedence <= get_rule(parser.current.type)->precedence) {
    advance();
    ParseFn infix_rule = get_rule(parser.previous.type)->infix;
    infix_rule(can_assign);
  }
  if (can_assign && match(TOKEN_EQUAL)) {
    error_at_current("Invalid assignment target");
  }
}

static u16 identifier_constant(Token* name) {
  ObjectString* str = copy_string(name->length, name->start);
  Value value;
  bool exists = table_get(&symbol_table, str, &value);
  if (exists) {
    return (u16)value.as.number;
  }
  Value nil;
  value.type = VAL_NUMBER;
  nil.type = VAL_NIL;
  value.as.number = make_constant(nil);
  table_set(&symbol_table, str, value);
  // Creates a global constant of the name of the token, also, it will be equal to the next
  // thing in the stack. Returns the index in which the string of the variable name is stored
  // make_constant();
  return value.as.number;
}

static void define_variable(u16 global) { emit_op_u16(OP_DEFINE_GLOBAL, global); }

static void expression() { parse_precedence(PREC_ASSIGNMENT); }

static void assert_current_and_advance(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
    return;
  }
  error_at_current(message);
}

static u16 parse_variable(const char* error_message) {
  assert_current_and_advance(TOKEN_IDENTIFIER, error_message);
  return identifier_constant(&parser.previous);
}

static void var_declaration() {
  u16 global = parse_variable("Expected a string");
  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emit_byte(OP_NIL);
  }
  assert_current_and_advance(TOKEN_SEMICOLON, "Expected ';' after variable declaration");
  define_variable(global);
}

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

static void grouping(bool _) {
  expression();
  assert_current_and_advance(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void binary(bool _) {
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

static void literal(bool _) {
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

static void string(bool _) {
  Token t = parser.previous;
  ObjectString* string = copy_string(t.length - 2, t.start + 1);
  emit_constant(OBJECT_VAL(string));
}

// ::= '-' expression
static void unary(bool _) {
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

static void ternary(bool _) {
  // TODO: Emit code of handling ternary
  parse_precedence(PREC_UNARY);
  assert_current_and_advance(TOKEN_DOUBLE_DOT, "expected doubles");
  parse_precedence(PREC_UNARY);
}

static void number(bool _) {
  double value = strtod(parser.previous.start, NULL);
  emit_constant(NUMBER_VAL(value));
}

static void expression_statement() {
  expression();
  assert_current_and_advance(TOKEN_SEMICOLON, "Expected ';' after expression.");
  emit_byte(OP_POP);
}

static void statement(bool _) {
  if (match(TOKEN_PRINT)) {
    print_statement();
  } else {
    expression_statement();
  }
}

static void declaration(bool _) {
  if (match(TOKEN_VAR)) {
    var_declaration();
  } else {
    statement(false);
  }
  if (parser.panic_mode) {
    synchronize();
  }
}

u8 compile(const char* source, Chunk* chunk_to_add_on) {
  // if (symbol_table.capacity != 0) free_table(&symbol_table);
  if (symbol_table.capacity == 0) init_table(&symbol_table);
  init_scanner(source);
  chunk = chunk_to_add_on;
  parser.had_error = 0;
  parser.panic_mode = 0;
  advance();
  while (!match(TOKEN_EOF)) {
    declaration(false);
  }
  // expression();
  assert_current_and_advance(TOKEN_EOF, "Expected end of expression");
  end_compiler();
  return !parser.had_error;
}