#ifndef JOINT_VM
#define JOINT_VM

#include "chunk.h"
#include "value.h"
#include "object.h"
#include "config.h"
#include "table.h"
#include "library.h"

typedef struct {
  ObjClosure *closure;
  uint8_t *ip;
  Value *slots;
} CallFrame;

typedef struct {
  Value *top;
  Value stack[VM_STACK_MAX_SIZE];
} VMStack;

typedef struct {
  int frameCount;
  CallFrame frames[FRAMES_MAX];
} CallStack;

struct VM {
  VMStack *main_stack;
  CallStack *call_stack;
  ObjUpvalue *openUpvalues;

  size_t bytesAllocated;
  size_t nextGC;

  Object *objects; /* Points to the head of the object linked list */
  Table strings;
  Table globals;

   int grayCount;
  int grayCapacity;
  Object **grayStack;
};

//extern VM vm;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

void defineNative(char *name, libFn function, VM *vm);
VMStack *getStack(VM *vm);
void dumpStacks(VM *vm);
void printStacks();

void initVM(VM *vm);
void freeVM(VM *vm);
InterpretResult interpret(char *source, char *filename, VM *vm);
void push(VMStack *stack, Value value);
Value pop(VMStack *stack);


#endif
