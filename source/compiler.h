#ifndef JOINT_COMPILER
#define JOINT_COMPILER

#include "chunk.h"
#include "value.h"
#include "vm.h"

ObjFunction *compile(char *source, char *filename, VM *vm);
void markCompilerRoots();
#endif
