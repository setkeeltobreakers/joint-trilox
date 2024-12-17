#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

#include "config.h"
#include "object.h"
#include "library.h"
#include "value.h"
#include "vm.h"

typedef int (*LibraryLoader)(libraryStruct *pointer); /* Return int so they can pass error codes. 
				   A return of 0 is interpreted as no error, 
				   similarly to standard Unix error codes.
				   TODO: define error codes in an enum. */


static void *libraries[64];
static int librariesCount = 0;

Value wrapLibraryFunc(libFn *libfn, int argCount, Value *args, VM *vm) {
  switch (libfn->rettype) {
  case RETURN_NIL: {
    libfn->function(argCount, args);
    return NIL_VAL;
  }
  case RETURN_NUM: {
    double *num = (double *) libfn->function(argCount, args);
    double number = *num;
    free(num);
    return NUMBER_VAL(number);
  }
  case RETURN_STRING: {
    char *str = (char *) libfn->function(argCount, args);
    ObjString *string = copyString(str, strlen(str), vm);
    free(str);
    return OBJECT_VAL(string);
  }
  }
}

void loadNativeLibrary(char *filename, VM *vm) {
  void *library = dlopen(filename, RTLD_NOW);
  
  if (library == NULL) {
    fprintf(stderr, "Error opening '%s' native library: %s\n", filename, dlerror());
    exit(1);
  }

  int *libCount = (int *) dlsym(library, "LibraryFunctionCount");
  LibraryLoader loader = (LibraryLoader) dlsym(library, "loadLibrary");

  if (loader == NULL || libCount == NULL) {
    fprintf(stderr, "Error in '%s' native library: %s\n", filename, dlerror());
    exit(1);
  }

  libraryStruct *libPointer = malloc(sizeof(libraryStruct) + sizeof(libFn) * *libCount);
  
  if (libPointer == NULL) {
    fprintf(stderr, "Ran out of memory loading array. RIP!\n");
    exit(1);
  }
  
  int result = loader(libPointer);
  
  if (!result) {
    for (int i = 0; i < *libCount; i++) {
      defineNative(libPointer->library[i].name, libPointer->library[i], vm);
    }
    if (DEBUG_PRINT_LIBRARY) {
      printf("Successfully loaded '%s' native library\n", filename);
    }
  } else if (result) {
    fprintf(stderr, "Error loading '%s' native library. Error code: %d", filename, result);
    exit(1);
  }

  free(libPointer);

  libraries[librariesCount] = library;
  librariesCount++;
}

void closeLibraries() {
  for (int i = 0; i < librariesCount; i++) {
    dlclose(libraries[i]);
  }
}
