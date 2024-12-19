#ifndef JOINT_TABLE
#define JOINT_TABLE

#include <stdint.h>

#include "config.h"
#include "value.h"
#include "chunk.h"

typedef struct {
  ObjString *key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry *entries;
} Table;
  
void initTable(Table *table);
void freeTable(Table *table, VM *vm);
int tableSet(Table *table, ObjString *key, Value value, VM *vm);
int tableGet(Table *table, ObjString *key, Value *value);
int tableDelete(Table *table, ObjString *key);
ObjString *tableFindString(Table *table, char *chars, int length, uint32_t hash);
void tableAddAll(Table *from, Table *to, VM *vm);
void tableRemoveWhite(Table *table);
void printTable(Table *table);

#endif
