#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include "common.h"
#include "object.h"
#include "chunk.h"
#include "memory.h"
#include "vm.h"

uint8_t getFromChunk(Chunk *chunk, int slot) {
  if (slot >= chunk->count) {
    fprintf(stderr, "Out of bounds read of chunk");
    exit(1);
  }
  return chunk->code[slot];
}

uint16_t getLongFromChunk(Chunk *chunk, int slot) {
  if (slot >= chunk->count) {
    fprintf(stderr, "Out of bounds read of chunk");
    exit(1);
  }
  int high = (int) chunk->code[slot];
  int low = (int) chunk->code[slot + 1];
  return (uint16_t) ((high << 8) | low);
}

static int simpleInstruction(char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int constantInstruction(char *name, Chunk *chunk, int offset) {
  uint8_t constant = getFromChunk(chunk, offset + 1);
  printf("%-16s %4d '", name, constant);
  printValue(getFromValueArray(&chunk->constants, (int) constant));
  printf("'\n");
  return offset + 2;
}

static int longConstantInstruction(char *name, Chunk *chunk, int offset) {
  uint16_t constant = getLongFromChunk(chunk, offset + 1);
  printf("%-16s %4d '", name, constant);
  printValue(getFromValueArray(&chunk->constants, (int) constant));
  printf("\n");
  return offset + 3;
}

static int byteInstruction(char *name, Chunk *chunk, int offset) {
  uint8_t slot = getFromChunk(chunk, offset + 1);
  printf("%-16s %4d\n", name, slot);
  return offset + 2; 
}

static int jumpInstruction(char *name, int sign, Chunk *chunk, int offset) {
  uint16_t jump = (uint16_t) (chunk->code[offset+1] << 8);
  jump |= chunk->code[offset+2];
  printf("%-16s %4d -> %d\n", name, offset, offset + 3 + sign*jump);
  return offset + 3;
}

void initChunk(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
}

void writeChunk(Chunk *chunk, uint8_t byte, int line, VM *vm) {  
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity, vm);
    chunk->lines = GROW_ARRAY(int, chunk->lines, oldCapacity, chunk->capacity, vm);
  }

  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

void freeChunk(Chunk *chunk, VM *vm) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity, vm);
  FREE_ARRAY(int, chunk->lines, chunk->capacity, vm);
  freeValueArray(&chunk->constants, vm);
  initChunk(chunk); 
}

int addConstant(Chunk *chunk, Value value, VM *vm) {
  push(getStack(vm), value);
  writeValueArray(&chunk->constants, value, vm);
  pop(getStack(vm));
  return chunk->constants.count - 1;  
}

