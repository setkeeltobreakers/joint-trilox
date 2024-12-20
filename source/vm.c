#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "vm.h"
#include "common.h"
#include "config.h"
#include "compiler.h"
#include "value.h"
#include "object.h"
#include "memory.h"
#include "logic.h"
#include "library.h"

VMStack *getStack(VM *vm) {
  return vm->main_stack;
}

static void dumpFrames(VM *vm) {
  for (int i = vm->call_stack->frameCount - 1; i >= 0; i--) {
    CallFrame *frame = &vm->call_stack->frames[i];
    ObjFunction *function = frame->closure->function;
    size_t instruction = frame->ip - function->chunk.code;
    fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }
}

static void runtimeError(char *message, VM *vm) {
  printf("%s\n", message);
  dumpFrames(vm);
}

void defineNative(char *name, libFn function, VM *vm) {
  push(vm->main_stack, OBJECT_VAL(copyString(name, (int)strlen(name), vm)));
  push(vm->main_stack, OBJECT_VAL(newNative(function, vm)));
  tableSet(&vm->globals, AS_STRING(vm->main_stack->stack[0]), vm->main_stack->stack[1], vm);
  pop(vm->main_stack);
  pop(vm->main_stack);
}

void resetStacks(VM *vm) {
  vm->main_stack->top = &vm->main_stack->stack[0];
  vm->call_stack->frameCount = 0;
  vm->openUpvalues = NULL;
}

void initVM(VM *vm) {
  vm->main_stack = malloc(sizeof(VMStack));
  if (vm->main_stack == NULL) {
    fprintf(stderr, "Ran out of memory initializing VM.\n");
    exit(1);
  }
  
  vm->call_stack = malloc(sizeof(CallStack));
  if (vm->call_stack == NULL) {
    fprintf(stderr, "Ran out of memory initializing VM.\n");
    exit(1);
  }
  resetStacks(vm);

  vm->objects = NULL;

  vm->bytesAllocated = 0;
  vm->nextGC = GC_DEFAULT_THRESHOLD;
  
  vm->grayCount = 0;
  vm->grayCapacity = 0;
  vm->grayStack = NULL;

  initTable(&vm->strings);
  initTable(&vm->globals);
  
  //printf("%p, %p, %p\n", vm.main_stack, vm.main_stack->top, vm.main_stack->stack);
  loadNativeLibrary("lib/native/corelib.binlib", vm);
}

void freeVM(VM *vm) {
  free(vm->main_stack);
  free(vm->call_stack);
  freeObjects(vm->objects, vm);
  free(vm->grayStack);
  freeTable(&vm->strings, vm);
  freeTable(&vm->globals, vm);
  closeLibraries();
}

void dumpStacks(VM *vm) {
  printf("Main VM Stack: \n [ ");
  VMStack *stack = vm->main_stack;
  while (stack->top - stack->stack > 0) {
    stack->top--;
    printValue(*stack->top);
    printf(", ");
  }
  printf("]\n");
}

void printStacks(VM *vm) {
  printf("Main VM Stack: \n [");
  for (Value *i = vm->main_stack->top - 1; i - vm->main_stack->stack > -1; i--) {
    printValue(*i);
    printf(", ");
  }
  printf("]\n");
}

static void dumpVM(VM *vm) {
  dumpStacks(vm);
  dumpFrames(vm);
}

Value peek(int offset, VMStack *stack) {
  return *(stack->top -1 - offset);
}

static int call(ObjClosure *closure, int argCount, VM *vm, VMStack *vmstack) {
  if (argCount != closure->function->arity) {
    runtimeError("Wrong number of arguments inputted to function", vm);
    return 0;
  }

  if (vm->call_stack->frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow", vm);
    return 0;
  }

  CallFrame *frame = &vm->call_stack->frames[vm->call_stack->frameCount++];
  frame->closure = closure;
  frame->ip = closure->function->chunk.code;
  frame->slots = vmstack->top - argCount - 1;
  return 1;
}

