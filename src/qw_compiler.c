#include "qw_compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#define UINT16_COUNT (UINT16_MAX + 1)

typedef struct {
  Token name;
  i32 depth;
  bool mutable;
} Local;

typedef struct {
  Local locals[UINT16_COUNT];
  i32 local_count;
  i32 scope_depth;
} Compiler;

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
static i32 add_variable_to_global_symbols(Token* name, bool mutable);
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

Compiler* current = NULL;

static void init_compiler(Compiler* compiler_parameter) {
  compiler_parameter->local_count = 0;
  compiler_parameter->scope_depth = 0;
  current = compiler_parameter;
}

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
static inline void emit_op_u32(u8 op, u32 value) {
  emit_u32((op << 24) | (value << 16) | (value & 0xFF)); /* ! TODO: INCOMPLETE*/
}
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

/// Returns the index of the local (in the stack)
static i32 resolve_local(Compiler* compiler, Token* name) {
  for (i32 i = compiler->local_count - 1; i >= 0; i--) {
    Local* local = &compiler->locals[i];
    if (local->name.length == name->length && memcmp(local->name.start, name->start, name->length) == 0) {
      if (!local->mutable) {
        error_at_current("Can't modify `let` ummutable variable");
      }
      return i;
    }
  }
  return -1;
}

/// This is a function that parses a identifier ['=' <expression>]
static void named_variable(Token name, bool can_assign) {
  u8 get_op;
  u8 set_op;
  i32 arg = resolve_local(current, &name);
  if (arg == -1) {
    arg = add_variable_to_global_symbols(&name, true);
    if (arg == -1) {
      error_at_current("cannot reassign (or redeclare) an immutable variable");
      return;
    }
    get_op = OP_GET_GLOBAL;
    set_op = OP_SET_GLOBAL;
  } else {
    get_op = OP_GET_LOCAL;
    set_op = OP_SET_LOCAL;
  }
  // This is an assignment
  if (can_assign && match(TOKEN_EQUAL)) {
    expression();
    emit_op_u16(set_op, (u16)arg);
  } else {
    emit_op_u16(get_op, (u16)arg);
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

/// Tries to add a (mutable or immutable) variable to the
/// global symbol. If the passed name exists, and the mutability flag
/// differs from the value mutability, it will return -1.
///
/// TODO: Maybe set an option that if the variable doesn't exist in the symbol table
///       return -1 as well?
static i32 add_variable_to_global_symbols(Token* name, bool mutable) {
  ObjectString* str = copy_string(name->length, name->start);
  Value value;
  bool exists = table_get(&symbol_table, str, &value);
  if (exists) {
    if (mutable && value.type == VAL_INTERNAL_COMPILER_IMMUTABLE ||
        (!mutable && value.type == VAL_INTERNAL_COMPILER_MUTABLE))
      return -1;
    return (u16)value.as.number;
  }
  Value nil;
  value.type = mutable ? VAL_INTERNAL_COMPILER_MUTABLE : VAL_INTERNAL_COMPILER_IMMUTABLE;
  nil.type = VAL_NIL;
  value.as.number = make_constant(nil);
  table_set(&symbol_table, str, value);
  // Creates a global constant of the name of the token, also, it will be equal to the next
  // thing in the stack. Returns the index in which the string of the variable name is stored
  // make_constant();
  return value.as.number;
}

/// Defines a new variable, either local or global
static inline void define_variable(u16 global) {
  // No need to create a reference to the global position
  // we already got it in the stack...
  if (current->scope_depth > 0) {
    current->locals[current->local_count - 1].depth = current->scope_depth;
    return;
  }
  emit_op_u16(OP_DEFINE_GLOBAL, global);
}

static inline void expression() { parse_precedence(PREC_ASSIGNMENT); }

static inline void begin_scope() {
  // : )
  ++current->scope_depth;
}

static inline void end_scope() {
  // : )
  --current->scope_depth;
  // Pop out of the scope all the declared variables...
  while (current->local_count > 0 && current->locals[current->local_count - 1].depth > current->scope_depth) {
    emit_byte(OP_POP);
    --current->local_count;
  }
}

static void assert_current_and_advance(TokenType type, const char* message) {
  if (parser.current.type == type) {
    advance();
    return;
  }
  error_at_current(message);
}

static inline void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration(false);
  }
  assert_current_and_advance(TOKEN_RIGHT_BRACE, "Expected '}' at the end of the block");
}

