#ifndef JOINT_SCANNER
#define JOINT_SCANNER

typedef enum {
  TOKEN_NIL,
  
  /* Bracket variations */
  TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
  TOKEN_LEFT_SQUARE, TOKEN_RIGHT_SQUARE,
  TOKEN_TABLE_OPEN,

  /* Seperator tokens */
  TOKEN_COMMA, TOKEN_DOT, TOKEN_SEMICOLON, TOKEN_COLON,

  /* Math operators */
  TOKEN_PLUS, TOKEN_MINUS, TOKEN_TIMES, TOKEN_DIVIDE, TOKEN_MODULO, TOKEN_EXPONENTIAL,

  /* Logic literals */
  TOKEN_FALSE, TOKEN_UNKNOWN, TOKEN_TRUE,

  /* Comparison operators */
  TOKEN_COMPARE,
  TOKEN_LESS_THAN, TOKEN_LT_EQUAL,
  TOKEN_GREAT_THAN, TOKEN_GT_EQUAL,
  TOKEN_EQUAL, TOKEN_NOT_EQUAL,

  /* Kleene/Priest operators */
  TOKEN_AND, TOKEN_OR, TOKEN_XOR, TOKEN_NOT,

  /* Control Flow statements */
  TOKEN_IF, TOKEN_WHILE, TOKEN_FOR, TOKEN_IN, TOKEN_DO, TOKEN_EACH, TOKEN_CONTINUE,
  TOKEN_TRUE_STMT, TOKEN_UNKNOWN_STMT, TOKEN_FALSE_STMT,
  
  /* Declarative keywords */
  TOKEN_PROGRAM, TOKEN_END_DECL, TOKEN_FUNCTION, TOKEN_ATOM, TOKEN_BLK_NAME,
  TOKEN_BLK_DECL, TOKEN_VAR, TOKEN_ASSIGN, TOKEN_STATE_DECL,

  /* Literals */
  TOKEN_IDENTIFIER, TOKEN_STRING, TOKEN_NUMBER,

  /* Special stuff */
  TOKEN_ERROR, TOKEN_EOF
} tokenType;

typedef struct {
  tokenType type;
  char *start;
  int length;
  int line;
} Token;

 void initScanner(char *source);
 Token scanToken();

#endif
