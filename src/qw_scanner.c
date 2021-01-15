#include "qw_scanner.h"

#include <stdio.h>
#include <string.h>

#include "qw_common.h"

typedef struct {
  const char* start;
  const char* current;
  int line;
} Scanner;

Scanner scanner;

void init_scanner(const char* source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

#define IS_END (*scanner.current == '\0')
#define PEEK (*scanner.current)
#define PEEK_NEXT (IS_END ? '\0' : scanner.current[1])

static Token make_token(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (u32)(scanner.current - scanner.start);
  token.line = scanner.line;
  return token;
}

static Token error_token(const char* message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.line = scanner.line;
  token.start = message;
  token.length = (u32)strlen(message);
  return token;
}

static char advance() {
  scanner.current++;
  return *(scanner.current - 1);
}

/// Unwraps the token, if the peek is equal to the passed char,
/// will return `if_equal` and advance the scanner, else just return alternative
#define MATCH_PEEK(c, if_equal, alternative) \
  (c == *(scanner.current) && !IS_END && ++scanner.current ? if_equal : alternative)

#define IS_DIGIT(c) (c >= '0' && c <= '9')

#define IS_ALPHA(c) ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')

static void skip_whitespace() {
  char c = PEEK;
  while ((c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '/') && !IS_END) {
    if (c == '\n') {
      scanner.line++;
    } else if (c == '/') {
      // Skip if it's a comment
      if (PEEK_NEXT == '/') {
        while (PEEK != '\n' && !IS_END) {
          advance();
        }
        continue;
      } else {
        break;
      }
    }
    advance();
    c = PEEK;
  }
}

static Token string() {
  while (PEEK != '"') {
    if (PEEK == '\0') return error_token("Expected '\"', instead got EOF");
    if (PEEK == '\n') {
      scanner.line++;
    }
    advance();
  }
  // Consume `"`
  advance();
  return make_token(TOKEN_STRING);
}

static Token number() {
  while (IS_DIGIT(PEEK)) {
    advance();
  }
  if (PEEK == '.' && IS_DIGIT(PEEK_NEXT)) {
    // Consume '.'
    advance();
    // Handle decimal
    while (IS_DIGIT(PEEK)) {
      advance();
    }
  }
  return make_token(TOKEN_NUMBER);
}

static TokenType identifier_type() { return TOKEN_IDENTIFIER; }

static Token identifier() {
  while (IS_ALPHA(PEEK) || IS_DIGIT(PEEK)) advance();
  return make_token(identifier_type());
}

Token scan_token() {
  skip_whitespace();
  if (IS_END) {
    return make_token(TOKEN_EOF);
  }
  scanner.start = scanner.current;
  char current_character = advance();
  if (IS_DIGIT(current_character)) {
    return number();
  }
  if (IS_ALPHA(current_character)) {
    return identifier();
  }
  // printf("%c \n", current_character);
  switch (current_character) {
    case '(':
      return make_token(TOKEN_LEFT_PAREN);
    case ')':
      return make_token(TOKEN_RIGHT_PAREN);
    case '{':
      return make_token(TOKEN_LEFT_BRACE);
    case '}':
      return make_token(TOKEN_RIGHT_BRACE);
    case ';':
      return make_token(TOKEN_SEMICOLON);
    case ',':
      return make_token(TOKEN_COMMA);
    case '.':
      return make_token(TOKEN_DOT);
    case '-':
      return make_token(TOKEN_MINUS);
    case '+':
      return make_token(TOKEN_PLUS);
    case '/':
      return make_token(TOKEN_SLASH);
    case '*':
      return make_token(TOKEN_STAR);
    case '!':
      return make_token(MATCH_PEEK('=', TOKEN_BANG_EQUAL, TOKEN_BANG));
    case '=':
      return make_token(MATCH_PEEK('=', TOKEN_EQUAL_EQUAL, TOKEN_EQUAL));
    case '>':
      return make_token(MATCH_PEEK('=', TOKEN_GREATER_EQUAL, TOKEN_GREATER));
    case '<':
      return make_token(MATCH_PEEK('=', TOKEN_LESS_EQUAL, TOKEN_LESS));
    case '"': {
      return string();
    }
  }
  return error_token("unknown token");
}