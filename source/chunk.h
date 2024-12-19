#ifndef JOINT_BYTECODE
#define JOINT_BYTECODE

typedef struct VM VM;

#include <stddef.h>
#include <stdint.h>

#include "value.h"
#include "table.h"

typedef enum {
  OP_NIL,
  OP_CONSTANT,
  OP_CONSTANT_16,
  OP_PUSH_1,
  OP_COLLECT,
  OP_TABLE_SET,
  OP_TABLE_SET_16,
  OP_POP,
  OP_FALSE,
  OP_UNKNOWN,
  OP_TRUE,
  OP_NEGATE,
  OP_KP_NOT,
  OP_KP_AND,
  OP_KP_OR,
  OP_KP_XOR,
  OP_COMPARE,
  OP_KP_LESS_THAN,
  OP_KP_LT_EQUAL,
  OP_KP_GREAT_THAN,
  OP_KP_GT_EQUAL,
  OP_KP_EQUAL,
  OP_KP_NOT_EQUAL,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_MODULO,
  OP_EXPONENTIAL,
  OP_DEFINE_GLOBAL,
  OP_DEFINE_GLOBAL_16,
  OP_SET_GLOBAL,
  OP_GET_GLOBAL,
  OP_SET_GLOBAL_16,
  OP_GET_GLOBAL_16,
  OP_SET_LOCAL,
  OP_GET_LOCAL,
  OP_SET_UPVALUE,
  OP_GET_UPVALUE,
  OP_CLOSE_UPVALUE,
  OP_SET_ARRAY,
  OP_GET_ARRAY,
  OP_GET_ARRAY_LOOP,
  OP_GET_ARRAY_COUNT,
  OP_TABLE_CLC_SET,
  OP_TABLE_CLC_GET,
  OP_JUMP,
  OP_JUMP_IF_FALSE,
  OP_JUMP_IF_UNKNOWN,
  OP_JUMP_IF_TRUE,
  OP_JUMP_IF_NOT_TRUE,
  OP_JUMP_TABLE_JUMP,
  OP_LOOP,
  OP_CALL,
  OP_CLOSURE,
  OP_CLOSURE_16,
  OP_RETURN
} OpCode;

typedef struct {
  int count;
  int capacity;
  Table *tables;
} TableArray;

typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  ValueArray constants;
  int *lines;
  TableArray jumpTables;
} Chunk;

void initChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int line, VM *vm);
void freeChunk(Chunk *chunk, VM *vm);
void disassembleChunk(Chunk *chunk, char *name);

int addJumpTable(Chunk *chunk, VM *vm);
Table *getJumpTable(Chunk *chunk, uint8_t number);

int addConstant(Chunk *chunk, Value value, VM *vm);

#endif
