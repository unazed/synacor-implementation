#ifndef __VM_H
#define __VM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

#define __N_REGISTERS 			(8)
#define __N_STACK_SLOTS 		(32)
#define __STACK_REALLOC_INC (4)
#define VM_MAX_LOADSIZE 		(65536)
#define VM_MAX_LOADSIZE_S 	"65536"

#define DEFINE_OPCODE(name) void VM_opcode_##name(vm_state_t* self, __vm_word_t* operands)
#define OPCODE(name) VM_opcode_##name

#ifdef DEBUG
#	include <stdio.h>
#	define DBG(x, ...) do {																											\
		fprintf( stdout, "%s:%s:%d: " x "\n", "DBG", __func__,		\
				__LINE__, ##__VA_ARGS__);			\
		fflush (stdout);																													\
		fflush (stderr);																													\
		} while (0)
#else
#	define DBG(x, ...) do { } while(0)
#endif /* DEBUG */

#include "enum.h"

typedef uint16_t __vm_register_t;
typedef uint16_t __vm_word_t;

typedef struct
{
  __vm_word_t *head;

  size_t size;
  size_t capacity;

  void (*push) (__vm_word_t);
    __vm_word_t (*pop) (void);
  bool (*is_empty) (void);
} vm_stack_t;

struct vm_errstate
{
  const char *function;
  const char *reason;
};

struct vm_buffer
{
  const char *buffer;
  size_t size;
};

typedef struct
{
  uint16_t pc;

  __vm_register_t registers[__N_REGISTERS];
  vm_stack_t *stack;

  struct vm_errstate traceback;
  struct vm_buffer buffer;

  bool (*load) (char *from, size_t n);
  void (*free) (void);
  bool (*execute) (void);
} vm_state_t;

__attribute__((noreturn))
     static inline void VM_error (const char *msg)
{
  fprintf (stderr, "fatal error: %s\n", msg);
  exit (EXIT_FAILURE);
}

static struct
{
  vm_stack_t *stack;
  vm_state_t *state;
} __current_state;

static inline vm_state_t *
__VM_get_state (void)
{
  if (__current_state.state == NULL)
    VM_error ("__VM_get_state() failed, invalid state");
  return __current_state.state;
}

static inline vm_stack_t *
__VM_get_stack (void)
{
  if (__current_state.state == NULL)
    VM_error ("__VM_get_stack() failed, invalid state");
  if (__current_state.state->stack->head == NULL)
    VM_error ("__VM_get_stack() failed, stack state uninitialized");
  return __current_state.state->stack;
}

static inline void
VM_set_traceback (const char *function, const char *reason)
{
  vm_state_t *state = __VM_get_state ();
  DBG ("non-fatal error occurred, traceback has been set");
  state->traceback.function = function;
  state->traceback.reason = reason;
}

static inline bool
__VM_stack_reallocate (vm_stack_t * stack)
{
  if (realloc ((void *) ((uintptr_t) stack->head
			 - (uintptr_t) stack->size * sizeof (__vm_word_t)),
	       stack->capacity + __STACK_REALLOC_INC) == NULL)
    return false;
  DBG ("__VM_stack_reallocate() success");
  stack->capacity += __STACK_REALLOC_INC;
  return true;
}

static inline void
__VM_stack_deallocate (void)
{
  vm_stack_t *stack = __VM_get_stack ();
  if (stack->head == NULL)
    VM_error ("__VM_stack_deallocate() failed, stack was never allocated");
  free (stack->head);
  stack->capacity = 0;
  stack->size = 0;
  free (stack);
  DBG ("__VM_stack_deallocate() successful");
}

static bool
VM_load (char *from, size_t n)
{
  vm_state_t *self = __VM_get_state ();
  if (self->buffer.buffer != NULL)
    {
      VM_set_traceback ("VM_load", "cannot double-load buffers");
      return false;
    }
  if (n > VM_MAX_LOADSIZE)
    {
      VM_set_traceback ("VM_load", "cannot load buffers larger than "
			VM_MAX_LOADSIZE_S " bytes");
      return false;
    }
  self->buffer.buffer = from;
  self->buffer.size = n;
  return true;			/* always return true */
}

static void
VM_free (void)
{
  __VM_stack_deallocate ();
}

static __vm_word_t
VM_read_word (vm_state_t * state)
{
  __vm_word_t word = *(__vm_word_t *) (&state->buffer.buffer[state->pc]);
  state->pc += sizeof (__vm_word_t);
  return word;
}

static __vm_word_t
VM_read_word_at (vm_state_t * state, __vm_word_t idx)
{
  return *(__vm_word_t *) (&state->buffer.buffer[sizeof (__vm_word_t) * idx]);
}

