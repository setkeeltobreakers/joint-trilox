#ifndef JOINT_VALUE
#define JOINT_VALUE

typedef struct Object Object;
typedef struct ObjString ObjString;
typedef struct ObjFunction ObjFunction;
typedef struct ObjArray ObjArray;
typedef struct ObjTable ObjTable;

typedef struct VM VM;

#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define IS_NIL(value) ((value).type == VAL_NIL)

#define NUMBER_VAL(value) ((Value) {VAL_NUMBER, {.number = value}})
#define AS_NUMBER(value) ((value).as.number)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

#define OBJECT_VAL(value) ((Value) {VAL_OBJECT, {.object = (Object *)value}}) /* takes in a pointer to the object, casts it to a generic object pointer. */
#define AS_OBJECT(value) ((value).as.object) /* Returns pointer */
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

#define LOGIC_VAL(value) ((Value) {VAL_LOGIC, {.logic = value}})
#define AS_LOGIC(value) ((value).as.logic)
#define IS_LOGIC(value) ((value).type == VAL_LOGIC)
#define LOGIC_TO_TRILOX(expression) ((expression) ? TRILOX_TRUE : TRILOX_FALSE) /* Converts logical true and false to Trilox true and false. Expects expression that resolves to Boolean */
#define LOGIC_TO_BOOL(value) ((value) == TRILOX_TRUE) /* Returns 1 if value == TRILOX_TRUE, false in all other cases. Maybe it'll be useful? */

typedef enum {
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJECT,
  VAL_LOGIC,
} valueType;

typedef enum {
  TRILOX_FALSE,
  TRILOX_UNKNOWN,
  TRILOX_TRUE
} TriloxLogic; /* Have to put it here bc of circular dependency issues. :( */

typedef struct {
  valueType type;
  union {
    double number;
    Object *object;
    TriloxLogic logic;
  } as;
} Value;

typedef struct {
  int count;
  int capacity;
  Value *values;
} ValueArray;

void printValue(Value value);
void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value, VM *vm);
void freeValueArray(ValueArray *array, VM *vm);
Value getFromValueArray(ValueArray *array, int slot);

#endif
