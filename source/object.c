#include <stdio.h>
#include <string.h>
#include <math.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"
#include "table.h"

#define ALLOCATE_OBJECT(type, objectType, vm)		\
  (type *)allocateObject(sizeof(type), objectType, vm)

static Object *allocateObject(size_t size, ObjType objectType, VM *vm) {
  Object *object = (Object *)reallocate(NULL, 0, size, vm);

  object->next = vm->objects;
  vm->objects = object;
  object->type = objectType;
  object->isMarked = 0;
  
  if (DEBUG_LOG_GC) {
    char *typeTag = "pleh";
    switch(objectType) {
    case OBJ_NATIVE: typeTag = "ObjNative"; break;
    case OBJ_STRING: typeTag = "ObjString"; break;
    case OBJ_FUNCTION: typeTag = "ObjFunction"; break;
    case OBJ_CLOSURE: typeTag = "ObjClosure"; break;
    case OBJ_UPVALUE: typeTag = "ObjUpvalue"; break;
    case OBJ_ARRAY: typeTag = "ObjArray"; break;
    case OBJ_TABLE: typeTag = "ObjTable"; break;
    }
    printf("%p allocate %zu for %s\n", (void *)object, size, typeTag);
  }
  
  return object;
}

ObjArray *newArrayObject(VM *vm) {
  ObjArray *arrayObj = ALLOCATE_OBJECT(ObjArray, OBJ_ARRAY, vm);
  initValueArray(&arrayObj->values);
  return arrayObj;
}

ObjTable *newTableObject(VM *vm) {
  ObjTable *tableObj = ALLOCATE_OBJECT(ObjTable, OBJ_TABLE, vm);
  initTable(&tableObj->table);
  return tableObj;
}

Value getFromArrayObject(ObjArray *array, Value index) {
  /* if (!IS_NUMBER(index)) {
    printf("Tried to index into array with something that isn't a number. What?")
    return NIL_VAL;
    } */ /* Debating whether or not to do this check. OP_GET_ARRAY already 
	    checks for number indices and is better equipped to exit from errors. */
  double num_index = AS_NUMBER(index);
  int int_index = round(num_index);
  return getFromValueArray(&array->values, int_index - 1);
}

void setInArrayObject(ObjArray *array, Value index, Value value, VM *vm) {
  double num_index = AS_NUMBER(index);
  int int_index = round(num_index);
  if (int_index > array->values.count) {
    while (array->values.count < int_index - 1) {
      writeValueArray(&array->values, NIL_VAL, vm);
    }
    writeValueArray(&array->values, value, vm);
  } else {
    array->values.values[int_index - 1] = value;
  }
}

Value getFromTableObject(ObjTable *table, ObjString *key) {
  Value value;
  if (tableGet(&table->table, key, &value)) {
    return value;
  } else {
    value = NIL_VAL;
    return value;
  }
}

void setInTableObject(ObjTable *table, ObjString *key, Value value, VM *vm) {
  tableSet(&table->table, key, value, vm);
}

ObjFunction *newFunction(VM *vm) {
  ObjFunction *function = ALLOCATE_OBJECT(ObjFunction, OBJ_FUNCTION, vm);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjNative *newNative(libFn function, VM *vm) {
  ObjNative *native = ALLOCATE_OBJECT(ObjNative, OBJ_NATIVE, vm);
  native->function = function;
  return native;
}

ObjClosure *newClosure(ObjFunction *function, VM *vm) {
  ObjUpvalue **upvalues = ALLOCATE(ObjUpvalue *, function->upvalueCount, vm);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }
  ObjClosure *closure = ALLOCATE_OBJECT(ObjClosure, OBJ_CLOSURE, vm);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjUpvalue *newUpvalue(Value *slot, VM *vm) {
  ObjUpvalue *upvalue = ALLOCATE_OBJECT(ObjUpvalue, OBJ_UPVALUE, vm);
  upvalue->location = slot;
  upvalue->next = NULL;
  upvalue->closed = NIL_VAL;
  return upvalue;
}

static ObjString *allocateString(char *chars, int length, uint32_t hash, VM *vm) {
  ObjString *string = ALLOCATE_OBJECT(ObjString, OBJ_STRING, vm);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  push(getStack(vm), OBJECT_VAL(string));
  tableSet(&vm->strings, string, NIL_VAL, vm); /* Put the string in the string table */
  pop(getStack(vm));
  return string;
}

void printFunction(ObjFunction *function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }

  printf("<fn %s>", function->name->chars);
}

void printObject(Value object) {
  switch (OBJ_TYPE(object)) {
  case OBJ_STRING: printf("%s", AS_CSTRING(object)); break;
  case OBJ_FUNCTION: {
    printFunction(AS_FUNCTION(object)); break;
  }
  case OBJ_NATIVE: printf("<native fn>"); break;
  case OBJ_CLOSURE: printFunction(AS_CLOSURE(object)->function); break;
  case OBJ_UPVALUE: printf("upvalue"); break;
  case OBJ_ARRAY: {
    ValueArray *array = &AS_ARRAY(object)->values;
    printf("[ ");
    for (int i = 0; i < array->count - 1; i++) {
      printValue(array->values[i]);
      printf(", ");
    }
    if (array->count > 0) {
      printValue(array->values[array->count - 1]);
    }
    printf(" ]");
  } break;
  case OBJ_TABLE: printTable(&AS_TABLE(object)->table);
  }
}

static uint32_t hashString(char *chars, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t) chars[i];
    hash *= 16777619;
  }
  return hash;
}

ObjString *copyString(char *chars, int length, VM *vm) {
  uint32_t hash = hashString(chars, length);
  ObjString *interned = tableFindString(&vm->strings, chars, length, hash);
  if (interned != NULL) return interned;
  
  char *heapChars = ALLOCATE(char, length+1, vm);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash, vm);
  
}

ObjString *takeString(char *chars, int length, VM *vm) {
  uint32_t hash = hashString(chars, length);
  ObjString *interned = tableFindString(&vm->strings, chars, length, hash);

  if (interned != NULL) {
    FREE_ARRAY(char, chars, length+1, vm);
    return interned;
  }
  
  return allocateString(chars, length, hash, vm);
}