static int callValue(Value callee, int argCount, VM *vm, VMStack *vmstack) {
  if (IS_OBJECT(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_CLOSURE:
      return call(AS_CLOSURE(callee), argCount, vm, vmstack);
    case OBJ_NATIVE: {
      libFn libfn = AS_NATIVE(callee);
      Value result = wrapLibraryFunc(&libfn, argCount, vmstack->top - argCount, vm);
      vmstack->top -= argCount + 1;
      push(vmstack, result);
      return 1;
    }
    default: break;
    }
  }

  runtimeError("Tried to call non-function value", vm);
  return 0;
}

static ObjUpvalue *captureUpvalue(Value *local, VM *vm) {
  ObjUpvalue *prevUpvalue = NULL;
  ObjUpvalue *upvalue = vm->openUpvalues;

  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }
  
  ObjUpvalue *createdUpvalue = newUpvalue(local, vm);
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm->openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }
  
  return createdUpvalue;
}

static void closeUpvalues(Value *last, VM *vm) {
  while (vm->openUpvalues != NULL && vm->openUpvalues->location >= last) {
    ObjUpvalue *upvalue = vm->openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm->openUpvalues = upvalue->next;
  }
}

static void concatenate(VM *vm, VMStack *vmstack) {
  ObjString *b = AS_STRING(peek(0, vmstack));
  ObjString *a = AS_STRING(peek(1, vmstack));

  int length = a->length + b->length;
  char *chars = ALLOCATE(char, length + 1, vm);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString *result = takeString(chars, length, vm);
  pop(vmstack);
  pop(vmstack);
  push(vmstack, OBJECT_VAL(result));
}

static InterpretResult run(VM *vm) {
  CallFrame *frame = &vm->call_stack->frames[vm->call_stack->frameCount - 1];  
  VMStack *vmstack = getStack(vm);
  uint8_t *ip = frame->ip;
  uint8_t *codestart = frame->closure->function->chunk.code;
  int codelength = frame->closure->function->chunk.count;
  Value *constants = frame->closure->function->chunk.constants.values;
  
#define READ_BYTE() (*ip++)
#define READ_CONSTANT() (constants[READ_BYTE()])
#define READ_SHORT() (ip += 2, (uint16_t) ((ip[-2] << 8) | ip[-1]))
#define READ_LONG_CONSTANT() (constants[READ_SHORT()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define READ_LONG_STRING() AS_STRING(READ_LONG_CONSTANT())
#define BINARY_OP(op, stack) do {				     \
    if  (!IS_NUMBER(peek(0, stack)) || !IS_NUMBER(peek(1, stack))) { \
      runtimeError("Operands must be numbers!", vm);		     \
      return INTERPRET_RUNTIME_ERROR;			\
    }							\
    double b = AS_NUMBER(pop(stack));			\
    double a = AS_NUMBER(pop(stack));			\
    push(stack, NUMBER_VAL(a op b)); } while (0)
#define BIN_FUNCTION_OP(func, stack) do {		\
    if  (!IS_NUMBER(peek(0, stack)) || !IS_NUMBER(peek(1, stack))) { \
      runtimeError("Operands must be numbers!", vm);		     \
      return INTERPRET_RUNTIME_ERROR;			\
    }							\
    double b = AS_NUMBER(pop(stack));			\
    double a = AS_NUMBER(pop(stack));			\
    push(stack, NUMBER_VAL(func(a,b)));			\
  } while (0)

