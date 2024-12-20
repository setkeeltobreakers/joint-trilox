#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "scanner.h"
#include "config.h"

typedef struct {
  char *start;
  char *current;
  int line;
} Scanner_struct;

static Scanner_struct scanner;

void initScanner(char *source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

static Token makeToken(tokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int) (scanner.current - scanner.start);
  token.line = scanner.line;

  return token;
}

static Token errorToken(char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int) strlen(message);
  token.line = scanner.line;

  return token;
}

static int isAtEnd() {
  //if (*scanner.current == '\0') printf("Pleh\n");
  return *scanner.current == '\0';
}

static char advance() {
  scanner.current++;
  return scanner.current[-1];
}

static int match(char expected) {
  if (isAtEnd()) return 0;
  if (*scanner.current != expected) return 0;
  scanner.current++;
  return 1;
}

static char peek() {
  return *scanner.current;
}

static char peekNext() {
  if (isAtEnd()) return '\0';
  return scanner.current[1];
}

static void skipWhitespace() {
  while (1) {
    char c = peek(); /* Gets the next character, but doesn't consume it */
    switch (c) { /* Runs mini scanner routine */
    case ' ':
    case '\r':
    case '\t':
      advance(); /* Do the same for tabs as everything above */
      break;
    case '\n':
      advance(); 
      scanner.line++; /* Incriment line number when scanner hits a newline character */
      break;
    case '#': /* Comments */
      while (peek() != '\n' && !isAtEnd()) advance(); /* Consume all characters except for \n in a comment */
      break;
    default: /* Return once scanner hits non-whitespace character*/
      return;
    }
  }
}

static int isAlpha(char c) {
  return (c >= 'a' && c <= 'z') ||
    (c >= 'A' && c <= 'Z') ||
    c == '_';
}

static int isDigit(char c) {
  return (c >= '0' && c <= '9');
}

static tokenType checkKeyword(int start, int length, char *rest, tokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }

  return TOKEN_IDENTIFIER;
}

static tokenType identifierType() {
  if (scanner.current - scanner.start == 1) return TOKEN_IDENTIFIER;
  /* Return identifier token if token is only one letter. */
  
  switch (*scanner.start) {
  case 'p': return checkKeyword(1, 6, "rogram", TOKEN_PROGRAM);
  case 'd': {
    switch (scanner.start[1]) {
    case 'e': return checkKeyword(2, 5, "fault", TOKEN_DEFAULT);
    case 'o': return checkKeyword(1, 1, "o", TOKEN_DO);
    }
  }
  case 'e': switch (scanner.start[1]) {
    case 'n': return checkKeyword(2, 1, "d", TOKEN_END_DECL);
    case 'a': return checkKeyword(2, 2, "ch", TOKEN_EACH);
    case 'l': return checkKeyword(2, 2, "se", TOKEN_ELSE);
    } break;
  case 'f': {
    switch (scanner.start[1]) {
    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
    case 'u': return checkKeyword(2, 6, "nction", TOKEN_FUNCTION);
    case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
    } break;
  }
  case 'a': {
    switch (scanner.start[1]) {
    case 'n': return checkKeyword(2, 1, "d", TOKEN_AND);
    case 't': return checkKeyword(2, 2, "om", TOKEN_ATOM);
    } break;
  }
  case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
  case 's': {
    switch (scanner.start[1]) {
    case 't': return checkKeyword(2, 3, "ate", TOKEN_STATE_DECL);
    case 'w': return checkKeyword(2, 4, "itch", TOKEN_SWITCH);
    }
  }
  case 'n': {
    switch (scanner.start[1]) {
    case 'o': return checkKeyword(2,1, "t", TOKEN_NOT);
    case 'i': return checkKeyword(2,1, "l", TOKEN_NIL);
    } break;
  }
  case 'x': return checkKeyword(1, 2, "or", TOKEN_XOR);
  case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
  case 'c': {
    switch(scanner.start[1]) {
    case 'a': return checkKeyword(2, 2, "se", TOKEN_CASE);
    case 'o': {
      switch (scanner.start[2]) {
      case 'm': return checkKeyword(3, 4, "pare", TOKEN_COMPARE);
      case 'n': {
	switch (scanner.start[3]) {
	case 't': return checkKeyword(4, 4, "inue", TOKEN_CONTINUE);
	case 's': return checkKeyword(4, 4, "ider", TOKEN_CONSIDER);
	}
      }
      }
    }
    }
  } break;
  case 'w': if (scanner.start[1] == 'h') {
      switch (scanner.start[2]) {
      case 'i': return checkKeyword(3, 2, "le", TOKEN_WHILE);
      case 'e': return checkKeyword(2, 2, "en", TOKEN_WHEN);
      }
    }
  case 'i': {
    switch (scanner.start[1]) {
    case 'n': return checkKeyword(1, 1, "n", TOKEN_IN);
    case 'f': return checkKeyword(1, 1, "f", TOKEN_IF);
    } break;
  }
  case 't': {
    switch (scanner.start[1]) {
    case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
    case 'a': return checkKeyword(2, 3, "ble", TOKEN_TABLE_DECL);
    }
  }
  case 'b': return checkKeyword(1, 4, "reak", TOKEN_BREAK);
  case 'u': return checkKeyword(1, 6, "nknown", TOKEN_UNKNOWN);
  }

  return TOKEN_IDENTIFIER;
}

