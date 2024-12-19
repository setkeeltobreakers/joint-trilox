#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "value.h"
#include "object.h"
#include "memory.h"
#include "library.h"

typedef struct {
  Token current;
  Token previous;
  Token prevNext;
  int hadError;
  int panicMode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,  
  PREC_OR,          
  PREC_AND,         
  PREC_EQUALITY,    
  PREC_COMPARISON,
  PREC_MODULO,
  PREC_ADDSUB,        
  PREC_MULTDIV,
  PREC_EXPONENTIAL,
  PREC_UNARY,       
  PREC_CALL,        
  PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)(int canAssign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  int isCaptured;
} Local;

typedef struct {
  uint8_t index;
  int isLocal;
} Upvalue;

typedef enum {
  TYPE_FUNCTION,
  TYPE_SCRIPT
} FunctionType;

typedef struct Compiler Compiler;

struct Compiler {
  Compiler *enclosing;
  ObjFunction *function;
  FunctionType type;
  
  Local locals[UINT8_MAX + 1];
  int localCount;
  Upvalue upvalues[UINT8_MAX + 1];
  int scopeDepth;
  int loopLevel;
  int loopStarts[MAX_LOOP_NESTING];
};

static void unary(int canAssign);
static void grouping(int canAssign);
static void binary(int canAssign);
static void number(int canAssign);
static void string(int canAssign);
static void nil(int canAssign);
static void logic_lit(int canAssign);
static void variable(int canAssign);
static void call(int canAssign);
static void atom(int canAssign);
static void array(int canAssign);
static void accessArray(int canAssign);
static void hashTable(int canAssign);
static void tableCalculatedAccess(int canAssign);

static void declaration();
static void statement();
static void expression();

