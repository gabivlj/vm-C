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

/// Returns TokenType `token_type` related to the passed keyword
/// `start` is the start idx for the scanner.start
/// `len` is the length of the `target`
/// `target` is the string literal to compare
/// `token_type` Expected keyword
static TokenType check_keyword(u8 start, u8 len, const char* target, TokenType token_type) {
  if (scanner.current - scanner.start != len + start) {
    return TOKEN_IDENTIFIER;
  }
  for (int i = start, j = 0; j < len; i++, j++) {
    if (target[j] != scanner.start[i]) {
      return TOKEN_IDENTIFIER;
    }
  }
  return token_type;
}

/// Returns the TokenType related to the current parsed identifier current start
static TokenType identifier_type() {
  switch (scanner.start[0]) {
    // Complex cases
    case 'f': {
      if (scanner.current - scanner.start <= 1) break;
      switch (scanner.start[1]) {
        case 'a':
          return check_keyword(2, 3, "lse", TOKEN_FALSE);
        case 'u':
          return check_keyword(2, 1, "n", TOKEN_FUN);
        case 'o':
          return check_keyword(2, 1, "r", TOKEN_FOR);
      }
      break;
    }
    case 't': {
      if (scanner.current - scanner.start <= 1) break;
      switch (scanner.start[1]) {
        case 'r':
          return check_keyword(2, 2, "ue", TOKEN_TRUE);
        case 'h':
          return check_keyword(2, 2, "is", TOKEN_THIS);
      }
      break;
    }
    // Simple cases
    case 'a':
      return check_keyword(1, 2, "nd", TOKEN_AND);
    case 'c':
      return check_keyword(1, 4, "lass", TOKEN_CLASS);
    case 'e':
      return check_keyword(1, 3, "lse", TOKEN_ELSE);
    case 'i':
      return check_keyword(1, 1, "f", TOKEN_IF);
    case 'n':
      return check_keyword(1, 2, "il", TOKEN_NIL);
    case 'o':
      return check_keyword(1, 1, "r", TOKEN_OR);
    case 'p':
      return check_keyword(1, 4, "rint", TOKEN_PRINT);
    case 'r':
      return check_keyword(1, 5, "eturn", TOKEN_RETURN);
    case 's':
      return check_keyword(1, 4, "uper", TOKEN_SUPER);
    case 'v':
      return check_keyword(1, 2, "ar", TOKEN_VAR);
    case 'w':
      return check_keyword(1, 4, "hile", TOKEN_WHILE);
  }
  return TOKEN_IDENTIFIER;
}

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
    case '?':
      return make_token(TOKEN_QUESTION);
    case ':':
      return make_token(TOKEN_DOUBLE_DOT);
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