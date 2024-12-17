#ifndef JOINT_CONFIG
#define JOINT_CONFIG

/* debug config stuff */
/* All of this stuff is going to be turned into command-line arguments for accessability. */

extern int DEBUG_PRINT_BYTECODE;
extern int DEBUG_STRESS_GC;
extern int DEBUG_LOG_GC;
extern int DEBUG_PRINT_LIBRARY;

/* internal stuff */
#define FRAMES_MAX 64
#define VM_STACK_MAX_SIZE (FRAMES_MAX * (UINT8_MAX + 1))

#define MAX_ARITY 255

#define MAX_FUNC_NESTING 256 // Arbitrary limit for function nesting. Not implimented currently.

#define MAX_LOOP_NESTING 64

#define TABLE_MAX_LOAD_FACTOR 0.75

#define GC_DEFAULT_THRESHOLD 1024 * 1024

#define GC_HEAP_GROWTH_FACTOR 2

#endif