ParseRule rules[] = {
  [TOKEN_NIL] = {nil, NULL, PREC_NONE},
  [TOKEN_LEFT_PAREN] = {grouping, call, PREC_CALL},
  [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
  [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
  [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
  [TOKEN_LEFT_SQUARE] = {array, accessArray, PREC_CALL},
  [TOKEN_RIGHT_SQUARE] = {NULL, NULL, PREC_NONE},
  [TOKEN_TABLE_OPEN] = {hashTable, tableCalculatedAccess, PREC_CALL},
  [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
  [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
  [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
  [TOKEN_COLON] = {NULL, NULL, PREC_NONE},
  [TOKEN_MINUS] = {unary, binary, PREC_ADDSUB},
  [TOKEN_PLUS] = {NULL, binary, PREC_ADDSUB},
  [TOKEN_TIMES] = {NULL, binary, PREC_MULTDIV},
  [TOKEN_DIVIDE] = {NULL, binary, PREC_MULTDIV},
  [TOKEN_MODULO] = {NULL, binary, PREC_MODULO},
  [TOKEN_EXPONENTIAL] = {NULL, binary, PREC_EXPONENTIAL},
  [TOKEN_FALSE] = {logic_lit, NULL, PREC_NONE},
  [TOKEN_UNKNOWN] = {logic_lit, NULL, PREC_NONE},
  [TOKEN_TRUE] = {logic_lit, NULL, PREC_NONE},
  [TOKEN_COMPARE] = {NULL, binary, PREC_COMPARISON},
  [TOKEN_LESS_THAN] = {NULL, binary, PREC_COMPARISON},
  [TOKEN_LT_EQUAL] = {NULL, binary, PREC_COMPARISON},
  [TOKEN_GREAT_THAN] = {NULL, binary, PREC_COMPARISON},
  [TOKEN_GT_EQUAL] = {NULL, binary, PREC_COMPARISON},
  [TOKEN_EQUAL] = {NULL, binary, PREC_COMPARISON},
  [TOKEN_NOT_EQUAL] = {NULL, binary, PREC_COMPARISON},
  [TOKEN_AND] = {NULL, binary, PREC_AND},
  [TOKEN_OR] = {NULL, binary, PREC_OR},
  [TOKEN_XOR] = {NULL, binary, PREC_OR},
  [TOKEN_NOT] = {unary, NULL, PREC_UNARY},
  [TOKEN_IF] = {NULL, NULL, PREC_NONE},
  [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
  [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
  [TOKEN_IN] = {NULL, NULL, PREC_NONE},
  [TOKEN_PROGRAM] = {NULL, NULL, PREC_NONE},
  [TOKEN_END_DECL] = {NULL, NULL, PREC_NONE},
  [TOKEN_FUNCTION] = {NULL, NULL, PREC_NONE},
  [TOKEN_ATOM] = {atom, NULL, PREC_PRIMARY},
  [TOKEN_BLK_NAME] = {NULL, NULL, PREC_NONE},
  [TOKEN_BLK_DECL] = {NULL, NULL, PREC_NONE},
  [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
  [TOKEN_ASSIGN] = {NULL, NULL, PREC_NONE},
  [TOKEN_STATE_DECL] = {NULL, NULL, PREC_NONE},
  [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
  [TOKEN_STRING] = {string, NULL, PREC_NONE},
  [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
  [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
  [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

Parser parser;
Token nulltoken = (Token) {TOKEN_ERROR, NULL, 0, 0};
Compiler *current = NULL;
VM *vm;

static Chunk *currentChunk() {
  return &current->function->chunk;
}

static void consume(tokenType type, char *message);
static int match(tokenType type);
static int check(tokenType type);
static int checkNewLine();
static void advance();
static ParseRule *getRule(tokenType type);
static void parsePrecedence(Precedence precedence);

static void initParser() {
  parser.current = nulltoken;
  parser.previous = nulltoken;
  parser.prevNext = nulltoken;
  parser.hadError = 0;
  parser.panicMode = 0;
}

static void initCompiler(Compiler *compiler, FunctionType type) {
  compiler->enclosing = current;
  compiler->function = NULL;
  compiler->type = type;
  compiler->localCount = 0;
  compiler->scopeDepth = 0;
  compiler->loopLevel = 0;
  compiler->function = newFunction(vm);
  current = compiler;

  if (type != TYPE_SCRIPT) {
    current->function->name = copyString(parser.previous.start, parser.previous.length, vm);
  }
  
  Local *local = &current->locals[current->localCount++];
  local->depth = 0;
  local->name.start = "";
  local->name.length = 0;
}

void scan(char *source) {
  initScanner(source);

  int line = -1;
  while (1) {
    Token token = scanToken();

    if (token.type == TOKEN_EOF) {
      printf("   EOF\n");
      break;
    }
    
    if (token.line != line) {
      printf("%4d ", token.line);
      line = token.line;
    } else {
      printf("   | ");
    }
    printf("%2d '%.*s'\n", token.type, token.length, token.start);
  }
}

static void errorAt(Token *token, char *message) {
  if (parser.panicMode) return;
  
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    /* Nada */
  } else {
    fprintf(stderr, " at %.*s", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.hadError = 1;
  parser.panicMode = 1;
}

static void errorAtCurrent(char *message) {
  errorAt(&parser.current, message);
}

static void emitByte(uint8_t byte) {
  writeChunk(currentChunk(), byte, parser.previous.line, vm);
}

static void emitReturn() {
  emitByte(OP_NIL);
  emitByte(OP_RETURN);
}

static void emitBytePair(uint8_t byte1, uint8_t byte2) {
  emitByte(byte1);
  emitByte(byte2);
}

static void emitByteLong(uint8_t byte1, uint16_t long1) {
  emitByte(byte1);
  emitByte((long1 >> 8) & 0xff);
  emitByte(long1 & 0xff);
}

static int emitJump(uint8_t instruction) {
  emitByte(instruction);
  emitByte(0xff);
  emitByte(0xff);
  return currentChunk()->count - 2;
}

static void emitLoop(int loopStart) {
  emitByte(OP_LOOP);

  int offset = currentChunk()->count - loopStart + 2;
  if (offset > UINT16_MAX) errorAtCurrent("Don't get me started on them big ass loops in this program.");

  emitByte((offset >> 8) & 0xff);
  emitByte(offset & 0xff);
  
}

static uint16_t makeConstant(Value value) {
  int constant = addConstant(currentChunk(), value, vm);
  if (constant > UINT16_MAX) {
    errorAtCurrent("Too many constants");
    return 0;
    }
  
  return (uint16_t) constant;
}

static void emitConstant(Value value) {
  uint16_t constant = makeConstant(value);
  if (constant > UINT8_MAX) {
    emitByteLong(OP_CONSTANT_16, constant);
  } else {
    emitBytePair(OP_CONSTANT, (uint8_t) constant);
  }
}

static void emitCustomConstant(Value value, OpCode op, OpCode op16) {
  uint16_t constant = makeConstant(value);
  if (constant > UINT8_MAX) {
    emitByteLong(op16, constant);
  } else {
    emitBytePair(op, (uint8_t) constant);
  }
}

static void emitVariableLength(uint16_t constant, OpCode op, OpCode op16) {
  if (constant > UINT8_MAX) {
    emitByteLong(op16, constant);
  } else {
    emitBytePair(op, (uint8_t) constant);
  }
}

static void patchJump(int offset) {
  /* -2 to adjust for the bytecode for the jump offset itselt. */
  int jump = currentChunk()->count - offset - 2;

  if (jump > UINT16_MAX) {
    errorAtCurrent("Programmer, you smoke too tough, your swag too different, your jumps too large, the program can't compile. :(");
  }

  currentChunk()->code[offset] = (jump >> 8) & 0xff;
  currentChunk()->code[offset + 1] = jump & 0xff;
}

static uint16_t identifierConstant(Token *name) {
  return makeConstant(OBJECT_VAL(copyString(name->start, name->length, vm)));
}

static void addLocal(Token name) {
  if (current->localCount > UINT8_MAX) {
    errorAtCurrent("Too many local variables in a function");
    return;
  }

  Local *local = &current->locals[current->localCount++];
  local->name = name;
  local->depth = -1;
  local->isCaptured = 0;
}

static int identifiersEqual(Token *a, Token *b) {
  if (a->length != b->length) return 0;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
  for (int i = compiler->localCount - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifiersEqual(name, &local->name)) {
      if (local->depth == -1) {
	errorAtCurrent("Can't read local variable in its own initializer. Duh!");
      }
      return i;
    }
  }

  return -1;
}

static int addUpvalue(Compiler *compiler, uint8_t index, int isLocal) {
  int upvalueCount = compiler->function->upvalueCount;
  
  for (int i = 0; i < upvalueCount; i++) {
    Upvalue *upvalue = &compiler->upvalues[i];
    if (upvalue->index == index && upvalue->isLocal == isLocal) {
      return i;
    }
  }

  if (upvalueCount == UINT8_MAX + 1) {
    errorAtCurrent("Too many closure variables in function.");
    return 0;
  }
  
  compiler->upvalues[upvalueCount].isLocal = isLocal;
  compiler->upvalues[upvalueCount].index = index;
  return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name) {
  if (compiler->enclosing == NULL) return -1;

  int local = resolveLocal(compiler->enclosing, name);
  if (local != -1) {
    compiler->enclosing->locals[local].isCaptured = 1;
    return addUpvalue(compiler, (uint8_t) local, 1);
  }

  int upvalue = resolveUpvalue(compiler->enclosing, name);
  if (upvalue != -1) {
    return addUpvalue(compiler, (uint8_t) upvalue, 0);
  }

  return -1;
}

static void declareVariable() {
  if (current->scopeDepth == 0) {
    return;
  }

  Token *name = &parser.previous;
  for (int i = current->localCount - 1; i >= 0; i--) {
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scopeDepth) {
      break;
    }

    if (identifiersEqual(name, &local->name)) {
      errorAtCurrent("Already a local variable with this name in function. ");
    }
  }

  addLocal(*name);
}

static ObjFunction *endCompiler() {
  //emitReturn();
  ObjFunction *function = current->function;

  if (parser.hadError || DEBUG_PRINT_BYTECODE) {
    disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
  }
  
  current = current->enclosing;
  return function;
}

static void beginScope() {
  current->scopeDepth++;
}

static void endScope() {
  current->scopeDepth--;

  while (current->localCount > 0 && current->locals[current->localCount -1].depth > current->scopeDepth) {
    if (current->locals[current->localCount - 1].isCaptured) {
      emitByte(OP_CLOSE_UPVALUE);
    } else {
      emitByte(OP_POP);
    }
    current->localCount--;
  }
}

static ParseRule *getRule(tokenType type) {
  return &rules[type];
}

static void synchronize() {
  parser.panicMode = 0;

  while (parser.current.type != TOKEN_EOF) {
    if (checkNewLine()) return;
    switch (parser.current.type) {
    case TOKEN_PROGRAM:
    case TOKEN_END_DECL:
    case TOKEN_FUNCTION:
    case TOKEN_ATOM:
    case TOKEN_BLK_DECL:
    case TOKEN_VAR:
    case TOKEN_STATE_DECL:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_FOR:
      return;
    default:
      ;
    }
    
    advance();
  }
}

static uint16_t parseVariable(char *errorMessage) {
  consume(TOKEN_IDENTIFIER, errorMessage);

  declareVariable();
  if (current->scopeDepth > 0) return 0;
  
  return identifierConstant(&parser.previous);
}

static void markInitialized() {
  if (current->scopeDepth == 0) return;
  current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint16_t global) {
  if (current->scopeDepth > 0) {
    markInitialized();
    return;
  }
  if (global > UINT8_MAX) {
    emitByteLong(OP_DEFINE_GLOBAL_16, global);
  } else {
    emitBytePair(OP_DEFINE_GLOBAL, (uint8_t) global);
  }
}

static uint8_t argumentList() {
  uint8_t argCount = 0;
  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      expression();
      if (argCount >= MAX_ARITY) errorAtCurrent("Too many arguments passed to function.");
      argCount++;
    } while (match(TOKEN_COMMA));
  }
  
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
  return argCount;
}

static void expression() {
  parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }
  
  consume(TOKEN_RIGHT_BRACE, "Expect block end here.");
}

static void body() {
  /* This function is sposed to be used inside functions and programs. */
  while (!check(TOKEN_END_DECL) && !check(TOKEN_EOF)) {
    declaration();
  }

  consume(TOKEN_END_DECL, "This (should be) the end, my only friend, the end.");
}

static void function(FunctionType type) {
  Compiler compiler;
  initCompiler(&compiler, type);
  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expect ( before function input parameters.");

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > MAX_ARITY) {
	errorAtCurrent("Too many input parameters for function.");
      }
      uint16_t constant = parseVariable("Expect parameter name.");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expect ) after function input paremeters.");
  body();
  
  if (match(TOKEN_LEFT_PAREN)) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after return expression.");
    emitByte(OP_RETURN);
  } else {
    emitReturn();
  }
  ObjFunction *function = endCompiler();
  emitCustomConstant(OBJECT_VAL(function), OP_CLOSURE, OP_CLOSURE_16);
  
  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void atom(int canAssign) {
  Compiler compiler;
  initCompiler(&compiler, TYPE_FUNCTION);
  beginScope();

  consume(TOKEN_LEFT_PAREN, "Expect ( before function input parameters.");

  if (!check(TOKEN_RIGHT_PAREN)) {
    do {
      current->function->arity++;
      if (current->function->arity > MAX_ARITY) {
	errorAtCurrent("Too many input parameters for function.");
      }
      uint16_t constant = parseVariable("Expect parameter name.");
      defineVariable(constant);
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RIGHT_PAREN, "Expect ) after function input paremeters.");
  // body();
  
  consume(TOKEN_LEFT_PAREN, "Expect '(' in atom.");
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' in atom.");
  emitByte(OP_RETURN);

  ObjFunction *function = endCompiler();
  emitCustomConstant(OBJECT_VAL(function), OP_CLOSURE, OP_CLOSURE_16);
  
  for (int i = 0; i < function->upvalueCount; i++) {
    emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
    emitByte(compiler.upvalues[i].index);
  }
}

static void functionDeclaration() {
  uint16_t global = parseVariable("Expect function name.");
  markInitialized();
  function(TYPE_FUNCTION);
  defineVariable(global);
}

static void checkEndStatement() {
  if (!checkNewLine()) {
    if (!match(TOKEN_SEMICOLON)) {
      if (!check(TOKEN_COMMA)) {
	if (!check(TOKEN_RIGHT_PAREN)) {
	  if (!check(TOKEN_END_DECL)) {
	    if (!check(TOKEN_RIGHT_BRACE)) {
	      errorAtCurrent("Expected end of expression");
	    }
	  }
	}
      }
    }
  }
}

static void expressionStatement() {
  expression();
  checkEndStatement();
  emitByte(OP_POP);
}

static void ifStatement() {
  expression();
  consume(TOKEN_DO, "Expect 'do' after condition.");
  
  int trueJump = 0; // = emitJump(OP_JUMP_IF_TRUE); // Unnecessary
  int unknownJump = 0; // = emitJump(OP_JUMP_IF_UNKNOWN);
  int falseJump = 0; // = emitJump(OP_JUMP_IF_FALSE);

  int endTrueJump = 0;
  int endUnknownJump = 0;
  int endFalseJump = 0;
  if (parser.current.type == TOKEN_TRUE || parser.current.type == TOKEN_UNKNOWN || parser.current.type == TOKEN_FALSE) {
    trueJump = emitJump(OP_JUMP_IF_TRUE); // Unnecessary
    unknownJump = emitJump(OP_JUMP_IF_UNKNOWN);
    falseJump = emitJump(OP_JUMP_IF_FALSE);
    
    for (int i = 0; i < 3; i++) {
      int shouldBreak = 0;
      switch(parser.current.type) {
      case TOKEN_TRUE: {
	advance();
	if (parser.current.type != TOKEN_COLON) errorAtCurrent("Expected ':' after logical block opener.");
	advance();
	patchJump(trueJump);
	emitByte(OP_POP);
	body();
	endTrueJump = emitJump(OP_JUMP);
      } break;
      case TOKEN_UNKNOWN: {
	advance();
	if (parser.current.type != TOKEN_COLON) errorAtCurrent("Expected ':' after logical block opener.");
	advance();
	patchJump(unknownJump);
	emitByte(OP_POP);
	body();
	endUnknownJump = emitJump(OP_JUMP);
      } break;
      case TOKEN_FALSE: {
	advance();
	if (parser.current.type != TOKEN_COLON) errorAtCurrent("Expected ':' after logical block opener.");
	advance();
	patchJump(falseJump);
	emitByte(OP_POP);
	body();
	endFalseJump = emitJump(OP_JUMP);
      } break;
      default: {
	shouldBreak = 1;
      }
      }
      if (shouldBreak) break;
    }

    
    
  } else {
    unknownJump = emitJump(OP_JUMP_IF_UNKNOWN);
    falseJump = emitJump(OP_JUMP_IF_FALSE);
    
    emitByte(OP_POP);
    statement();
    endTrueJump = emitJump(OP_JUMP);
    
    patchJump(unknownJump);
    emitByte(OP_POP);
    if (match(TOKEN_COMMA)) {
      if (!check(TOKEN_COMMA)) statement();
    }
    endUnknownJump = emitJump(OP_JUMP);
    
    patchJump(falseJump);
    emitByte(OP_POP);
    if (match(TOKEN_COMMA)) statement();
    //endFalseJump = emitJump(OP_JUMP);
    
  }
  if (endTrueJump != 0) {
    patchJump(endTrueJump);
  } else {
    patchJump(trueJump);
  }

  if (endUnknownJump != 0) {
    patchJump(endUnknownJump);
  } else {
    patchJump(unknownJump);
  }

  if (endFalseJump != 0) {
    patchJump(endFalseJump);
  } else {
    if (trueJump != 0) patchJump(falseJump);
  }
}

static void whileStatement() {
  current->loopLevel++;
  if (current->loopLevel > MAX_LOOP_NESTING) errorAtCurrent("Too many nested loops. What are you, a bird?");
  int loopStart = currentChunk()->count;
  current->loopStarts[current->loopLevel - 1] = loopStart;
  expression();
  consume(TOKEN_DO, "Expect 'do' after condition");

  int exitJump = emitJump(OP_JUMP_IF_FALSE);
  int unknownJump = emitJump(OP_JUMP_IF_UNKNOWN);
  emitByte(OP_POP);

  declaration();
  
  emitLoop(loopStart);

  patchJump(unknownJump);
  if (match(TOKEN_COMMA)) {
    declaration();
  }
  
  patchJump(exitJump);
  emitByte(OP_POP);
  current->loopLevel--;
}

static void eachStatement() {
  current->loopLevel++;
  beginScope();

  consume(TOKEN_IDENTIFIER, "Expect loop variable name after 'each'.");
  /* Creating the loop var is tricky. The syntax is completely different to
     a normal declaration, so we can't just reuse the same function. The logic
     is also a little different. The loop variable will never be global, so 
     we can just jump straight to defining it as a local. 
   */
  Token loopCounterToken = (Token) {TOKEN_IDENTIFIER, "counter", 7, 0};
  
  addLocal(loopCounterToken);
  markInitialized();
  addLocal(parser.previous);
  markInitialized();
  uint8_t loopCounter = resolveLocal(current, &loopCounterToken);
  uint8_t loopVar = resolveLocal(current, &parser.previous);
  
  /* Add a local variable for both the loop variable, and an loop counter variable. 
     The loop counter variable is an internal variable, but there's no way to make it
     unavailable to the user, except perhaps just giving it an obtuse name, but that's
     not really necessary. */
  
  emitByte(OP_PUSH_1);
  emitByte(OP_PUSH_1);
  
  consume(TOKEN_IN, "Expect 'in' after loop variable.");

  expression(); /* Must be an array. */
  
  consume(TOKEN_DO, "Expect 'do' after loop variable");

  
  int loopStart = currentChunk()->count; /* Mark the loop start */
  current->loopStarts[current->loopLevel - 1] = loopStart;
  
  emitByte(OP_GET_ARRAY_COUNT);
  emitBytePair(OP_GET_LOCAL, loopCounter);
  emitByte(OP_KP_GT_EQUAL); /* Compare the loop counter to the array count. */
  
  int exitJump = emitJump(OP_JUMP_IF_FALSE); /* Set up the exit jump. */

  emitByte(OP_POP); /* Remove the comparison result from the stack. */
  
  emitBytePair(OP_GET_LOCAL, loopCounter);
  emitByte(OP_GET_ARRAY_LOOP);
  emitBytePair(OP_SET_LOCAL, loopVar); /* Get the value from the array and put it in the loop variable. */
  emitByte(OP_POP);

  declaration();

  emitByte(OP_PUSH_1);
  emitBytePair(OP_GET_LOCAL, loopCounter);
  emitByte(OP_ADD);
  emitBytePair(OP_SET_LOCAL, loopCounter);
  emitByte(OP_POP);

  emitLoop(loopStart);
  
  patchJump(exitJump);

  emitByte(OP_POP);
  emitByte(OP_POP); /* Need two pops to get the array off the stack. */
  
  endScope();
  current->loopLevel--;
}

static void continueStatement() { /* TODO: Change things so locals inside a scope that is 
				     entirely internal to the loop body (not the loop scope 
				     itself) get discarded properly when this statement is 
				     called. */
  if (current->loopLevel < 1) {
    errorAtCurrent("Tried to use 'continue' outside of a loop.");
    return;
  }
  
  Token loopCounterToken = (Token) {TOKEN_IDENTIFIER, "counter", 7, 0};
  int counterCheck = resolveLocal(current, &loopCounterToken);

  if (counterCheck != -1) {
    emitBytePair(OP_GET_LOCAL, counterCheck);
    emitByte(OP_PUSH_1);
    emitByte(OP_ADD);
    emitBytePair(OP_SET_LOCAL, counterCheck);
    emitByte(OP_POP);
  }

  int jump = currentChunk()->count - current->loopStarts[current->loopLevel - 1] + 3;
  if (jump > UINT16_MAX) {
    errorAtCurrent("Oversized loop comin' through!");
  }
  
  
  emitByte(OP_LOOP);
  emitByte((jump >> 8) & 0xff);
  emitByte(jump & 0xff);
}

static void considerStatement() {
  int whenEndingJumps[256];
  int jumps = 0;
  
  while (match(TOKEN_WHEN)) {
    expression();
    consume(TOKEN_DO, "Expect 'do' after when conditional.");
    int falseJump = emitJump(OP_JUMP_IF_NOT_TRUE);
    emitByte(OP_POP);
    statement();
    whenEndingJumps[jumps] = emitJump(OP_JUMP);
    patchJump(falseJump);
    emitByte(OP_POP);
    jumps++;
  }

  if (match(TOKEN_ELSE)) {
    consume(TOKEN_DO, "Expect 'do' after else in consider-when statement.");
    statement();
  }
  
  for (int i = 0; i < jumps; i++) {
    patchJump(whenEndingJumps[i]);
  }
}

static void switchStatement() {
  expression();
  consume(TOKEN_DO, "Expect 'do' after switch input.");
  int caseEndingJumps[256];
  int jumps = 0;

  int jumpTableNum = addJumpTable(currentChunk(), vm);
  
  if (jumpTableNum > UINT8_MAX) {
    errorAtCurrent("Too many switch statements in function/script.");
  }
  
  Table *jumpTable = getJumpTable(currentChunk(), jumpTableNum);
  
  emitBytePair(OP_JUMP_TABLE_JUMP, (uint8_t) jumpTableNum);

  int switchStart = currentChunk()->count;
  
  while (match(TOKEN_CASE)) {
    consume(TOKEN_STRING, "Expect string for case conditional.");
    uint16_t stringNum = makeConstant(OBJECT_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2, vm)));
    int newConditional = tableSet(jumpTable, AS_STRING(currentChunk()->constants.values[stringNum]), NUMBER_VAL(currentChunk()->count - switchStart), vm);
    if (!newConditional) errorAtCurrent("Duplicate case conditional inside switch statement.");
    consume(TOKEN_DO, "Expect 'do' after case conditional.");
    emitByte(OP_POP);
    statement();
    caseEndingJumps[jumps] = emitJump(OP_JUMP);
    jumps++;
  }
  if (jumps == 0) errorAtCurrent("No 'case'-es inside switch statement!");

  if (match(TOKEN_DEFAULT)) {
    consume(TOKEN_DO, "Expect 'do' after default case.");
    uint16_t stringNum = makeConstant(OBJECT_VAL(copyString("___internal_switch_default", 26 , vm)));
    tableSet(jumpTable, AS_STRING(currentChunk()->constants.values[stringNum]), NUMBER_VAL(currentChunk()->count - switchStart), vm);
    emitByte(OP_POP);
    statement();
  } else {
    uint16_t stringNum = makeConstant(OBJECT_VAL(copyString("___internal_switch_default", 26 , vm)));
    tableSet(jumpTable, AS_STRING(currentChunk()->constants.values[stringNum]), NUMBER_VAL(currentChunk()->count - switchStart), vm);
    emitByte(OP_POP);
  }
  
  for (int i = 0; i < jumps; i++) {
    patchJump(caseEndingJumps[i]);
  }
}

static void statement() {
  if (match(TOKEN_LEFT_BRACE)) {
    beginScope();
    block();
    endScope();
  } else if (match(TOKEN_IF)) {
    ifStatement();
  } else if (match(TOKEN_WHILE)) {
    whileStatement();
  } else if (match(TOKEN_EACH)) {
    eachStatement();
  } else if (match(TOKEN_CONTINUE)){
    continueStatement();
  } else if (match(TOKEN_CONSIDER)) {
    considerStatement();
  } else if (match(TOKEN_SWITCH)) {
    switchStatement();
  } else {
    expressionStatement();
  }
}

static void varDeclaration() {
  uint16_t global = parseVariable("Expect variable name.");

  if (match(TOKEN_ASSIGN)) {
    expression();
  } else {
    emitByte(OP_NIL);
  }

  checkEndStatement();
  
  defineVariable(global);
}

static void declaration() {
  if (match(TOKEN_FUNCTION)) {
    functionDeclaration();
  } else if (match(TOKEN_VAR)) {
    varDeclaration();
  } else {
    statement();
  }

  if (parser.panicMode) synchronize();
}

static void array(int canAssign) {
  ObjArray *array = newArrayObject(vm);
  emitConstant(OBJECT_VAL(array)); /* First things first, put this mo' on the stack. */

  int arrayCount = 0;
  while (!check(TOKEN_RIGHT_SQUARE) && !check(TOKEN_EOF)) {
    expression(); /* Then, put the rest of them mo's on the stack. */
    match(TOKEN_COMMA); /* Commas can be used as seperators, or just whitespace*/
    if (arrayCount + 1 > UINT8_MAX) {
      errorAtCurrent("Arrays must be less than 256 items.");
    }
    arrayCount++;
  }

  consume(TOKEN_RIGHT_SQUARE, "Expect ']' at the end of array declaration.");
  emitBytePair(OP_COLLECT, (uint8_t) arrayCount); /* Then, collect all them mo's from the stack. */
}

static void hashTable(int canAssign) {
  ObjTable *table = newTableObject(vm);
  emitConstant(OBJECT_VAL(table));
  while (!check(TOKEN_RIGHT_SQUARE) && !check(TOKEN_EOF)) {
    consume(TOKEN_IDENTIFIER, "Expect identifier before ':' in table declaration.");
    uint16_t identifier = identifierConstant(&parser.previous);
    consume(TOKEN_COLON, "Expect ':' after identifier in table declaration.");
    expression();
    if (!check(TOKEN_RIGHT_SQUARE)) {
      consume(TOKEN_COMMA, "Expect ',' between entries in table declaration");
    }
    /* Stack looks like this rn: <table>, <expression return value> */
    if (identifier > UINT8_MAX) {
      emitByteLong(OP_TABLE_SET_16, identifier);
    } else { 
      emitBytePair(OP_TABLE_SET, (uint8_t) identifier);
    }
    /* Both of these instructions must remove the return value from the stack,
       but leave the table on the stack. */
  }

  consume(TOKEN_RIGHT_SQUARE, "Expected ']' after table declaration.");
}

static void accessArray(int canAssign) {
  /* We presume that the array is already on the stack. 
     Next, we need to put the index on the stack, then emit an GET/SET_ARRAY instruction. 
  */
  if (parser.prevNext.type == TOKEN_RIGHT_SQUARE) {
    errorAtCurrent("Tried to access an array while declaring it.");
  }
  expression();
  consume(TOKEN_RIGHT_SQUARE, "Expect ']' after array index");

  if (canAssign && match(TOKEN_ASSIGN)) {
    expression(); /* Put the value you want to put in the array on the stack. */
    emitByte(OP_SET_ARRAY);
    checkEndStatement();
  } else {
    emitByte(OP_GET_ARRAY);
  }
}

static void tableCalculatedAccess(int canAssign) {
  /* Very similar to array access. */
  if (parser.prevNext.type == TOKEN_RIGHT_SQUARE) {
    errorAtCurrent("Tried to access a table while declaring it.");
  }
  expression();
  consume(TOKEN_RIGHT_SQUARE, "Expect ']' after table access");

  if (canAssign && match(TOKEN_ASSIGN)) {
    expression();
    emitByte(OP_TABLE_CLC_SET);
    checkEndStatement();
  } else {
    emitByte(OP_TABLE_CLC_GET);
  }
}

static void nil(int canAssign) {
  emitByte(OP_NIL);
}

static void logic_lit(int canAssign) {
  switch (parser.previous.type) {
  case TOKEN_FALSE: emitByte(OP_FALSE); break;
  case TOKEN_UNKNOWN: emitByte(OP_UNKNOWN); break;
  case TOKEN_TRUE: emitByte(OP_TRUE); break;
  }
}

static void number(int canAssign) {
  double value = strtod(parser.previous.start, NULL);
  if (value == 1.0) {
    emitByte(OP_PUSH_1);
  } else {
    emitConstant(NUMBER_VAL(value));
  }
}

static void string(int canAssign) {
  emitConstant(OBJECT_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2, vm)));
}

static void namedVariable(Token name, int canAssign) {
  uint8_t getOp, setOp;
  int arg = resolveLocal(current, &name);
  if (arg != -1) {
    getOp = OP_GET_LOCAL;
    setOp = OP_SET_LOCAL;
  } else if ((arg = resolveUpvalue(current, &name)) != -1) {
    getOp = OP_GET_UPVALUE;
    setOp = OP_SET_UPVALUE;
  } else {
    arg = identifierConstant(&name);
    if (arg > UINT8_MAX) {
      getOp = OP_GET_GLOBAL_16;
      setOp = OP_SET_GLOBAL_16;
    } else {
      getOp = OP_GET_GLOBAL;
      setOp = OP_SET_GLOBAL;
    }
  }
  
  if (canAssign && match(TOKEN_ASSIGN)) {
    expression();
    if (arg > UINT8_MAX) {
      emitByteLong(setOp, (uint16_t) arg);
    } else {
      emitBytePair(setOp, (uint8_t) arg);
    }
  } else {
    if (arg > UINT8_MAX) {
      emitByteLong(getOp, (uint16_t) arg);
    } else {
      emitBytePair(getOp, (uint8_t) arg);
    }
  }
}

static void variable(int canAssign) {
  namedVariable(parser.previous, canAssign);
}

static void unary(int canAssign) {
  tokenType operatorType = parser.previous.type;

  parsePrecedence(PREC_UNARY);

  switch (operatorType) {
  case TOKEN_MINUS: emitByte(OP_NEGATE); break;
  case TOKEN_NOT: emitByte(OP_KP_NOT); break;
  default: return; /* Unreachable, ideally */
  }
}

static void binary(int canAssign) {
  tokenType operatorType = parser.previous.type;
  ParseRule *rule = getRule(operatorType); /* :) <- das me smiling cuz I'm drunk :D */
  parsePrecedence((Precedence) rule->precedence + 1);

  switch (operatorType) {
  case TOKEN_PLUS: emitByte(OP_ADD); break;
  case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
  case TOKEN_TIMES: emitByte(OP_MULTIPLY); break;
  case TOKEN_DIVIDE: emitByte(OP_DIVIDE); break;
  case TOKEN_MODULO: emitByte(OP_MODULO); break;
  case TOKEN_EXPONENTIAL: emitByte(OP_EXPONENTIAL); break;
  case TOKEN_COMPARE: emitByte(OP_COMPARE); break;
  case TOKEN_LESS_THAN: emitByte(OP_KP_LESS_THAN); break;
  case TOKEN_LT_EQUAL: emitByte(OP_KP_LT_EQUAL); break;
  case TOKEN_GREAT_THAN: emitByte(OP_KP_GREAT_THAN); break;
  case TOKEN_GT_EQUAL: emitByte(OP_KP_GT_EQUAL); break;  
  case TOKEN_EQUAL: emitByte(OP_KP_EQUAL); break;
  case TOKEN_NOT_EQUAL: emitByte(OP_KP_NOT_EQUAL); break;
  case TOKEN_AND: emitByte(OP_KP_AND); break;
  case TOKEN_OR: emitByte(OP_KP_OR); break;
  case TOKEN_XOR: emitByte(OP_KP_XOR); break;
  default: return; /* Unreachable, hopefully */
  }
}

static void call(int canAssign) {
  uint8_t argCount = argumentList();
  emitBytePair(OP_CALL, argCount);
}

static void parsePrecedence(Precedence precedence) {
  advance();
  ParseFn prefixRule = getRule(parser.previous.type)->prefix;
  if (prefixRule == NULL) {
    errorAtCurrent("Expect expression.");
    return;
  }

  int canAssign = precedence <= PREC_ASSIGNMENT;
  prefixRule(canAssign);

  while (precedence <= getRule(parser.current.type)->precedence) {
    advance();
    ParseFn infixRule = getRule(parser.previous.type)->infix;
    infixRule(canAssign);
  }

  if (canAssign && match(TOKEN_EQUAL)) {
    errorAtCurrent("Invalid assignment target.");
  }
}

static void grouping() {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ) after expression.");
}

static void advance() {
  parser.prevNext = parser.previous;
  parser.previous = parser.current;

  while (1) {
    parser.current = scanToken();
    if (parser.current.type != TOKEN_ERROR) return;

    errorAtCurrent(parser.current.start);
  }
}

static void consume(tokenType type, char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  errorAtCurrent(message);
}

static int check(tokenType type) {
  return parser.current.type == type;
}

static int checkNewLine() { /* Returns true if the next token is on a different line to the previous one. */
  return parser.previous.line != parser.current.line || parser.current.type == TOKEN_EOF;
}

static int match(tokenType type) {
  if (!check(type)) return 0;
  advance();
  return 1;
}

ObjFunction *compile(char *source, char *filename, VM *veem) {
  /* steps: scan code for tokens, parse tokens for syntax tree, 
     compile syntax tree to bytecode, pipe bytecode to VM and run program. wahoo */

  //  scan(source); /* This prints out the raw tokens our scanner produces from the source code. */

  vm = veem;
  initScanner(source);
  Compiler compiler;
  initCompiler(&compiler, TYPE_SCRIPT);
  initParser();
  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }

  emitReturn();
  ObjFunction *function = endCompiler();
  
  return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
  Compiler *compiler = current;
  while (compiler != NULL) {
    markObject((Object *)compiler->function, vm);
    compiler = compiler->enclosing;
  }
}
