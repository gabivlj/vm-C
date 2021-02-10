#ifndef qw_compiler_h
#define qw_compiler_h
#include "qw_chunk.h"
#include "qw_common.h"
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

#define UINT16_COUNT (UINT8_MAX + 1)

typedef struct {
  Token name;
  i32 depth;
  bool mutable;
  bool is_captured;
} Local;

typedef struct {
  u8 index;
  bool is_local;
} Upvalue;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
  // The compiler which was called brom
  struct Compiler* enclosing_compiler;
  ObjectFunction* function;
  FunctionType function_type;
  ValueArray* globals;
  Local locals[UINT16_COUNT];
  i32 local_count;
  i32 scope_depth;
  Upvalue upvalues[UINT8_MAX];
} Compiler;

extern Table symbol_table;
extern Compiler* current;
ObjectFunction* compile(const char* source);

void mark_compiler_roots();

#endif