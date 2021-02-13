/*

*/

#include "qw_compiler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "qw_common.h"
#include "qw_debug.h"
#include "qw_native_functions.h"
#include "qw_object.h"
#include "qw_scanner.h"
#include "qw_table.h"
#include "qw_vm.h"

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
static void and_op(bool);
static void call(bool);
static void or_op(bool);
static void dot(bool);
// instance->"key", instance->defined_variable_as_key, instance->0
static void index_access(bool);
static void add_native_function(const char* name, NativeFn function);
static i32 add_variable_to_global_symbols(Token* name, bool mutable, bool can_assign);
ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, dot, PREC_CALL},
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
    [TOKEN_AND] = {NULL, and_op, PREC_AND},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, or_op, PREC_OR},
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
    [TOKEN_DOUBLE_POINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS_ARROW] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACKET] = {NULL, index_access, PREC_CALL},
    [TOKEN_RIGHT_BRACKET] = {NULL, NULL, PREC_NONE},
    [TOKEN_NOTHING] = {NULL, NULL, PREC_NONE},
};

static ParseRule* get_rule(TokenType type) { return &rules[type]; }

typedef struct {
  Token current;
  Token previous;
  u8 had_error;
  u8 panic_mode;
} Parser;

Parser parser;

Table symbol_table;

Compiler* current;

void mark_compiler_roots() {
  Compiler* compiler = current;
  while (compiler != NULL) {
    mark_array(compiler->globals);
    mark_object((Object*)compiler->function);
    compiler = compiler->enclosing_compiler;
  }
  mark_table(&symbol_table);
}

static void init_compiler(Compiler* compiler_parameter, FunctionType type) {
  compiler_parameter->function = NULL;
  compiler_parameter->local_count = 0;
  compiler_parameter->function = NULL;
  compiler_parameter->scope_depth = 0;
  compiler_parameter->function_type = type;
  compiler_parameter->function = new_function();
  push(OBJECT_VAL(compiler_parameter->function));
  compiler_parameter->globals = NULL;
  if (current == NULL || current->globals == NULL) {
    compiler_parameter->globals = malloc(sizeof(ValueArray));
    init_value_array(compiler_parameter->globals);
  } else {
    compiler_parameter->globals = current->globals;
  }
  //
  compiler_parameter->enclosing_compiler = current;
  current = compiler_parameter;
  pop();
  if (type != TYPE_SCRIPT) {
    current->function->name = copy_string(parser.previous.length, parser.previous.start);
  }

  Local* local = &current->locals[current->local_count++];
  local->depth = 0;
  local->name.start = "";
  local->name.length = 0;
  local->is_captured = false;
  if (compiler_parameter->enclosing_compiler == NULL) {
    if (symbol_table.capacity != 0) {
      //      free_table(&symbol_table);
    }
    init_table(&symbol_table);
    //    add_native_function("clock", clock_native);
  }
}

static Chunk* current_chunk() { return &current->function->chunk; }
// We use add_constant_opcode (internal logic inside chunk.h) because
// it handles as well which kind of OP_CONSTANT to use
static u32 emit_constant(Value value) {
  push(value);
  u32 v = add_constant_opcode(current_chunk(), value, parser.previous.line);
  pop();
  return v;
}

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
static inline void emit_empty_return() {
  emit_byte(OP_NIL);
  emit_byte(OP_RETURN);
}

