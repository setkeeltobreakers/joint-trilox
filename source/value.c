#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include "memory.h"
#include "value.h"

void printValue(Value value) {
  switch (value.type) {
  case VAL_NIL: printf("nil"); break;
  case VAL_LOGIC: switch (value.as.logic) {
    case TRILOX_FALSE: printf("false"); break;
    case TRILOX_UNKNOWN: printf("unknown"); break;
    case TRILOX_TRUE: printf("true"); break;
    } break;
  case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
  case VAL_OBJECT: printObject(value); break;
  }
}

void initValueArray(ValueArray *array) {
  array->count = 0;
  array->capacity = 0;
  array->values = NULL;
}

void writeValueArray(ValueArray *array, Value value, VM *vm) {
  if (array->capacity < array->count + 1) {
    int oldCapacity = array->capacity;
    array->capacity = GROW_CAPACITY(oldCapacity);
    array->values = GROW_ARRAY(Value, array->values, oldCapacity, array->capacity, vm);
  }
  
  int count = array->count;
  array->values[count] = value;
  array->count++;
}

Value getFromValueArray(ValueArray *array, int slot) {
  if (slot >= array->count) {
    fprintf(stderr, "Out of bounds read of value array.\n");
    exit(1);
  }

  return array->values[slot];
}

void freeValueArray(ValueArray *array, VM *vm) {
  FREE_ARRAY(Value, array->values, array->capacity, vm);
  initValueArray(array);
}

