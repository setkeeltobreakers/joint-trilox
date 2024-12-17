#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>

#include "config.h"
#include "memory.h"
#include "object.h"
#include "compiler.h"
#include "table.h"
#include "value.h"
#include "vm.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize, VM *vm) {
  vm->bytesAllocated += newSize - oldSize;
  if (newSize > oldSize) {
    if (DEBUG_STRESS_GC) {
      collectGarbage(vm);
    }

    if (vm->bytesAllocated > vm->nextGC && !DEBUG_STRESS_GC) {
      collectGarbage(vm);
    }
  }

  if (newSize == 0) {
    free(pointer);
    return NULL;
  }
  
  void *result = realloc(pointer, newSize);

  if (result == NULL) {
    fprintf(stderr, "Ran out of memory growing array. RIP\n");
    exit(1);
  }

  return result;
}

static void freeObject(Object *object, VM *vm) {
  if (DEBUG_LOG_GC) {
    char *typeTag = "pleh";
    switch(object->type) {
    case OBJ_NATIVE: typeTag = "ObjNative"; break;
    case OBJ_STRING: typeTag = "ObjString"; break;
    case OBJ_FUNCTION: typeTag = "ObjFunction"; break;
    case OBJ_CLOSURE: typeTag = "ObjClosure"; break;
    case OBJ_UPVALUE: typeTag = "ObjUpvalue"; break;
    case OBJ_ARRAY: typeTag = "ObjArray"; break;
    case OBJ_TABLE: typeTag = "ObjTable"; break;
    }
    printf("%p free type %s\n", (void *)object, typeTag);
  }

  switch (object->type) {
  case OBJ_STRING: {
    ObjString *string = (ObjString *)object;
    FREE_ARRAY(char, string->chars, string->length + 1, vm);
    FREE(ObjString, string, vm);
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    freeChunk(&function->chunk, vm);
    FREE(ObjFunction, object, vm);
  } break;
  case OBJ_NATIVE: {
    FREE(ObjNative, object, vm);
  } break;
  case OBJ_CLOSURE: {
    ObjClosure *closure = (ObjClosure *)object;
    FREE_ARRAY(ObjUpvalue *, closure->upvalues, closure->upvalueCount, vm);
    FREE(ObjClosure, object, vm);
  } break;
  case OBJ_UPVALUE: {
    FREE(ObjUpvalue, object, vm);
  } break;
  case OBJ_ARRAY: {
    ObjArray *array = (ObjArray *)object;
    freeValueArray(&array->values, vm);
    FREE(ObjArray, object, vm);
  } break;
  case OBJ_TABLE: {
    ObjTable *table = (ObjTable *)object;
    freeTable(&table->table, vm);
    FREE(ObjTable, object, vm);
  } break;
  }
}

void freeObjects(Object *start, VM *vm) {
  Object *object = start;
  while (object != NULL){
    Object *next = object->next;
    freeObject(object, vm);
    object = next;
  }
}

void markObject(Object *object, VM *vm) {
  if (object == NULL) return;
  if (object->isMarked) return;
  if (DEBUG_LOG_GC) {
    printf("%p mark ", (void *)object);
    printValue(OBJECT_VAL(object));
    printf("\n");
  }
  object->isMarked = 1;

  if (vm->grayCapacity < vm->grayCount + 1) {
    vm->grayCapacity = GROW_CAPACITY(vm->grayCapacity);
    vm->grayStack = (Object **)realloc(vm->grayStack, sizeof(Object *) * vm->grayCapacity);
  }

  if (vm->grayStack == NULL) {
    fprintf(stderr, "Ran out of memory in the garbage collector? Now that's irony!\n");
    exit(1);
  }
  
  vm->grayStack[vm->grayCount++] = object;
}

void markValue(Value value, VM *vm) {
  if (IS_OBJECT(value)) markObject(AS_OBJECT(value), vm);
}

static void markArray(ValueArray *array, VM *vm) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i], vm);
  }
}

static void blackenObject(Object *object, VM *vm) {
  if (DEBUG_LOG_GC) {
    printf("%p blacken ", (void *)object);
    printValue(OBJECT_VAL(object));
    printf("\n");
  }
  
  switch (object->type) {
  case OBJ_NATIVE:
  case OBJ_STRING: break;
  case OBJ_FUNCTION: {
    ObjFunction *function = (ObjFunction *)object;
    markObject((Object *)function->name, vm);
    markArray(&function->chunk.constants, vm);
  } break;
  case OBJ_CLOSURE: {
    ObjClosure *closure = (ObjClosure *)object;
    markObject((Object *)closure->function, vm);
    for (int i = 0; i < closure->upvalueCount; i++) {
      markObject((Object *)closure->upvalues[i], vm);
    }
  } break;
  case OBJ_UPVALUE: {
    markValue(((ObjUpvalue *)object)->closed, vm);
  } break;
  case OBJ_ARRAY: {
    markArray(&((ObjArray *)object)->values, vm);
  } break;
  case OBJ_TABLE: {
    markTable(&((ObjTable *)object)->table, vm);
  } break;
  }
}

void markTable(Table *table, VM *vm) {
  for (int i = 0; i < table->capacity; i++) {
    Entry *entry = &table->entries[i];
    markObject((Object *)entry->key, vm);
    markValue(entry->value, vm);
  }
}

static void markRoots(VM *vm) {
  for (Value *slot = vm->main_stack->stack; slot < vm->main_stack->top; slot++) {
    markValue(*slot, vm);
  }
  for (int i = 0; i < vm->call_stack->frameCount; i++) {
    markObject((Object *)vm->call_stack->frames[i].closure, vm);
  }

  for (ObjUpvalue *upvalue = vm->openUpvalues; upvalue != NULL; upvalue = upvalue->next) {
    markObject((Object *)upvalue, vm);
  }
  
  markTable(&vm->globals, vm);
  markCompilerRoots();
}

static void traceReferences(VM *vm) {
  while (vm->grayCount > 0) {
    Object *object = vm->grayStack[--vm->grayCount];
    blackenObject(object, vm);
  }
}

static void sweep(VM *vm) {
  Object *previous = NULL;
  Object *object = vm->objects;
  while (object != NULL) {
    if (object->isMarked) {
      object->isMarked = 0;
      previous = object;
      object = object->next;
    } else {
      Object *unreached = object;
      object = object->next;
      if (previous != NULL) {
	previous->next = object;
      } else {
	vm->objects = object;
      }

      freeObject(unreached, vm);
    }
  }
}

void collectGarbage(VM *vm) {
  if (DEBUG_LOG_GC) {
    printf("-- gc begin\n");
  }
  size_t before = vm->bytesAllocated;
  
  markRoots(vm);
  traceReferences(vm);
  tableRemoveWhite(&vm->strings);
  sweep(vm);

  vm->nextGC = vm->bytesAllocated * GC_HEAP_GROWTH_FACTOR;
  
  if (DEBUG_LOG_GC) {
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu)\n", before - vm->bytesAllocated, before, vm->bytesAllocated);
  }
}
