#ifndef JOINT_LIBRARY
#define JOINT_LIBRARY

#include "vm.h"

/* In order to create a functional native library for Trilox, you need to include, at
   minimum, the following header files:
   "config.h" - Provides certain debug flags which can affect library loading.
   "object.h" - Provides the NativeFn type and access to Trilox Objects
   "value.h"  - Provides access to Trilox Values
   "library.h"- Provides the libFn and libraryStruct types.

   You must define the following variable:
   int LibraryFunctionCount - This is what Joint uses to know how many functions to load.

   You must also define the following function
   int loadLibrary(libraryStruct *pointer) - This is the function that Joint looks for 
                                             when loading a library. 
   
   This function takes in a pointer to a libraryStruct, which is allocated by the 
   'loadNativeLibrary' function that Joint uses to load native libraries. Joint will 
   create a libraryStruct value and allocate enough memory for it to hold the number of
   functions specified by LibraryFunctionCount. Your loadLibrary function just needs to
   put your library functions, along with the name you intend users to access them by, 
   in the libraryStruct * that Joint provides, and return an exit code.
   An exit code of 0 is interpreted as no error, similar to standard Unix exit codes.

   Native Libraries are powerful because they provide direct access to C functions, which
   are just about always faster than anything that could be written directly in Trilox.
   However, they have a limited ability to interact with the rest of the Trilox system.
   Any state that is created by a native library will not be managed by the Trilox garbage
   collector, and cannot be accessed by a Trilox program except through the interface 
   provided by the library.
*/

#define LIBFN(name, type, func) ((libFn) {name, type, func})

typedef enum {
  RETURN_NUM,
  RETURN_NIL,
  RETURN_STRING,
} returnType;

typedef void *(*LibraryFn)(int argCount, Value *args);

struct libFn {
  char *name;
  returnType rettype;
  LibraryFn function;
};

#include "object.h"

typedef struct {
  int count; /* This isn't actually necessary. loadNativeLibrary never sets it or uses it. Fix? Prolly not worth it. */
  libFn library[];
} libraryStruct;

Value wrapLibraryFunc(libFn *libfn, int argCount, Value *args, VM *vm);
void loadNativeLibrary(char *filename, VM *vm);
void closeLibraries();

#endif