static u32 make_constant(Value val) {
  //
  push(val);
  i32 index = add_constant(current_chunk(), val);
  pop();
  return index;
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

static void expression(void);

/// Returns the index of the local (in the stack)
static i32 resolve_local(Compiler* compiler, Token* name) {
  for (i32 i = compiler->local_count - 1; i >= 0; i--) {
    Local* local = &compiler->locals[i];
    if (local->name.length == name->length && memcmp(local->name.start, name->start, name->length) == 0) {
      // if (!local->mutable) {
      //   error_at_current("Can't modify `let` ummutable variable");
      // }
      return i;
    }
  }
  return -1;
}

i32 add_upvalue(Compiler* compiler, i32 upvalue_index, bool is_local) {
  u32 upvalue_count = compiler->function->upvalue_count;
  for (i32 i = 0; i < upvalue_count; i++) {
    Upvalue* upvalue = &compiler->upvalues[i];
    if (upvalue->index == upvalue_index && upvalue->is_local == is_local) {
      return i;
    }
  }
  compiler->upvalues[upvalue_count].is_local = is_local;

  compiler->upvalues[upvalue_count].index = upvalue_index;
  return compiler->function->upvalue_count++;
}

/// Returns the upvalue index (if it finds one)
static i32 resolve_upvalue(Compiler* compiler, Token* name) {
  if (compiler->enclosing_compiler == NULL) {
    return -1;
  }

  // Resolve it if it's just above
  i32 local = resolve_local(compiler->enclosing_compiler, name);

  // It means that it's just above this function
  if (local != -1) {
    compiler->enclosing_compiler->locals[local].is_captured = true;
    return add_upvalue(compiler, local, true);
  }

  // check end of file explaining diagram
  i32 upvalue = resolve_upvalue(compiler->enclosing_compiler, name);

  // This means that it's 2 levels above, minimum
  if (upvalue != -1) {
    return add_upvalue(compiler, upvalue, false);
  }
  return -1;
}

/// This is a function that parses a identifier ['=' <expression>]
static void named_variable(Token name, bool can_assign) {
  u8 get_op;
  u8 set_op;
  i32 arg = resolve_local(current, &name);
  if (arg != -1) {
    get_op = OP_GET_LOCAL;
    set_op = OP_SET_LOCAL;
  } else {
    arg = resolve_upvalue(current, &name);
    if (arg != -1) {
      get_op = OP_GET_UPVALUE;
      set_op = OP_SET_UPVALUE;
    } else {
      arg = add_variable_to_global_symbols(&name, !can_assign, can_assign);
      if (arg == -1) {
        error_at_current("cannot reassign (or redeclare) an immutable variable");
        return;
      }
      get_op = OP_GET_GLOBAL;
      set_op = OP_SET_GLOBAL;
    }
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
/// TODO: Create various functions so we don't check local state.
/// At the moment the user can "literally" do anything
/// TODO: Maybe set an option that if the variable doesn't exist in the symbol table
///       return -1 as well?
static i32 add_variable_to_global_symbols(Token* name, bool mutable, bool can_assign) {
  ObjectString* str = copy_string(name->length, name->start);
  push(OBJECT_VAL(str));
  Value value;
  bool exists = table_get(&symbol_table, str, &value);
  if (exists) {
    pop();
    // if (mutable && value.type == VAL_INTERNAL_COMPILER_IMMUTABLE && can_assign) return -1;
    return (u16)value.as.number;
  }
  // if (!can_assign) {
  //   return -1;
  // }
  Value nil;
  value.type = mutable ? VAL_INTERNAL_COMPILER_MUTABLE : VAL_INTERNAL_COMPILER_IMMUTABLE;
  nil.type = VAL_NUMBER;
  nil.as.number = 0;
  push_value(current->globals, nil);
  value.as.number = current->globals->count - 1;
  table_set(&symbol_table, str, value);
  pop();
  return value.as.number;
}

static void add_native_function(const char* name, NativeFn function) {
  ObjectString* name_str = copy_string((u32)strlen(name), name);
  ObjectNative* native_fn = new_native_function(function);
  Value object_value = OBJECT_VAL(native_fn);
  push(OBJECT_VAL(name_str));
  push(object_value);
  push_value(current->globals, object_value);
  pop();
  Value fill_value;
  bool exists = table_get(&symbol_table, name_str, &fill_value);
  if (exists) {
    error_at_current("can't declare native function because name already exists");
    return;
  }
  fill_value.type = VAL_INTERNAL_COMPILER_IMMUTABLE;
  fill_value.as.number = current->globals->count - 1;
  table_set(&symbol_table, name_str, fill_value);
  pop();
}

static inline void mark_initialized() {
  if (current->scope_depth == 0) return;
  current->locals[current->local_count - 1].depth = current->scope_depth;
}

/// Defines a new variable, either local or global
static inline void define_variable(u16 global) {
  // No need to create a reference to the global position
  // we already got it in the stack...
  if (current->scope_depth > 0) {
    mark_initialized();
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
    if (current->locals[current->local_count - 1].is_captured)
      emit_byte(OP_CLOSE_UPVALUE);
    else
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
  local->is_captured = false;
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
  return add_variable_to_global_symbols(&parser.previous, mutable, true);
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

static inline ObjectFunction* end_compiler() {
  emit_empty_return();
  ObjectFunction* function = current->function;
  function->global_array = current->globals;
#ifdef DEBUG_PRINT_CODE
  if (!parser.had_error) {
    dissasemble_chunk(&function->chunk, function->name != NULL ? function->name->chars : "<script>");
  }
#endif
  current = current->enclosing_compiler;
  return function;
}

static void user_level_statement(u8 op_code_to_emit) {
  expression();
  assert_current_and_advance(TOKEN_SEMICOLON, "Expected ';' after value.");
  emit_byte(op_code_to_emit);
}

static void grouping(bool _) {
  expression();
  assert_current_and_advance(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

static void index_access(bool assignable) {
  parse_precedence(PREC_ASSIGNMENT + 1);
  assert_current_and_advance(TOKEN_RIGHT_BRACKET, "Expected ] after index access");
  if (assignable && match(TOKEN_EQUAL)) {
    expression();
    emit_byte(OP_SET_PROPERTY_TOP_STACK);
  } else {
    emit_byte(OP_GET_PROPERTY_TOP_STACK);
  }
}

static void dot(bool assignable) {
  assert_current_and_advance(TOKEN_IDENTIFIER, "Expected identifier after .");
  u16 name = make_constant(OBJECT_VAL(copy_string(parser.previous.length, parser.previous.start)));
  if (assignable && match(TOKEN_EQUAL)) {
    expression();
    emit_op_u16(OP_SET_PROPERTY, name);
  } else {
    emit_op_u16(OP_GET_PROPERTY, name);
  }
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

/// Patch jump passes the position where the jump instruction is and patches the operands
/// with the value of `current_chunk()->count - jump_code_slot-2`, which is the number of lines
static void patch_jump(i32 jump_code_slot) {
  u32 lines_to_jump = current_chunk()->count - jump_code_slot - 2;
  if (lines_to_jump > UINT16_MAX) {
    error_at_current("we can't let you do ifs that jump 65000 op codes");
    return;
  }
  // printf("%d %d\n", ((lines_to_jump & 0xFF00) | (lines_to_jump & 0xFF)), jump_code_slot);
  current_chunk()->code[jump_code_slot] = (lines_to_jump & 0xFF00) >> 8;
  current_chunk()->code[jump_code_slot + 1] = lines_to_jump & 0xFF;
}

static void and_op(bool _) {
  i32 end_jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  parse_precedence(PREC_AND);
  patch_jump(end_jump);
}

static void or_op(bool _) {
  i32 else_jump = emit_jump(OP_JUMP_IF_FALSE);
  i32 end_jump = emit_jump(OP_JUMP);
  patch_jump(else_jump);
  emit_byte(OP_POP);
  parse_precedence(PREC_OR);
  patch_jump(end_jump);
}

static void if_statement() {
#if USE_PARENS_FOR_STATEMENT_CONDITIONS
  assert_current_and_advance(TOKEN_LEFT_PAREN, "Expected '(' before condition");
#endif
  expression();
#if USE_PARENS_FOR_STATEMENT_CONDITIONS
  assert_current_and_advance(TOKEN_RIGHT_PAREN, "Expected ')' after condition");
#endif
  // First condition
  i32 then_jump = emit_jump(OP_JUMP_IF_FALSE);
  // Pop the condition
  emit_byte(OP_POP);
  // Parse stmts of if
  statement(false);
  // Emit else opcode
  i32 else_jump = emit_jump(OP_JUMP);
  // Pop the condition
  emit_byte(OP_POP);
  // Patch now the if
  patch_jump(then_jump);
  // Now if there is an else, parse more statements
  if (match(TOKEN_ELSE)) {
    statement(false);
  }
  patch_jump(else_jump);
}

static void emit_loop(i32 start) {
  // + 3 because we have to jump over OP_JUMP_BACK and its 2 bytes (operands)
  emit_op_u16(OP_JUMP_BACK, current_chunk()->count - start + 3);
}

static u8 argument_list() {
  u8 arg_count = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      arg_count++;
    } while (match(TOKEN_COMMA));
  }
  if (arg_count == 255) {
    error_at_previous("can't have more than 255 args...");
  }
  assert_current_and_advance(TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
  return arg_count;
}

static void call(bool can_assign) {
  //
  u8 arg_list = argument_list();
  emit_op_u8(OP_CALL, arg_list);
}

static void while_statement() {
#if USE_PARENS_FOR_STATEMENT_CONDITIONS
  assert_current_and_advance(TOKEN_LEFT_PAREN, "Expected '(' before condition");
#endif
  i32 condition_start = current_chunk()->count;
  expression();
#if USE_PARENS_FOR_STATEMENT_CONDITIONS
  assert_current_and_advance(TOKEN_RIGHT_PAREN, "Expected ')' after condition");
#endif
  i32 jump = emit_jump(OP_JUMP_IF_FALSE);
  emit_byte(OP_POP);
  statement(false);
  emit_loop(condition_start);
  patch_jump(jump);
  emit_byte(OP_POP);
}

static void for_statement() {
  begin_scope();
  assert_current_and_advance(TOKEN_LEFT_PAREN, "Expected '(' after 'for'");

  if (match(TOKEN_SEMICOLON)) {
    // nothing bro...
  } else if (match(TOKEN_VAR)) {
    var_declaration(true);
  } else if (match(TOKEN_LET)) {
    var_declaration(false);
  } else {
    // do this because expression statement expects a ';'
    expression_statement();
  }

  i32 loop_start = current_chunk()->count;

  i32 jump = -1;

  if (!match(TOKEN_SEMICOLON)) {
    expression();
    assert_current_and_advance(TOKEN_SEMICOLON, "Expected ; after second expression on for");
    jump = emit_jump(OP_JUMP_IF_FALSE);
    // pop prev. expression
    emit_byte(OP_POP);
  }

  if (!match(TOKEN_RIGHT_PAREN)) {
    i32 jump_condition = emit_jump(OP_JUMP);
    i32 start_increment = current_chunk()->count;
    expression();
    emit_byte(OP_POP);
    assert_current_and_advance(TOKEN_RIGHT_PAREN, "Expected ')' after 'for'");
    emit_loop(loop_start);
    loop_start = start_increment;
    patch_jump(jump_condition);
  }

  statement(false);

  emit_loop(loop_start);

  if (jump != -1) {
    patch_jump(jump);
    emit_byte(OP_POP);
  }

  end_scope();
}

static void when_condition(void);
static void handle_token_bar() {
  // This is the equivalent of doing an OR
  i32 else_jump = emit_jump(OP_JUMP_IF_FALSE);
  i32 end_jump = emit_jump(OP_JUMP);
  patch_jump(else_jump);
  // Pop previous condition
  emit_byte(OP_POP);
  // Try again
  when_condition();
  // Set the jump to this if true
  patch_jump(end_jump);
}

static void when_condition() {
  // Repeat the same value
  emit_byte(OP_PUSH_TOP);
  expression();
  if (match(TOKEN_BAR)) {
    emit_byte(OP_EQUAL);
    handle_token_bar();
  } else if (match(TOKEN_DOUBLE_POINT)) {
    emit_byte(OP_GREATER);
    i32 jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    emit_byte(OP_PUSH_TOP);
    expression();
    emit_byte(OP_LESS);
    patch_jump(jump);
    // Keep checking token_bars
    if (match(TOKEN_BAR)) {
      handle_token_bar();
    }
  } else {
    emit_byte(OP_EQUAL);
  }
}

static i32 do_when_condition() {
  when_condition();
  i32 to_jump = emit_jump(OP_JUMP_IF_FALSE);
  assert_current_and_advance(TOKEN_MINUS_ARROW, "expected arrow...");
  // emit_byte(OP_POP);
  statement(false);
  emit_byte(OP_POP);
  i32 jump = emit_jump(OP_JUMP);
  patch_jump(to_jump);
  // Pop out the condition
  emit_byte(OP_POP);
  return jump;
}

// Parses and generates bytecode for the following grammar rules
// ```
//     when ::= 'when' expression '{'
//       (when_expression '->' statement)*
//       'nothing' '->' statement
//     '}'
//     when_expression ::= expression (when_operators expression)*
//     when_operators ::= '|' | '..'
// ```
static void when_statement() {
  // Get the expression on top of the stack
  expression();
  assert_current_and_advance(TOKEN_LEFT_BRACE, "Expected brace TODO ERROR");
  // Maximum 64 Jumps. NOTE: Maybe we should do it dynamic?
  i32 jumps[64] = {};
  u8 i = 0;
  // Meanwhile we don't arrive at the last condition, keep getting jumps
  while (!match(TOKEN_NOTHING)) {
    jumps[i] = do_when_condition();
    i++;
  }
  // Check that we got a -> next to the nothing
  assert_current_and_advance(TOKEN_MINUS_ARROW, "Expected arrow");
  // parse the statement next to ->
  statement(false);
  for (int j = 0; j < i; j++) {
    patch_jump(jumps[j]);
  }
  emit_byte(OP_POP);
  assert_current_and_advance(TOKEN_RIGHT_BRACE, "Expected brace TODO ERROR");
}

static void parse_function(FunctionType type) {
  // Initialize a new compiler for this function
  Compiler compiler;
  init_compiler(&compiler, type);
  begin_scope();
  assert_current_and_advance(TOKEN_LEFT_PAREN, "Expected '(' after a function name");
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->number_of_parameters++;
      if (current->function->number_of_parameters > 255) {
        error_at_current("can't have more than 255 parameters");
      }
      u16 param_constant = parse_variable("Expect parameter name.", true);
      define_variable(param_constant);
    } while (match(TOKEN_COMMA));
  }
  assert_current_and_advance(TOKEN_RIGHT_PAREN, "Expected ')' after a function name");

  assert_current_and_advance(TOKEN_LEFT_BRACE, "Expected '{' before fn body");
  block();

  ObjectFunction* function = end_compiler();
  push(OBJECT_VAL(function));
  u16 constant = make_constant(OBJECT_VAL(function));
  pop();
  emit_op_u16(OP_CLOSURE, constant);

  // emit_constant(OBJECT_VAL(function));
  for (i32 i = 0; i < function->upvalue_count; ++i) {
    emit_byte(compiler.upvalues[i].is_local ? 1 : 0);
    // IF NOT LOCAL: It will store where in the frame above is stored
    // IF LOCAL : It will store where in the stack is stored
    emit_byte(compiler.upvalues[i].index);
  }
  // end_scope();
}

// 0x7ffeef3ffff8
static void fun_declaration(void) {
  u16 global = parse_variable("Expected function name.", false);
  mark_initialized();
  parse_function(TYPE_FUNCTION);
  define_variable(global);
}

static void class_declaration() {
  // Identifier
  Token* name = &parser.previous;

  // Allocate local/global variable
  // If local: It's cool because we got it in the stack and OP_CLASS will push class constructor into stack
  // If global: We reserve a space in global symbol_table and then in define_variable we will emit
  //            OP_DEFINE_GLOBAL which will take the top element of the stack (just pushed by OP_CLASS)
  u16 name_constant = parse_variable("Expected identifier", true);

  // Make the constant (ObjectString*) which the OP_CLASS will use
  u16 constant_name = make_constant(OBJECT_VAL(copy_string(name->length, name->start)));
  emit_op_u16(OP_CLASS, constant_name);

  // As we said, declare global, if it's local make it usable
  define_variable(name_constant);

  assert_current_and_advance(TOKEN_LEFT_BRACE, "expected '{'");
  assert_current_and_advance(TOKEN_RIGHT_BRACE, "expected '}'");
}

static void statement(bool _) {
  if (match(TOKEN_CLASS)) {
    class_declaration();
  } else if (match(TOKEN_RETURN)) {
#ifdef DONT_ALLOW_RETURNS_IN_TOP_LEVEL
    if (current->function_type == TYPE_SCRIPT) {
      error_at_previous("Can't return at top level");
      return;
    }
#endif
    if (match(TOKEN_SEMICOLON)) {
      emit_empty_return();
    } else {
      expression();
      emit_byte(OP_RETURN);
      assert_current_and_advance(TOKEN_SEMICOLON, "Expected ';' after return");
    }
  } else if (match(TOKEN_PRINT)) {
    user_level_statement(OP_PRINT);
  } else if (match(TOKEN_ASSERT)) {
    user_level_statement(OP_ASSERT);
  } else if (match(TOKEN_FUN)) {
    fun_declaration();
  } else if (match(TOKEN_WHEN)) {
    when_statement();
  } else if (match(TOKEN_IF)) {
    if_statement();
  } else if (match(TOKEN_WHILE)) {
    while_statement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    begin_scope();
    block();
    end_scope();
  } else if (match(TOKEN_FOR)) {
    for_statement();
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

ObjectFunction* compile(const char* source) {
  init_scanner(source);
  Compiler compiler;
  init_compiler(&compiler, TYPE_SCRIPT);
  parser.had_error = 0;
  parser.panic_mode = 0;
  advance();
  while (!match(TOKEN_EOF)) {
    declaration(false);
  }
  assert_current_and_advance(TOKEN_EOF, "Expected end of expression");
  ObjectFunction* fn = end_compiler();
  return parser.had_error ? NULL : fn;
}

/*

EXPLANATION ON HOW UPVALUE RESOLVING WORKS


                                     ┌───┐      ┌────────────┐
                                     │...│      │ It's here! │
                                     └───┘      └───┬────────┘
                                                    │
                                ┌──────────────┐    │ ┌─────────────────────────┐
                                │  Function 2  │    │ │it's on the third element│
                                └──────────────┘    │ │       of my frame       │
                                        ▲    ┌──────┘ └─────────────────────────┘
                                        │    │         ┌────────────────────────────┐
┌────────────────────────────────────┐  │    │         │                            │
│    resolve_upvalue("Variable");    │  │    │         │   Will try to store the    │
└────────────────────────────────────┘  │    │         │      upvalue asked by      │
                                        │    │         │  the function below, then  │
                                ┌────────────▼─┐       │ return the index where it  │
                                │  Function 3  │       │         stored it.         │
                                └────────────┬─┘       │                            │
                                        ▲    │         └────────────────────────────┘
                                        │    │
  ┌────────────────────────────────────┐│   ┌┘  ┌──────────────────────────────────────────────────────────┐
  │    resolve_upvalue("Variable");    ││   │   │ it's on the 6th element of my frame! So when you lookup  │
  └────────────────────────────────────┘│   │   │      for it in the Virtual Machine it will be there      │
                                        │   │   └──────────────────────────────────────────────────────────┘
                                ┌───────────▼───┐
                                │  Function 4   │
                                └───────────────┘
                            ┌───────────────────────┐
                            │Understood, I will save│
                            │ it on the 1st element │
                            │of my frame, and store │
                            │ in which index is in  │
                            │      your frame!      │
                            └───────────────────────┘
** VIRTUAL MACHINE **

 fun a() { --> nothing
  var x = 0; --> nothing
  fun b () { --> saves into slot 0 local variable X stored in current frame
    fun c() { --> saves into slot 0 external variable X stored in b slot 0
      fun d() { --> saves into slot 0 external variable X stored in c slot 0
        print x; --> OP_GET_UPVALUE 0 -- current_frame.upvalues[0]
      }
      print x; --> OP_GET_UPVALUE 0
    }
  }
}
*/