static Token number() {
  while (isDigit(peek())) advance(); /* Manger manger! :flag_FR: */

  if (peek() == '.') advance(); /* Consume . even if there's nothing after it */
  
  while (isDigit(peek())) advance(); /* If there are digits afterwards, consume them too */

  return makeToken(TOKEN_NUMBER);
}

static Token string() {
  while(peek() != '"') {
    if (isAtEnd()) return errorToken("Unterminated string. Where's Arnold when you need him?");
    if (peek() == '\n') scanner.line++;

    advance();
  }

  advance(); /* Nab the ending quote */
  return makeToken(TOKEN_STRING);
}

static Token identifier() {
  while (isAlpha(peek()) || isDigit(peek())) advance();

  return makeToken(identifierType());
}

Token scanToken() {
  skipWhitespace();
  scanner.start = scanner.current;
  
  if (isAtEnd()) return makeToken(TOKEN_EOF);
  
  /* Central switch statement */
  char c = advance();

  if (isAlpha(c)) return identifier();
  if (isDigit(c)) return number();
  
  switch (c) {
  case '(': return makeToken(TOKEN_LEFT_PAREN);
  case ')': return makeToken(TOKEN_RIGHT_PAREN);
  case '{': return makeToken(TOKEN_LEFT_BRACE);
  case '}': return makeToken(TOKEN_RIGHT_BRACE);
  case '[': return makeToken(TOKEN_LEFT_SQUARE);
  case ']': return makeToken(TOKEN_RIGHT_SQUARE);
  case ',': return makeToken(TOKEN_COMMA);
  case '.': return makeToken(TOKEN_DOT);
  case ';': return makeToken(TOKEN_SEMICOLON);
  case ':': return makeToken(match('[') ? TOKEN_TABLE_OPEN : TOKEN_COLON);
  case '+': return makeToken(TOKEN_PLUS);
  case '-': return makeToken(TOKEN_MINUS);
  case '*': return makeToken(TOKEN_TIMES);
  case '/': return makeToken(TOKEN_DIVIDE);
  case '%': return makeToken(TOKEN_MODULO);
  case '^': return makeToken(TOKEN_EXPONENTIAL);
  case '<': return makeToken(match('=') ? TOKEN_LT_EQUAL : TOKEN_LESS_THAN);
  case '>': return makeToken(match('=') ? TOKEN_GT_EQUAL : TOKEN_GREAT_THAN);
  case '=': return makeToken(match('=') ? TOKEN_EQUAL : TOKEN_ASSIGN);
  case '!': return makeToken(match('=') ? TOKEN_NOT_EQUAL : TOKEN_NOT);
  case '"': return string();
  }
  
  return errorToken("Unexpected Character. Most unexpected indeed.");
}

