#ifndef qw_vm_h
#define qw_vm_h

#include "qw_chunk.h"
#include "qw_object.h"
#include "qw_table.h"
#include "qw_values.h"

typedef struct {
  // Points to itself (access to constants)
  ObjectClosure* function;

  // Points to the instruction pointer
  u8* ip;

  // Points to the start of the stack function
  Value* slots;
} CallFrame;

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * 256)

typedef struct {
  u32 count;
  u32 capacity;
  Object** stack;
} Stack;

typedef struct {
  CallFrame frames[FRAMES_MAX];

  i32 frame_count;

  /// Stack of values
  Value stack[STACK_MAX];

  /// Instruction pointer points to the next instruction
  u8* ip;

  /// Chunk stores the instructions that the VM is running
  Chunk* chunk;

  /// Points to the memory address that is the top of the stack
  Value* stack_top;

  /// String interning, where the same strings share the same string address
  Table strings;

  ValueArray globals;

  /// Objects (as a Linked List) stores all the objects in the VM (so we can access them somehow when GC)
  Object* objects;

  /// LinkedList of open_upvalues that the VM is finding along the way, so when the
  /// user 'closes' with a closure some values, it would find them right here and
  /// store them in the heap
  ObjectUpvalue* open_upvalues;

  /// GARBAGE COLLECTOR: The current stack of object greys which we are processing
  Stack gray_stack_gc;

  /// GARBAGE COLLECTOR: Bytes allocated keeps track on the number of bytes allocated by the GC so we can make a good
  /// throughput of the system
  isize bytes_allocated;

  /// GARBAGE COLLECTOR: Next GC means what is the size needed for the next garbage collection
  isize next_gc;

  /// string for refering constructors
  ObjectString* init_string;
} VM;

typedef enum { INTERPRET_OK, INTERPRET_COMPILER_ERROR, INTERPRET_RUNTIME_ERROR } InterpretResult;

extern VM vm;

void init_vm(void);
void free_vm(void);
void push(Value value);
Value* stack_vm(void);
InterpretResult interpret_source(const char* source);

Value pop(void);

InterpretResult interpret(Chunk* chunk);

#endif