static inline bool
VM_stack_isempty (void)
{
  vm_stack_t *self = __VM_get_stack ();
  DBG ("VM_stack_isempty() -> %d", !self->size);
  return !self->size;
}

static void
VM_stack_push (__vm_word_t val)
{
  vm_stack_t *self = __VM_get_stack ();
  if (self->size++ == self->capacity)
    if (!__VM_stack_reallocate (self))
      VM_error ("__VM_stack_reallocate() failed, insufficient memory");
  *self->head++ = val;
  DBG ("VM_stack_push(%d) successful", val);
}

static __vm_word_t
VM_stack_pop (void)
{
  vm_stack_t *self = __VM_get_stack ();
  if (!self->size--)
    VM_error ("VM_stack_pop() failed, tried to pop from empty stack");
  DBG ("VM_stack_pop() -> %d", *(self->head - 1));
  return *(--self->head);
}

static bool
VM_stack_initialize (vm_stack_t * stack)
{
  if ((stack->head =
       (__vm_word_t *) calloc (__N_STACK_SLOTS,
			       sizeof (__vm_word_t))) == NULL)
    return false;
  DBG ("initialized stack successfully");
  stack->capacity = __N_STACK_SLOTS;
  stack->is_empty = VM_stack_isempty;
  stack->push = VM_stack_push;
  stack->pop = VM_stack_pop;
  return true;
}

static __vm_word_t
VM_interpret_value (__vm_word_t value)
{
  vm_state_t *state = __VM_get_state ();
  if (value >= 32768)
    return state->registers[value - 32768];
  return value;
}

static __vm_word_t *
VM_interpret_lvalue (__vm_word_t value)
{
  vm_state_t *state = __VM_get_state ();
  if (value >= 32768)
    return &state->registers[value - 32768];
  return (__vm_word_t *) (&state->buffer.buffer[value]);
}

static void
print_coredump (void)
{
  vm_state_t *state = __VM_get_state ();
  puts ("\t\tCoredump\n\tRegisters");
  for (size_t i = 0;
       i < sizeof (state->registers) / sizeof (*state->registers); ++i)
    printf ("r%zu=%d ", i, state->registers[i]);
  puts ("\n\tStack");
  for (size_t i = 0; i < state->stack->size; ++i)
    printf ("-%zu: %d\n", i, state->stack->pop ());
}

DEFINE_OPCODE (halt)
{
  print_coredump ();
  VM_error ("program halted");
}

DEFINE_OPCODE (set)
{
  *VM_interpret_lvalue (operands[0]) = VM_interpret_value (operands[1]);
}

DEFINE_OPCODE (push)
{
  VM_stack_push (VM_interpret_value (operands[0]));
}

DEFINE_OPCODE (pop)
{
  if (VM_stack_isempty ())
    VM_error ("tried to pop when stack is empty");
  *VM_interpret_lvalue (operands[0]) = VM_stack_pop ();
}

DEFINE_OPCODE (eq)
{
  *VM_interpret_lvalue (operands[0]) = VM_interpret_value (operands[1])
    == VM_interpret_value (operands[2]);
}

DEFINE_OPCODE (gt)
{
  *VM_interpret_lvalue (operands[0]) = VM_interpret_value (operands[1])
    > VM_interpret_value (operands[2]);
}

DEFINE_OPCODE (jmp)
{
  self->pc = VM_interpret_value (operands[0]) * sizeof (__vm_word_t);
}

DEFINE_OPCODE (jt)
{
  if (VM_interpret_value (operands[0]))
    OPCODE (jmp) (self, &operands[1]);
}

DEFINE_OPCODE (jf)
{
  if (!VM_interpret_value (operands[0]))
    OPCODE (jmp) (self, &operands[1]);
}

DEFINE_OPCODE (add)
{
  *VM_interpret_lvalue (operands[0])
    = (VM_interpret_value (operands[1]) + VM_interpret_value (operands[2]))
    % 32768;
}

DEFINE_OPCODE (mult)
{
  *VM_interpret_lvalue (operands[0])
    = (VM_interpret_value (operands[1]) * VM_interpret_value (operands[2]))
    % 32768;
}

DEFINE_OPCODE (mod)
{
  *VM_interpret_lvalue (operands[0])
    = VM_interpret_value (operands[1]) % VM_interpret_value (operands[2]);
}

DEFINE_OPCODE (and_)
{
  *VM_interpret_lvalue (operands[0])
    = VM_interpret_value (operands[1]) & VM_interpret_value (operands[2]);
}