int disassembleInstruction(Chunk *chunk, int offset) {
  printf("%04d ", offset);

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_NIL: return simpleInstruction("OP_NIL", offset);
  case OP_RETURN: return simpleInstruction("OP_RETURN", offset);
  case OP_CONSTANT: return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_CONSTANT_16: return longConstantInstruction("OP_CONSTANT_16", chunk, offset);
  case OP_PUSH_1: return simpleInstruction("OP_PUSH_1", offset);
  case OP_DEFINE_GLOBAL: return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
  case OP_DEFINE_GLOBAL_16: return longConstantInstruction("OP_DEFINE_GLOBAL_16", chunk, offset);
  case OP_COLLECT: return byteInstruction("OP_COLLECT", chunk, offset);
  case OP_TABLE_SET: return constantInstruction("OP_TABLE_SET", chunk, offset);
  case OP_TABLE_SET_16: return longConstantInstruction("OP_TABLE_SET_16", chunk, offset);
  case OP_SET_GLOBAL: return constantInstruction("OP_SET_GLOBAL", chunk, offset);
  case OP_SET_GLOBAL_16: return longConstantInstruction("OP_SET_GLOBAL_16", chunk, offset);
  case OP_SET_LOCAL: return byteInstruction("OP_SET_LOCAL", chunk, offset);
  case OP_SET_UPVALUE: return byteInstruction("OP_SET_UPVALUE", chunk, offset);
  case OP_GET_GLOBAL: return constantInstruction("OP_GET_GLOBAL", chunk, offset);
  case OP_GET_GLOBAL_16: return longConstantInstruction("OP_GET_GLOBAL_16", chunk, offset);
  case OP_GET_LOCAL: return byteInstruction("OP_GET_LOCAL", chunk, offset);
  case OP_GET_UPVALUE: return byteInstruction("OP_GET_UPVALUE", chunk, offset);
  case OP_CLOSE_UPVALUE: return simpleInstruction("OP_CLOSE_UPVALUE", offset);
  case OP_SET_ARRAY: return simpleInstruction("OP_SET_ARRAY", offset);
  case OP_GET_ARRAY: return simpleInstruction("OP_GET_ARRAY", offset);
  case OP_GET_ARRAY_LOOP: return simpleInstruction("OP_GET_ARRAY_LOOP", offset);
  case OP_GET_ARRAY_COUNT: return simpleInstruction("OP_GET_ARRAY_COUNT", offset);
  case OP_TABLE_CLC_SET: return simpleInstruction("OP_SET_TABLE", offset);
  case OP_TABLE_CLC_GET: return simpleInstruction("OP_GET_TABLE", offset);
  case OP_POP: return simpleInstruction("OP_POP", offset);
  case OP_FALSE: return simpleInstruction("OP_FALSE", offset);
  case OP_UNKNOWN: return simpleInstruction("OP_UNKNOWN", offset);
  case OP_TRUE: return simpleInstruction("OP_TRUE", offset);
  case OP_NEGATE: return simpleInstruction("OP_NEGATE", offset);
  case OP_KP_NOT: return simpleInstruction("OP_KP_NOT", offset);
  case OP_COMPARE: return simpleInstruction("OP_COMPARE", offset);
  case OP_KP_LESS_THAN: return simpleInstruction("OP_KP_LESS_THAN", offset);
  case OP_KP_LT_EQUAL: return simpleInstruction("OP_KP_LT_EQUAL", offset);
  case OP_KP_GREAT_THAN: return simpleInstruction("OP_KP_GREAT_THAN", offset);
  case OP_KP_GT_EQUAL: return simpleInstruction("OP_KP_GT_EQUAL", offset);
  case OP_KP_EQUAL: return simpleInstruction("OP_KP_EQUAL", offset);
  case OP_KP_NOT_EQUAL: return simpleInstruction("OP_KP_NOT_EQUAL", offset);
  case OP_ADD: return simpleInstruction("OP_ADD", offset);
  case OP_SUBTRACT: return simpleInstruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY: return simpleInstruction("OP_MULTIPLY", offset);
  case OP_DIVIDE: return simpleInstruction("OP_DIVIDE", offset);
  case OP_MODULO: return simpleInstruction("OP_MODULO", offset);
  case OP_EXPONENTIAL: return simpleInstruction("OP_EXPONENTIAL", offset);
  case OP_JUMP: return jumpInstruction("OP_JUMP", 1, chunk, offset);
  case OP_JUMP_IF_FALSE: return jumpInstruction("OP_JUMP_IF_FALSE", 1, chunk, offset);
  case OP_JUMP_IF_UNKNOWN: return jumpInstruction("OP_JUMP_IF_UNKNOWN", 1, chunk, offset);
  case OP_JUMP_IF_TRUE: return jumpInstruction("OP_JUMP_IF_TRUE", 1, chunk, offset);
  case OP_LOOP: return jumpInstruction("OP_LOOP", -1, chunk, offset);
  case OP_CALL: return byteInstruction("OP_CALL", chunk, offset);
  case OP_CLOSURE: {
    offset++;
    uint8_t constant = chunk->code[offset++];
    printf("%-16s %4d ", "OP_CLOSURE", constant);
    printValue(chunk->constants.values[constant]);
    printf("\n");

    ObjFunction *function = AS_FUNCTION(chunk->constants.values[constant]);
    for (int j = 0; j < function->upvalueCount; j++) {
      int isLocal = chunk->code[offset++];
      int index = chunk->code[offset++];
      printf("%04d    |             %s %d\n", offset-2, isLocal ? "local" : "upvalue", index);
    }
    
    return offset;
  }
  case OP_CLOSURE_16: {
    offset++;
    uint8_t high = chunk->code[offset++];
    uint8_t low = chunk->code[offset++];
    uint16_t constant = (uint16_t) ((high << 8) | low);
    printf("%-16s %4d ", "OP_CLOSURE_16", constant);
    printValue(chunk->constants.values[constant]);
    printf("\n");
    
    ObjFunction *function = AS_FUNCTION(chunk->constants.values[constant]);
    for (int j = 0; j < function->upvalueCount; j++) {
      int isLocal = chunk->code[offset++];
      int index = chunk->code[offset++];
      printf("%04d    |             %s %d\n", offset-2, isLocal ? "local" : "upvalue", index);
    }
    
    return offset;
  
  }
  default:
    printf("Unknown OpCode: %d\n", instruction);
    return offset+1;
  }
}

void disassembleChunk(Chunk *chunk, char *name) {
  printf("<:: %s ::>\n", name);

  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

