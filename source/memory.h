#ifndef JOINT_MEMORY
#define JOINT_MEMORY
#include "object.h"
#include "table.h"
#include "vm.h"

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount, vm)			\
  (type *)reallocate(pointer, sizeof(type) * oldCount, sizeof(type) * newCount, vm)

#define FREE_ARRAY(type, pointer, oldCount, vm)			\
  (type *)reallocate(pointer, sizeof(type) * oldCount, 0, vm)

#define FREE(type, pointer, vm)			\
  reallocate(pointer, sizeof(type), 0, vm)

#define ALLOCATE(type, count, vm)				\
  (type *)reallocate(NULL, 0, sizeof(type) * (count), vm)

void *reallocate(void *pointer, size_t oldSize, size_t newSize, VM *vm);
void markObject(Object *object, VM *vm);
void markValue(Value value, VM *vm);
void markTable(Table *table, VM *vm);
void collectGarbage(VM *vm);
void freeObjects(Object *start, VM *vm);

#endif