DEFINE_OPCODE (or_)
{
  *VM_interpret_lvalue (operands[0])
    = VM_interpret_value (operands[1]) | VM_interpret_value (operands[2]);
}

DEFINE_OPCODE (not_)
{
  *VM_interpret_lvalue (operands[0]) =
    ~VM_interpret_value (operands[1]) & 0x7fff;
}

DEFINE_OPCODE (rmem)
{
  *VM_interpret_lvalue (operands[0]) =
    VM_read_word_at (self, VM_interpret_value (operands[1]));
}

DEFINE_OPCODE (wmem)
{
  *VM_interpret_lvalue (2 * VM_interpret_value (operands[0])) =
    VM_interpret_value (operands[1]);
}

DEFINE_OPCODE (call)
{
  VM_stack_push ((__vm_word_t) ceil ((float) self->pc / 2.0));
  OPCODE (jmp) (self, operands);
}

DEFINE_OPCODE (ret)
{
  self->pc = VM_stack_pop ();
}

DEFINE_OPCODE (out)
{
  putchar ((char) operands[0]);
}

DEFINE_OPCODE (in)
{
  *VM_interpret_lvalue (operands[0]) = (__vm_word_t) getchar ();
}

DEFINE_OPCODE (nop)
{
  (void) self;
  (void) operands;
}

#ifdef DEBUG
static void
print_instruction (opcode_t opcode, __vm_word_t * operands)
{
  printf ("%s ", opcode.name);
  for (size_t i = 0; i < opcode.n_params; ++i)
    if (operands[i] < 32768)
      printf ("#%d, ", operands[i]);
    else
      printf ("r%d, ", operands[i] - 32768);
  printf ("\n");
}
#endif

typedef void (*__vm_opcode_fn_t) (vm_state_t *, __vm_word_t *);
__vm_opcode_fn_t opcode_fn_table[22] = {
  OPCODE (halt),
  OPCODE (set),
  OPCODE (push),
  OPCODE (pop),
  OPCODE (eq),
  OPCODE (gt),
  OPCODE (jmp),
  OPCODE (jt),
  OPCODE (jf),
  OPCODE (add),
  OPCODE (mult),
  OPCODE (mod),
  OPCODE (and_),
  OPCODE (or_),
  OPCODE (not_),
  OPCODE (rmem),
  OPCODE (wmem),
  OPCODE (call),
  OPCODE (ret),
  OPCODE (out),
  OPCODE (in),
  OPCODE (nop)
};

static bool
VM_execute (void)
{
  vm_state_t *self = __VM_get_state ();
  DBG ("beginning execution");
  while (self->pc < self->buffer.size)
    {
#ifdef DEBUG
      printf ("%hu: ", self->pc / 2);
#endif
      __vm_word_t operands[3] = { 0 };
      __vm_word_t opcode_idx = VM_read_word (self);
      if (opcode_idx >= sizeof (opcode_fn_table) / sizeof (*opcode_fn_table))
	{
	  DBG ("operand with idx.: %d\n", opcode_idx);
	  VM_error ("misaligned jmp, or operand not implemented yet");
	}
      opcode_t opcode = OPCODE_LIST[opcode_idx];
      size_t i;
      for (i = 0; i < opcode.n_params; ++i)
	{
	  __vm_word_t operand = VM_read_word (self);
	  if (operand >= 32776)
	    VM_error ("operand overflow caught");
	  operands[i] = operand;
	}
#ifdef DEBUG
      printf ("%hu: ", self->pc / 2);
#endif
      if (i != opcode.n_params)
	VM_error ("opcode received incorrect number of opcodes");
#ifdef DEBUG
      print_instruction (opcode, operands);
#endif
      opcode_fn_table[opcode_idx] (self, operands);
    }
  return true;
}

bool
VM_initialize (vm_state_t * state)
{
  state->load = VM_load;
  state->free = VM_free;
  state->execute = VM_execute;
  state->buffer.buffer = NULL;

  vm_stack_t *stack = (vm_stack_t *) malloc (sizeof (vm_stack_t));
  stack->head = NULL;
  stack->size = 0;
  stack->capacity = __N_STACK_SLOTS;

  state->stack = stack;
  __current_state.stack = stack;
  __current_state.state = state;
  if (!VM_stack_initialize (stack))
    {
      VM_set_traceback ("VM_stack_initialize",
			"failed to allocate sufficient memory");
      return false;
    }
  memset (state->registers, 0, sizeof (__vm_register_t) * __N_REGISTERS);
  DBG ("initialized VM state successfully");
  return true;
}

#endif /* __VM_H */