#define BIN_FUNCTION_LOGIC(func, stack) do {	\
    Value a = pop(stack);			\
    Value b = pop(stack);			\
    push(stack, LOGIC_VAL(func(b, a)));		\
  } while (0)
    
  while (1) {
    if (ip - codestart > codelength) {
      runtimeError("VM instruction pointer escaped the frame chunk! 99% chance this is an implimentation error, bug report time!", vm);
       return INTERPRET_RUNTIME_ERROR;
       }
    uint8_t instruction = READ_BYTE();
    switch (instruction) {
    case OP_NIL: push(vmstack, NIL_VAL); break;
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(vmstack, constant);
      break;
    }
    case OP_CONSTANT_16: {
      Value constant = READ_LONG_CONSTANT();
      push(vmstack, constant);
    } break; /* Use this to expand the constants table. When you get around to it. */
    case OP_PUSH_1: {
      push(vmstack, NUMBER_VAL(1));
    } break;
    case OP_COLLECT: {
      uint8_t arrayCount = READ_BYTE();
      //printStacks();
      if (!IS_ARRAY(peek(arrayCount, vmstack))) {
	runtimeError("Trying to collect into a non-array, this is an implimentation error, not yours!", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      ObjArray *array = AS_ARRAY(peek(arrayCount, vmstack));
      for (int i = 1; i <= arrayCount; i++) { /* Accessing from the top of the stack down would put values in the array in reverse order. 
						 Doing it this way (hopefully) puts them in the correct order. */
	writeValueArray(&array->values, peek(arrayCount - i, vmstack), vm);
      }
      //printStacks();
      for (int i = 0; i < arrayCount; i++) {
	pop(vmstack); /* Get them off the stack after writing everything to the array, bc GC reasons. */
      }
      //printStacks();
    } break;
    case OP_TABLE_SET: {
      if (!IS_TABLE(peek(1, vmstack))) {
	runtimeError("Trying to add an entry to a non-table. This is an implimentation error, get out your bug report!", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      setInTableObject(AS_TABLE(peek(1,vmstack)), READ_STRING(), peek(0, vmstack), vm);
      pop(vmstack);
    } break;
    case OP_TABLE_SET_16: {
      if (!IS_TABLE(peek(1, vmstack))) {
	runtimeError("Trying to add an entry to a non-table. This is an implimentation error, get out your bug report!", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      setInTableObject(AS_TABLE(peek(1,vmstack)), READ_LONG_STRING(), peek(0, vmstack), vm);
      pop(vmstack);
    } break;
    case OP_TABLE_GET: {
      if (!IS_TABLE(peek(0, vmstack))) {
	runtimeError("Trying to get an entry from a non-table. This is an implimentation error, get out your bug report!", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      Value value = getFromTableObject(AS_TABLE(peek(0,vmstack)), READ_STRING());
      pop(vmstack);
      push(vmstack, value);
    } break;
    case OP_TABLE_GET_16: {
      if (!IS_TABLE(peek(0, vmstack))) {
	runtimeError("Trying to get an entry from a non-table. This is an implimentation error, get out your bug report!", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      getFromTableObject(AS_TABLE(peek(0,vmstack)), READ_STRING());
    } break;
    case OP_POP: pop(vmstack); break;
    case OP_FALSE: push(vmstack, LOGIC_VAL(TRILOX_FALSE)); break;
    case OP_UNKNOWN: push(vmstack, LOGIC_VAL(TRILOX_UNKNOWN)); break;
    case OP_TRUE: push(vmstack, LOGIC_VAL(TRILOX_TRUE)); break;
    case OP_NEGATE:
      if (!IS_NUMBER(peek(0, vmstack))) {
	runtimeError("Operand must be a number!", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      push(vmstack, NUMBER_VAL(-AS_NUMBER(pop(vmstack))));
      break;
    case OP_KP_NOT: push(vmstack, LOGIC_VAL(valueNot(pop(vmstack)))); break;
    case OP_KP_AND: BIN_FUNCTION_LOGIC(valuesAnd, vmstack); break;
    case OP_KP_OR: BIN_FUNCTION_LOGIC(valuesOr, vmstack); break;
    case OP_KP_XOR: BIN_FUNCTION_LOGIC(valuesXor, vmstack); break;
    case OP_COMPARE: BIN_FUNCTION_LOGIC(ternaryCompare, vmstack); break;
    case OP_KP_LESS_THAN: BIN_FUNCTION_LOGIC(valuesLessThan, vmstack); break;
    case OP_KP_LT_EQUAL: BIN_FUNCTION_LOGIC(valuesLToEqual, vmstack); break;
    case OP_KP_GREAT_THAN: BIN_FUNCTION_LOGIC(valuesGreaterThan, vmstack); break;
    case OP_KP_GT_EQUAL: BIN_FUNCTION_LOGIC(valuesGToEqual, vmstack); break;
    case OP_KP_EQUAL: BIN_FUNCTION_LOGIC(valuesEqual, vmstack); break;
    case OP_KP_NOT_EQUAL: BIN_FUNCTION_LOGIC(valuesNotEqual, vmstack); break;
    case OP_ADD: {
      if (IS_STRING(peek(0, vmstack)) && IS_STRING(peek(1, vmstack))) {
	concatenate(vm, vmstack);
      } else if (IS_NUMBER(peek(0, vmstack)) && IS_NUMBER(peek(0, vmstack))) {
	double b = AS_NUMBER(pop(vmstack));
	double a = AS_NUMBER(pop(vmstack));			
	push(vmstack, NUMBER_VAL(a + b));
      } else {
	runtimeError("Operands must be two numbers or two strings.", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
    } break;
    case OP_SUBTRACT: BINARY_OP(-, vmstack); break;
    case OP_MULTIPLY: BINARY_OP(*, vmstack); break;
    case OP_DIVIDE: BINARY_OP(/, vmstack); break;
    case OP_MODULO: BIN_FUNCTION_OP(fmod, vmstack); break;
    case OP_EXPONENTIAL: BIN_FUNCTION_OP(pow, vmstack); break;
    case OP_DEFINE_GLOBAL: {
      ObjString *name = READ_STRING();
      tableSet(&vm->globals, name, peek(0, vmstack), vm);
      pop(vmstack);
      break;
    }
    case OP_DEFINE_GLOBAL_16: {
      ObjString *name = READ_LONG_STRING();
      tableSet(&vm->globals, name, peek(0, vmstack), vm);
      pop(vmstack);
    } break;
    case OP_SET_GLOBAL: {
      ObjString *name = READ_STRING();
      if (tableSet(&vm->globals, name, peek(0, vmstack), vm)) {
	tableDelete(&vm->globals, name);
	runtimeError("Tried to assign an undefined variable.", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_GET_GLOBAL: {
      ObjString *name = READ_STRING();
      Value value;
      if (!tableGet(&vm->globals, name, &value)) {
	runtimeError("Undefined variable.", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      push(vmstack, value);
      break;
    }
    case OP_SET_GLOBAL_16: {
      ObjString *name = READ_LONG_STRING();
      if (tableSet(&vm->globals, name, peek(0, vmstack), vm)) {
	tableDelete(&vm->globals, name);
	runtimeError("Tried to assign an undefined variable.", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
    } break;
    case OP_GET_GLOBAL_16: {
      ObjString *name = READ_LONG_STRING();
      Value value;
      if (!tableGet(&vm->globals, name, &value)) {
	runtimeError("Undefined variable.", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      push(vmstack, value);
    } break;
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek(0, vmstack);
    } break;
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(vmstack, frame->slots[slot]);
    } break;
    case OP_SET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      *frame->closure->upvalues[slot]->location = peek(0, vmstack);
    } break;
    case OP_GET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      push(vmstack, *frame->closure->upvalues[slot]->location);
    } break;
    case OP_CLOSE_UPVALUE: {
      closeUpvalues(vm->main_stack->top - 1, vm);
      pop(vmstack);
    } break;
    case OP_SET_ARRAY: {
      if (!IS_NUMBER(peek(1, vmstack))) {
	runtimeError("Expected number for array access.", vm);
	 return INTERPRET_RUNTIME_ERROR;
      }
      if (!IS_ARRAY(peek(2,vmstack))) {
	runtimeError("Trying to do an array access on something that isn't an array!", vm);
	return INTERPRET_RUNTIME_ERROR;
      } /* Check for these errors seperately to make the error messages more clear to the user. */
      if (AS_NUMBER(peek(1,vmstack)) < 1) {
	runtimeError("Invalid index for array.", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      setInArrayObject(AS_ARRAY(peek(2, vmstack)), peek(1, vmstack), peek(0, vmstack), vm);
      pop(vmstack);
      pop(vmstack);
    } break;
    case OP_GET_ARRAY: {
      if (!IS_NUMBER(peek(0, vmstack))) {
	runtimeError("Expected number for array access.", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      if (!IS_ARRAY(peek(1,vmstack))) {
	runtimeError("Trying to do an array access on something that isn't an array!", vm);
	return INTERPRET_RUNTIME_ERROR;
      } /* Check for these errors seperately to make the error messages more clear to the user. */
      Value result = getFromArrayObject(AS_ARRAY(peek(1, vmstack)), peek(0, vmstack));
      pop(vmstack);
      pop(vmstack);
      push(vmstack, result);
    } break;
    case OP_GET_ARRAY_LOOP: { /* This instruction does everything exactly the same as the 
				 regular instruction, but it leaves the array on the stack. 
				 Used in 'in each' loops. */
      if (!IS_NUMBER(peek(0, vmstack))) {
	runtimeError("Expected number for array access.", vm);
	return INTERPRET_RUNTIME_ERROR;
      }

      Value result;
      
      if (IS_ARRAY(peek(1,vmstack))) {
	result = getFromArrayObject(AS_ARRAY(peek(1, vmstack)), peek(0, vmstack));
      } else if (IS_TABLE(peek(1,vmstack))) {
	Value key;
	tableObjectGetN(AS_TABLE(peek(1, vmstack)), peek(0, vmstack), &result, &key);
      } else {
	runtimeError("Trying to do an each loop on something that isn't an array or a table!", vm);
	return INTERPRET_RUNTIME_ERROR;
      } /* Check for these errors seperately to make the error messages more clear to the user. */
      
      pop(vmstack);
      push(vmstack, result);
    } break;
    case OP_GET_TABLE_LOOP: { /* Like the previous instruction, but it puts both the value
				 and key on the stack. Also expects only tables*/
      if (!IS_NUMBER(peek(0, vmstack))) {
	runtimeError("Expected number for array access.", vm);
	return INTERPRET_RUNTIME_ERROR;
      }

      Value result;
      Value key;

      if (IS_TABLE(peek(1,vmstack))) {
	tableObjectGetN(AS_TABLE(peek(1, vmstack)), peek(0, vmstack), &result, &key);
      } else {
	runtimeError("Trying to do a table each loop on something that isn't a table!", vm);
	return INTERPRET_RUNTIME_ERROR;
      }

      pop(vmstack);
      push(vmstack, result);
      push(vmstack, key);
    } break;
    case OP_GET_ARRAY_COUNT: {

      Value count;
      
      if (IS_ARRAY(peek(0, vmstack))) {
	count = NUMBER_VAL(AS_ARRAY(peek(0, vmstack))->values.count);
      } else if (IS_TABLE(peek(0, vmstack))) {
	count = NUMBER_VAL(AS_TABLE(peek(0, vmstack))->table.count);
      } else {
	runtimeError("Trying to get the count of something that isn't an array!", vm);
	printValue(peek(0, vmstack));
	return INTERPRET_RUNTIME_ERROR;
      }
      push(vmstack, count);
    } break;
    case OP_TABLE_CLC_SET: {
      if (!IS_STRING(peek(1, vmstack))) {
	runtimeError("Expected string for table access.", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      if (!IS_TABLE(peek(2, vmstack))) {
	runtimeError("Trying to do a table access on something that isn't a table!", vm);
	return INTERPRET_RUNTIME_ERROR;
      }

      setInTableObject(AS_TABLE(peek(2, vmstack)), AS_STRING(peek(1, vmstack)), peek(0, vmstack), vm);
      pop(vmstack);
      pop(vmstack);
    } break;
    case OP_TABLE_CLC_GET: {
      if (!IS_STRING(peek(0, vmstack))) {
	runtimeError("Expected string for table access.", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      if (!IS_TABLE(peek(1, vmstack))) {
	runtimeError("Trying to do a table access on something that isn't a table!", vm);
	return INTERPRET_RUNTIME_ERROR;
      }
      Value result = getFromTableObject(AS_TABLE(peek(1, vmstack)), AS_STRING(peek(0, vmstack)));
      pop(vmstack);
      pop(vmstack);
      push(vmstack, result);
    } break;
    case OP_JUMP: {
      uint16_t offset = READ_SHORT();
      ip += offset;
    } break;
    case OP_JUMP_IF_FALSE: {
      uint16_t offset = READ_SHORT();
      if (valueNot(peek(0, vmstack)) == TRILOX_TRUE) ip += offset; // Checks for negation bc reasons.
    } break;
    case OP_JUMP_IF_UNKNOWN: {
      uint16_t offset = READ_SHORT();
      if (valueNot(peek(0, vmstack)) == TRILOX_UNKNOWN) ip += offset; // One of the reasons is that it won't mess up when you get a non logical value. 
    } break;
    case OP_JUMP_IF_TRUE: {
      uint16_t offset = READ_SHORT();
      if (valueNot(peek(0, vmstack)) == TRILOX_FALSE) ip += offset;
    } break;
    case OP_JUMP_IF_NOT_TRUE: {
      uint16_t offset = READ_SHORT();
      if (valueNot(peek(0, vmstack)) != TRILOX_FALSE) ip += offset;
    } break;
    case OP_JUMP_TABLE_JUMP: {
      uint8_t jumpTableNum = READ_BYTE();
      Table *jumpTable = getJumpTable(&frame->closure->function->chunk, jumpTableNum);
      Value offsetVal;
      int isCase = 0;
      if (IS_STRING(peek(0, vmstack))) {
      isCase = tableGet(jumpTable, AS_STRING(peek(0, vmstack)), &offsetVal);
      }
      if (!isCase) {
	tableGet(jumpTable, copyString("___internal_switch_default", 26, vm), &offsetVal);
      }
      ip += (int) AS_NUMBER(offsetVal);
    } break;
    case OP_LOOP: {
      uint16_t offset = READ_SHORT();
      ip -= offset;
    } break;
    case OP_CALL: {
      int argCount = READ_BYTE();
      if (!callValue(peek(argCount, vmstack), argCount, vm, vmstack)) {
	return INTERPRET_RUNTIME_ERROR;
      }
      frame->ip = ip;
      frame = &vm->call_stack->frames[vm->call_stack->frameCount - 1];
      ip = frame->ip;
      codestart = frame->closure->function->chunk.code;
      codelength = frame->closure->function->chunk.count;
      constants = frame->closure->function->chunk.constants.values;
    } break;
    case OP_CLOSURE: {
      ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
      ObjClosure *closure = newClosure(function, vm);
      push(vmstack, OBJECT_VAL(closure));

      for (int i = 0; i < closure->upvalueCount; i++) {
	uint8_t isLocal = READ_BYTE();
	uint8_t index = READ_BYTE();
	if (isLocal) {
	  closure->upvalues[i] = captureUpvalue(frame->slots + index, vm);
	} else {
	  closure->upvalues[i] = frame->closure->upvalues[index];
	}
      }
    } break;
    case OP_CLOSURE_16: {
      ObjFunction *function = AS_FUNCTION(READ_LONG_CONSTANT());
      ObjClosure *closure = newClosure(function, vm);
      push(vmstack, OBJECT_VAL(closure));

      for (int i = 0; i < closure->upvalueCount; i++) {
	uint8_t isLocal = READ_BYTE();
	uint8_t index = READ_BYTE();
	if (isLocal) {
	  closure->upvalues[i] = captureUpvalue(frame->slots + index, vm);
	} else {
	  closure->upvalues[i] = frame->closure->upvalues[index];
	}
      }
    } break;
    case OP_RETURN: {
      Value result = pop(vmstack);
      closeUpvalues(frame->slots, vm);
      vm->call_stack->frameCount--;
      
      if (vm->call_stack->frameCount == 0) {
	pop(vmstack);
	return INTERPRET_OK;
      }
      vm->main_stack->top = frame->slots;
      push(vmstack, result);
      frame = &vm->call_stack->frames[vm->call_stack->frameCount - 1];
      ip = frame->ip;
      codestart = frame->closure->function->chunk.code;
      codelength = frame->closure->function->chunk.count;
      constants = frame->closure->function->chunk.constants.values;
    } break;
    default: return INTERPRET_RUNTIME_ERROR;
    }
  }
#undef BIN_FUNCTION_OP
#undef BINARY_OP
#undef READ_LONG_STRING
#undef READ_STRING
#undef READ_LONG_CONSTANT
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_BYTE
}

InterpretResult interpret(char *source, char *filename, VM *vm) {
  ObjFunction *function = compile(source, filename, vm);
  
  if (function == NULL) {
    printf("Error in compilation\n");
    return INTERPRET_COMPILE_ERROR;
  }

  VMStack *vmstack = vm->main_stack;
  push(vmstack, OBJECT_VAL(function));
  ObjClosure *closure = newClosure(function, vm);
  pop(vmstack);
  push(vmstack, OBJECT_VAL(closure));
  call(closure, 0, vm, vmstack);

  return run(vm);
}

void push(VMStack *stack, Value value) {
  *stack->top = value;
  stack->top++;
}

Value pop(VMStack *stack) {
  if (stack->top == 0) {
    fprintf(stderr, "Stack underflow");
    exit(1);
  }
  stack->top--;
  return *stack->top;
}
