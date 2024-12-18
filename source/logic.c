#include <stdio.h>
#include <math.h>

#include "value.h"
#include "object.h"
#include "logic.h"

TriloxLogic valuesEqual(Value a, Value b) {
  if (a.type != b.type) return TRILOX_UNKNOWN; /* Different types are incomparable */

  switch (a.type) {
  case VAL_NIL: return TRILOX_UNKNOWN; /* Nil is unknown in all comparisons, including with itself. Does this make sense? Maybe, I'm not sure. */
  case VAL_LOGIC: return (LOGIC_TO_TRILOX(AS_LOGIC(a) == AS_LOGIC(b)));
  case VAL_NUMBER: return LOGIC_TO_TRILOX(AS_NUMBER(a) == AS_NUMBER(b));
  case VAL_OBJECT: {
    if (AS_OBJECT(a)->type != AS_OBJECT(b)->type) return TRILOX_UNKNOWN;
    return LOGIC_TO_TRILOX(AS_OBJECT(a) == AS_OBJECT(b));
  }
  }
}

TriloxLogic valueNot(Value a) {
  if (a.type != VAL_LOGIC) return TRILOX_UNKNOWN; /* How are you gonna negate a non-logical value? */
  return AS_LOGIC(a) ? (AS_LOGIC(a) - 1 ? TRILOX_FALSE : TRILOX_UNKNOWN) : TRILOX_TRUE;
}

TriloxLogic valuesNotEqual(Value a, Value b) {
  return valueNot(LOGIC_VAL(valuesEqual(a,b)));
}

TriloxLogic ternaryCompare(Value a, Value b) {
  if (a.type != b.type) return TRILOX_UNKNOWN; /* Different types are incomparable */

  switch (a.type) {
  case VAL_NIL: return TRILOX_UNKNOWN;
  case VAL_LOGIC: return AS_LOGIC(a) - AS_LOGIC(b) > 0 ? TRILOX_TRUE : (AS_LOGIC(a) - AS_LOGIC(b) < 0 ? TRILOX_FALSE : TRILOX_UNKNOWN);
  case VAL_NUMBER: return AS_NUMBER(a) - AS_NUMBER(b) > 0 ? TRILOX_TRUE : (AS_NUMBER(a) - AS_NUMBER(b) < 0 ? TRILOX_FALSE : TRILOX_UNKNOWN);
  case VAL_OBJECT: {
    if (OBJ_TYPE(a) != OBJ_TYPE(b)) return TRILOX_UNKNOWN;
    switch (OBJ_TYPE(a)) {
    case OBJ_STRING: return AS_STRING(a)->length > AS_STRING(b)->length ? TRILOX_TRUE : (AS_STRING(a)->length < AS_STRING(b)->length ? TRILOX_FALSE : TRILOX_UNKNOWN);
    case OBJ_ARRAY: return AS_ARRAY(a)->values.count > AS_ARRAY(b)->values.count ? TRILOX_TRUE : (AS_ARRAY(a)->values.count < AS_ARRAY(b)->values.count ? TRILOX_FALSE : TRILOX_UNKNOWN);
    case OBJ_TABLE: return AS_TABLE(a)->table.count > AS_TABLE(b)->table.count ? TRILOX_TRUE : (AS_TABLE(a)->table.count < AS_TABLE(b)->table.count ? TRILOX_FALSE : TRILOX_UNKNOWN);
    default: return TRILOX_UNKNOWN;
    }
  }
  }
}

TriloxLogic valuesLessThan(Value a, Value b) {
  if (a.type != b.type) return TRILOX_UNKNOWN; /* Different types are incomparable */

  switch (a.type) {
  case VAL_NIL: return TRILOX_UNKNOWN;
  case VAL_LOGIC: return (LOGIC_TO_TRILOX(AS_LOGIC(a) < AS_LOGIC(b)));
  case VAL_NUMBER: return LOGIC_TO_TRILOX(AS_NUMBER(a) < AS_NUMBER(b));
  case VAL_OBJECT: {
    if (AS_OBJECT(a)->type != AS_OBJECT(b)->type) return TRILOX_UNKNOWN;
    return TRILOX_FALSE; /* Relative comparisons don't make any sense for non-numericals */
  }
  }
}

TriloxLogic valuesLToEqual(Value a, Value b) {
  return valuesEqual(a, b) == TRILOX_TRUE ? TRILOX_TRUE : valuesLessThan(a, b);
}

TriloxLogic valuesGreaterThan(Value a, Value b) {
  if (a.type != b.type) return TRILOX_UNKNOWN; /* Different types are incomparable */

  switch (a.type) {
  case VAL_NIL: return TRILOX_UNKNOWN;
  case VAL_LOGIC: return (LOGIC_TO_TRILOX(AS_LOGIC(a) > AS_LOGIC(b)));
  case VAL_NUMBER: return LOGIC_TO_TRILOX(AS_NUMBER(a) > AS_NUMBER(b));
  case VAL_OBJECT: {
    if (AS_OBJECT(a)->type != AS_OBJECT(b)->type) return TRILOX_UNKNOWN;
    return TRILOX_FALSE; /* Relative comparisons don't make any sense for non-numericals */
  }
  }
}

TriloxLogic valuesGToEqual(Value a, Value b) {
  return valuesEqual(a, b) == TRILOX_TRUE ? TRILOX_TRUE : valuesGreaterThan(a, b);
}

TriloxLogic valuesAnd(Value a, Value b) {
  if (a.type != VAL_LOGIC || a.type != b.type) return TRILOX_UNKNOWN;

  return AS_LOGIC(a) > AS_LOGIC(b) ? AS_LOGIC(b) : AS_LOGIC(a);
}

TriloxLogic valuesOr(Value a, Value b) {
  if (a.type != VAL_LOGIC || a.type != b.type) return TRILOX_UNKNOWN;

  return AS_LOGIC(a) < AS_LOGIC(b) ? AS_LOGIC(b) : AS_LOGIC(a);
}

TriloxLogic valuesXor(Value a, Value b) {
  if (a.type != VAL_LOGIC || a.type != b.type) return TRILOX_UNKNOWN;
  
  if (AS_LOGIC(a) == TRILOX_UNKNOWN || AS_LOGIC(b) == TRILOX_UNKNOWN) return TRILOX_UNKNOWN;

  return (AS_LOGIC(a) == AS_LOGIC(b)) ? TRILOX_FALSE : TRILOX_TRUE;
}
