#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include "value.h"
#include "object.h"
#include "library.h"

int LibraryFunctionCount = 4;

void *piNative(int argCount, Value *args) {
  double *pi = malloc(sizeof(double));
  *pi = 3.14159265358979323846;
  return (void *) pi;
}

void *clockNative(int argCount, Value *args) {
  double *time = malloc(sizeof(double));
  *time = (double) clock() / CLOCKS_PER_SEC;
  return (void *)time;
}

void *displayNative(int argCount, Value *args) {
  for (int i = 0; i < argCount - 1; i++) {
    printValue(args[i]);
    printf(", ");
  }
  if (argCount > 0) {
    printValue(args[argCount - 1]);
  }
  printf("\n");
  
  return NULL;
}

void *inputNative(int argCount, Value *args) {
  if (argCount > 0) {
    displayNative(argCount, args);
  }
  
  char *line = NULL;
  size_t linesize = 0;
  size_t size = getline(&line, &linesize, stdin);
  char *cleanline = malloc(size);
  strncpy(cleanline, line, size); /* Remove the trailing newline*/
  cleanline[size - 1] = '\0';
  free(line);
  
  return (void *) cleanline;
}

int loadLibrary(libraryStruct *pointer) {
  pointer->library[0] = LIBFN("disp", RETURN_NIL, displayNative);
  pointer->library[1] = LIBFN("pi", RETURN_NUM, piNative);
  pointer->library[2] = LIBFN("input", RETURN_STRING, inputNative);
  pointer->library[3] = LIBFN("clock", RETURN_NUM, clockNative);
  return 0;
}
