#ifndef JOINT_OBJECT
#define JOINT_OBJECT

#include <stdint.h>

typedef struct libFn libFn;
typedef struct ObjClosure ObjClosure;
typedef struct ObjUpvalue ObjUpvalue;

#include "library.h"
#include "common.h"
#include "chunk.h"
#include "value.h"
#include "table.h"



#define OBJ_TYPE(value) (AS_OBJECT(value)->type)

typedef enum {
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_FUNCTION,
  OBJ_CLOSURE,
  OBJ_UPVALUE,
  OBJ_ARRAY,
  OBJ_TABLE,
} ObjType;


struct Object {
  ObjType type;
  int isMarked;
  Object *next;
};

struct ObjFunction {
  Object obj;
  int arity;
  int upvalueCount;
  Chunk chunk;
  ObjString *name;
};

typedef Value (*NativeFn)(int argCount, Value *args, VM *vm);

typedef struct {
  Object obj;
  libFn function;
} ObjNative;

struct ObjArray {
  Object obj;
  ValueArray values;
};

struct ObjTable {
  Object obj;
  Table table;
};

typedef struct {
  Object obj;
  Entry *entry;
} ObjEntry; /* This is an object type that is only intended for internal use. 
	       In particular, with 'in each' loops that go through hash tables.*/

struct ObjString {
  Object obj;
  int length;
  uint32_t hash;
  char *chars;
};

typedef struct ObjUpvalue ObjUpvalue;

struct ObjUpvalue {
  Object obj;
  Value *location;
  Value closed;
  ObjUpvalue *next;
};

struct ObjClosure {
  Object obj;
  ObjFunction *function;
  ObjUpvalue **upvalues;
  int upvalueCount;
};

static inline int isObjType(Value value, ObjType type) {
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#define IS_FUNCTION(value) isObjType(value, OBJ_FUNCTION)
#define AS_FUNCTION(value) ((ObjFunction *)AS_OBJECT(value))

#define IS_NATIVE(value) isObjType(value, OBJ_NATIVE)
#define AS_NATIVE(value) (((ObjNative *)AS_OBJECT(value))->function)

#define IS_CLOSURE(value) isObjType(value, OBJ_CLOSURE)
#define AS_CLOSURE(value) ((ObjClosure *)AS_OBJECT(value))

#define IS_ARRAY(value) isObjType(value, OBJ_ARRAY)
#define AS_ARRAY(value) ((ObjArray *)AS_OBJECT(value))

#define IS_TABLE(value) isObjType(value, OBJ_TABLE)
#define AS_TABLE(value) ((ObjTable *)AS_OBJECT(value))

#define IS_STRING(value) isObjType(value, OBJ_STRING)
#define AS_STRING(value) ((ObjString *)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjString *)AS_OBJECT(value))->chars)

ObjFunction *newFunction(VM *vm);
ObjClosure *newClosure(ObjFunction *function, VM *vm);
ObjUpvalue *newUpvalue(Value *slot, VM *vm);
ObjNative *newNative(libFn function, VM *vm);
ObjArray *newArrayObject(VM *vm);
ObjTable *newTableObject(VM *vm);
void setInArrayObject(ObjArray *array, Value index, Value value, VM *vm);
Value getFromArrayObject(ObjArray *array, Value index);
void setInTableObject(ObjTable *table, ObjString *key, Value value, VM *vm);
Value getFromTableObject(ObjTable *table, ObjString *key);
ObjString *takeString(char *chars, int length, VM *vm);
ObjString *copyString(char *chars, int length, VM *vm);
void printObject(Value object);
#endif
