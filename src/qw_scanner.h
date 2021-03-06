#ifndef qw_scanner_h
#define qw_scanner_h

#include "qw_common.h"

typedef enum {
  // Single-character tokens.
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,
  TOKEN_RIGHT_BRACE,
  TOKEN_COMMA,
  TOKEN_DOT,
  TOKEN_MINUS,
  TOKEN_PLUS,
  TOKEN_SEMICOLON,
  TOKEN_SLASH,
  TOKEN_STAR,

  // One or two character tokens.
  TOKEN_BANG,
  TOKEN_BANG_EQUAL,
  TOKEN_EQUAL,
  TOKEN_EQUAL_EQUAL,
  TOKEN_GREATER,
  TOKEN_GREATER_EQUAL,
  TOKEN_LESS,
  TOKEN_LESS_EQUAL,

  // Literals.
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_NUMBER,

  // Keywords.
  TOKEN_AND,
  TOKEN_CLASS,
  TOKEN_ELSE,
  TOKEN_FALSE,
  TOKEN_FOR,
  TOKEN_FUN,
  TOKEN_IF,
  TOKEN_NIL,
  TOKEN_OR,
  TOKEN_PRINT,
  TOKEN_RETURN,
  TOKEN_SUPER,
  TOKEN_THIS,
  TOKEN_TRUE,
  TOKEN_VAR,
  TOKEN_WHILE,

  TOKEN_ERROR,
  TOKEN_EOF,
  // `
  TOKEN_STRING_LITERAL,
  TOKEN_DOLLAR,
  // TOKEN_STRING_LITERAL__PIECE,

  TOKEN_QUESTION,
  TOKEN_DOUBLE_DOT,

  TOKEN_LET,
  TOKEN_WHEN,
  TOKEN_BAR,
  TOKEN_AMPERSAND,
  TOKEN_DOUBLE_POINT,
  TOKEN_MINUS_ARROW,
  TOKEN_NOTHING,
  TOKEN_ASSERT,
  TOKEN_LEFT_BRACKET,
  TOKEN_RIGHT_BRACKET,
} TokenType;

typedef struct {
  TokenType type;
  const char* start;
  u32 length;
  u32 line;
} Token;

void init_scanner(const char* source);
Token scan_token(void);

#endif