/// Adds a local to the local array
/// Marks it as unitialized, make sure of calling declare_variable after this is called
static void add_local(Token name, bool mutable) {
  if (current->local_count == UINT16_COUNT) {
    error_at_current("Too many local variables in function");
    return;
  }
  for (i32 i = current->local_count - 1; i >= 0; i--) {
    Local* local = &current->locals[i];
    if (local->depth == -1 && local->depth < current->scope_depth) {
      break;
    }
    if (local->name.length == name.length && memcmp(name.start, local->name.start, name.length) == 0) {
      error_at_current("you cannot redeclare the same variable name");
    }
  }
  Local* local = &current->locals[current->local_count++];
  local->name = name;
  local->depth = -1;
  local->mutable = mutable;
}

static void try_declare_local_variable(bool mutable) {
  if (current->scope_depth == 0) {
    return;
  }
  Token* name = &parser.previous;
  add_local(*name, mutable);
}

/// parse_variable will check the token_identifier, try to declare a local or global variable
/// using the techniques mentioned in `var_declaration`
static i32 parse_variable(const char* error_message, bool mutable) {
  assert_current_and_advance(TOKEN_IDENTIFIER, error_message);
  try_declare_local_variable(mutable);
  // we check the scope_depth the lookup of local variables are different than global
  if (current->scope_depth > 0) return 0;
  return add_variable_to_global_symbols(&parser.previous, mutable);
}

/// parses ('var' | 'let') IDENT '=' <expression>;
/// When it's a global variable it will add it into the
/// symbols table and keep a reference to the index inside the
/// array of constants (So the VM access directly the global via index)
/// If it's in a local scope, it will just predict the index of the stack
/// and keep an index to it inside the compiler.locals, adding a new local
static void var_declaration(bool mutable) {
  i32 global = parse_variable("Expected a valid identifier", mutable);
  if (global == -1) {
    error_at_current("Can't redeclare an immutable/mutable variable...");
    return;
  }
  // Put in the stack the value
  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emit_byte(OP_NIL);
  }
  assert_current_and_advance(TOKEN_SEMICOLON, "Expected ';' after variable declaration");
  // Define the global variable if it's a global variable
  define_variable((u16)global);
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

/// Returns the pointer to where the instruction starts
static i32 emit_jump(u8 instruction) {
  emit_op_u16(instruction, 0xFFFF);
  return current_chunk()->count - 2;
}

static void patch_jump(i32 jump_code_slot) {
  u32 lines_to_jump = current_chunk()->count - jump_code_slot - 2;
  if (lines_to_jump > UINT16_MAX) {
    error_at_current("we can't let you do ifs that jump 65000 op codes");
    return;
  }
  current_chunk()->code[jump_code_slot] = (lines_to_jump & 0xFF00) >> 8;
  current_chunk()->code[jump_code_slot + 1] = lines_to_jump & 0xFF;
  printf("%d\n", ((lines_to_jump & 0xFF00) | (lines_to_jump & 0xFF)));
}

static void if_statement() {
#if USE_PARENS_FOR_IFS
  assert_current_and_advance(TOKEN_LEFT_PAREN, "Expected '(' before condition");
#endif
  expression();
#if USE_PARENS_FOR_IFS
  assert_current_and_advance(TOKEN_RIGHT_PAREN, "Expected ')' after condition");
#endif
  i32 then_jump = emit_jump(OP_JUMP_IF_FALSE);
  statement(false);
  patch_jump(then_jump);
  if (match(TOKEN_ELSE)) {
    statement(false);
  }
}

static void statement(bool _) {
  if (match(TOKEN_PRINT)) {
    print_statement();
  } else if (match(TOKEN_IF)) {
    if_statement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    begin_scope();
    block();
    end_scope();
  } else {
    expression_statement();
  }
}

static void declaration(bool _) {
  if (match(TOKEN_VAR)) {
    var_declaration(true);
  } else if (match(TOKEN_LET)) {
    var_declaration(false);
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
  Compiler compiler;
  init_compiler(&compiler);
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