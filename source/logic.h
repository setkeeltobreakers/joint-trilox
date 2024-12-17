#ifndef JOINT_LOGIC
#define JOINT_LOGIC

#include "value.h"

extern TriloxLogic valuesEqual(Value a, Value b);
extern TriloxLogic ternaryCompare(Value a, Value b);
extern TriloxLogic valueNot(Value a);
extern TriloxLogic valuesNotEqual(Value a, Value b);
extern TriloxLogic valuesLessThan(Value a, Value b);
extern TriloxLogic valuesLToEqual(Value a, Value b);
extern TriloxLogic valuesGreaterThan(Value a, Value b);
extern TriloxLogic valuesGToEqual(Value a, Value b);
extern TriloxLogic valuesAnd(Value a, Value b);
extern TriloxLogic valuesOr(Value a, Value b);
extern TriloxLogic valuesXor(Value a, Value b);


#endif
