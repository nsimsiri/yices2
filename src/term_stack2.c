/*
 * Stack-based API for building terms and types
 * Intended to support parsing.
 */

#if defined(CYGWIN) || defined(MINGW)
#ifndef __YICES_DLLSPEC__
#define __YICES_DLLSPEC__ __declspec(dllexport)
#endif
#endif

#include <assert.h>
#include <string.h>

#include "memalloc.h"
#include "hash_functions.h"
#include "bv_constants.h"
#include "bv64_constants.h"

#include "bvarith_buffer_terms.h"
#include "bvarith64_buffer_terms.h"

#include "yices.h"
#include "yices_extensions.h"
#include "yices_globals2.h"

#include "term_stack2.h"


#ifndef NDEBUG

#include <stdio.h>
#include <inttypes.h>

#include "type_printer.h"
#include "term_printer.h"

#endif


/****************
 *  EXCEPTIONS  *
 ***************/

/*
 * Exception raised when processing element e
 * - stack->error_pos is set to e->pos
 * - stack->error_op is set to stack->top_op
 * - stack->error_string is set to e's string field if e is a symbol or a string
 *   or a binding, or to NULL otherwise.
 * code is returned to exception handler by longjmp
 */
void __attribute__((noreturn)) raise_exception(tstack_t *stack, stack_elem_t *e, int code) {
  stack->error_loc = e->loc;
  stack->error_op = stack->top_op;
  switch (e->tag) {
  case TAG_SYMBOL:
  case TAG_STRING:
    stack->error_string = e->val.string;
    break;
  case TAG_BINDING:
    stack->error_string = e->val.binding.symbol;
    break;
  case TAG_TYPE_BINDING:
    stack->error_string = e->val.type_binding.symbol;
    break;
  default:
    stack->error_string = NULL;
    break;
  }
  longjmp(stack->env, code);
}

/*
 * Exception on a push_op operation
 * - loc = corresponding loc
 * - code = error code
 */
#ifndef NDEBUG
static void __attribute__((noreturn)) bad_op_exception(tstack_t *stack, loc_t *loc, uint32_t op) {
  stack->error_loc = *loc;
  stack->error_op = op;
  stack->error_string = NULL;
  longjmp(stack->env, TSTACK_INVALID_OP);
}
#endif


/*
 * Bad format or other error on a push_rational, push_float, push_bvbin, push_hexbin operation, etc.
 */
void __attribute__((noreturn)) push_exception(tstack_t *stack, loc_t *loc, char *s, int code) {
  stack->error_loc = *loc;
  stack->error_op = NO_OP;
  stack->error_string = s;
  longjmp(stack->env, code);
}

/*
 * Translate a yices error into an exception.
 */
void __attribute__((noreturn)) report_yices_error(tstack_t *stack) {
  uint32_t i;

  i = stack->frame;
  stack->error_loc = stack->elem[i].loc;
  stack->error_op = stack->top_op;
  stack->error_string = NULL;
  longjmp(stack->env, TSTACK_YICES_ERROR);
}


/********************
 *  OPERATOR TABLE  *
 *******************/

/*
 * Allocate a table of size n
 */
static void alloc_op_table(op_table_t *table, uint32_t n) {
  assert(n <= MAX_OP_TABLE_SIZE);
  table->assoc = (uint8_t *) safe_malloc(n * sizeof(uint8_t));
  table->eval = (eval_fun_t *) safe_malloc(n * sizeof(eval_fun_t));
  table->check = (check_fun_t *) safe_malloc(n * sizeof(check_fun_t));
  table->num_ops = 0;
  table->size = n;
}

/*
 * Delete
 */
static void delete_op_table(op_table_t *table) {
  safe_free(table->assoc);
  safe_free(table->eval);
  safe_free(table->check);
  table->assoc = NULL;
  table->eval = NULL;
  table->check = NULL;
}


/**************************
 *  STACK INITIALIZATION  *
 *************************/

/*
 * Initialize stack:
 * - nops = size of the operator table
 */
static void alloc_tstack(tstack_t *stack, uint32_t nops) {
  uint32_t n;
  stack_elem_t *tmp;

  n = DEFAULT_TERM_STACK_SIZE;
  tmp = (stack_elem_t *) safe_malloc(n * sizeof(stack_elem_t));

  // mark bottom element
  tmp->tag = TAG_OP;
  tmp->val.opval.opcode = NO_OP;
  tmp->val.opval.multiplicity = 0;
  tmp->val.opval.prev = 0;

  stack->elem = tmp;
  stack->top = 1;
  stack->size = n;
  stack->frame = 0;
  stack->top_op = NO_OP;

  alloc_op_table(&stack->op_table, nops);

  init_arena(&stack->mem);

  stack->aux_buffer = (int32_t *) safe_malloc(DEFAULT_AUX_SIZE * sizeof(int32_t));
  stack->aux_size = DEFAULT_AUX_SIZE;

  init_bvconstant(&stack->bvconst_buffer);

  stack->bva64buffer = NULL;
  stack->bvabuffer = NULL;
  stack->bvlbuffer = NULL;

  stack->tvar_id = 0;

  stack->avtbl = NULL;

  stack->error_op = NO_OP;
  stack->error_loc.line = 0;
  stack->error_loc.column = 0;
  stack->error_string = NULL;
}


/*
 * Extend: increase size by 50%
 */
static void tstack_extend(tstack_t *stack) {
  uint32_t n;

  n = stack->size + 1;
  n += n >> 1;

  if (n >= MAX_TERM_STACK_SIZE) {
    out_of_memory();
  }

  stack->elem = (stack_elem_t *) safe_realloc(stack->elem, n * sizeof(stack_elem_t));
  stack->size = n;
}


/*
 * Return top index and extend the stack if it's full
 * also increment top.
 */
static uint32_t tstack_get_top(tstack_t *stack) {
  uint32_t i;

  i = stack->top;
  stack->top ++;
  if (i >= stack->size) {
    tstack_extend(stack);
    assert(i < stack->size);
  }
  return i;
}

/*
 * Same thing but return a pointer to element i
 */
static stack_elem_t *tstack_get_topelem(tstack_t *stack) {
  uint32_t k;
  // The order is important: tstack_get_top has side effects
  // (including changing stack->elem)!!
  k = tstack_get_top(stack);
  return stack->elem + k;
}





/*********************
 *  PUSH OPERATIONS  *
 ********************/

/*
 * Push op: create a new frame and push the operator.
 *
 * For special operators (such that assoc[op] == 1) , if the current
 * top-operator is identical to op, then we just increment its
 * multiplicity index.
 *
 * If op is BIND, then top-op must be LET.
 *
 * For all operators except BIND we also open a new scope in the
 * arena. For BIND the arena scope remains the one open by the
 * enclosing LET.
 */
void tstack_push_op(tstack_t *stack, int32_t op, loc_t *loc) {
  uint32_t i;
  stack_elem_t *e;

#ifndef NDEBUG
  if (op < 0 ||
      op >= stack->op_table.num_ops ||
      stack->op_table.check[op] == NULL ||
      stack->op_table.eval[op] == NULL) {
    bad_op_exception(stack, loc, op);
  }
#endif

  if (stack->op_table.assoc[op] && stack->top_op == op) {
    i = stack->frame;
    stack->elem[i].val.opval.multiplicity ++;
    return;
  }

  i = tstack_get_top(stack);
  e = stack->elem + i;
  e->tag = TAG_OP;
  e->val.opval.opcode = op;
  e->val.opval.prev = stack->frame;
  e->val.opval.multiplicity = 0;
  e->loc = *loc;
  stack->top_op = op;
  stack->frame = i;

  if (op != BIND) {
    // mark start of new scope
    arena_push(&stack->mem);
  }
}


/*
 * Push opcode on top of the stack. Don't create a new frame.
 * Don't open a new scope in the arena.
 */
void tstack_push_opcode(tstack_t *stack, int32_t op, loc_t *loc) {
  stack_elem_t *e;

#ifndef NDEBUG
  if (op < 0 || op >= stack->op_table.num_ops) {
    bad_op_exception(stack, loc, op);
  }
#endif

  e = tstack_get_topelem(stack);
  e->tag = TAG_OPCODE;
  e->val.op = op;
  e->loc = *loc;
}

/*
 * Push a copy of string s, n = length of s
 * - tag = type of string (either TAG_STRING or TAG_SYMBOL)
 */
void tstack_push_str(tstack_t *stack, tag_t tag, char *s, uint32_t n, loc_t *loc) {
  char *tmp;
  stack_elem_t *e;

  assert(strlen(s) == n);

  tmp = (char *) arena_alloc(&stack->mem, n + 1);
  strcpy(tmp, s);
  e = tstack_get_topelem(stack);
  e->tag = tag;
  e->val.string = tmp;
  e->loc = *loc;
}


/*
 * For define-type or define term: push a name s on the stack.
 *
 * These functions are like push_symbol but they raise an exception if
 * name is already used (TSTACK_TYPENAME_REDEF, TSTACK_TERMNAME_REDEF,
 * or TSTACK_MACRO_REDEF)
 */
void tstack_push_free_typename(tstack_t *stack, char *s, uint32_t n, loc_t *loc) {
  if (yices_get_type_by_name(s) != NULL_TYPE) {
    push_exception(stack, loc, s, TSTACK_TYPENAME_REDEF);
  }
  tstack_push_str(stack, TAG_SYMBOL, s, n, loc);
}

void tstack_push_free_termname(tstack_t *stack, char *s, uint32_t n, loc_t *loc) {
  if (yices_get_term_by_name(s) != NULL_TERM) {
    push_exception(stack, loc, s, TSTACK_TERMNAME_REDEF);
  }
  tstack_push_str(stack, TAG_SYMBOL, s, n, loc);
}

/*
 * Convert a string to a rational and push that
 * - s must be null-terminated and of rational or floating point formats
 */
void tstack_push_rational(tstack_t *stack, char *s, loc_t *loc) {
  stack_elem_t *e;
  int code;

  e = tstack_get_topelem(stack);
  e->tag = TAG_RATIONAL;
  e->loc = *loc;
  q_init(&e->val.rational);
  code = q_set_from_string(&e->val.rational, s);
  if (code < 0) {
    // -1 means that the format is wrong
    // -2 means that the denominator is zero
    if (code == -1) {
      push_exception(stack, loc, s, TSTACK_RATIONAL_FORMAT);
    } else {
      assert(code == -2);
      push_exception(stack, loc, s, TSTACK_DIVIDE_BY_ZERO);
    }
  }
}

void tstack_push_float(tstack_t *stack, char *s, loc_t *loc) {
  stack_elem_t *e;
  int code;

  e = tstack_get_topelem(stack);
  e->tag = TAG_RATIONAL;
  e->loc = *loc;
  q_init(&e->val.rational);
  code = q_set_from_float_string(&e->val.rational, s);
  if (code < 0) {
    push_exception(stack, loc, s, TSTACK_FLOAT_FORMAT);
  }
}


/*
 * Push a small bitvector constant:
 * - n = bitsize (1 <= n <= 64)
 * - c = value
 */
static void tstack_push_bv64(tstack_t *stack, uint32_t n, uint64_t c, loc_t *loc) {
  stack_elem_t *e;

  assert(1 <= n && n <= 64 && c == norm64(c, n));

  e = tstack_get_topelem(stack);
  e->tag = TAG_BV64;
  e->val.bv64.bitsize = n;
  e->val.bv64.value = c;
  e->loc = *loc;
}


/*
 * Push a generic bitvector constant
 * - n = bitsize (n > 64)
 * - c = value as an array of words
 */
static void tstack_push_bv(tstack_t *stack, uint32_t n, uint32_t *c, loc_t *loc) {
  stack_elem_t *e;

  assert(n > 64);

  e = tstack_get_topelem(stack);
  e->tag = TAG_BV;
  e->val.bv.bitsize = n;
  e->val.bv.data = c;
  e->loc = *loc;
}


/*
 * Convert a string to a bitvector constant and push that
 * - n = length of the string
 * - s must be a string of binary or hexadecimal digits (no prefix)
 */
void tstack_push_bvbin(tstack_t *stack, char *s, uint32_t n, loc_t *loc) {
  uint32_t k;
  int code;
  uint32_t *tmp;
  uint64_t c;

  if (n > 64) {
    // large constant
    k = (n + 31) >> 5; // number of words
    tmp = bvconst_alloc(k);
    code = bvconst_set_from_string(tmp, n, s);
    if (code < 0) goto error;

    bvconst_normalize(tmp, n);
    tstack_push_bv(stack, n, tmp, loc);

  } else {
    // small constant
    code = bvconst64_set_from_string(&c, n, s);
    if (code < 0) goto error;
    tstack_push_bv64(stack, n, c, loc);
  }
  return;

 error:
  push_exception(stack, loc, s, TSTACK_BVBIN_FORMAT);
}

void tstack_push_bvhex(tstack_t *stack, char *s, uint32_t n, loc_t *loc) {
  uint32_t k;
  int code;
  uint32_t *tmp;
  uint64_t c;

  if (n > 16) {
    // large constant
    k = (n + 7) >> 3; // number of words
    tmp = bvconst_alloc(k);
    code = bvconst_set_from_hexa_string(tmp, n, s);
    if (code < 0) goto error;

    bvconst_normalize(tmp, 4 * n);
    tstack_push_bv(stack, 4 * n, tmp, loc);

  } else {
    // small constant
    code = bvconst64_set_from_hexa_string(&c, n, s);
    if (code < 0) goto error;
    tstack_push_bv64(stack, 4 * n, c, loc);
  }
  return;

 error:
  push_exception(stack, loc, s, TSTACK_BVHEX_FORMAT);
}


/*
 * Convert a name to a type or a term and push the type or term on the stack
 */
void tstack_push_type_by_name(tstack_t *stack, char *s, loc_t *loc) {
  stack_elem_t *e;
  type_t tau;

  tau = yices_get_type_by_name(s);
  if (tau == NULL_TYPE) push_exception(stack, loc, s, TSTACK_UNDEF_TYPE);

  e = tstack_get_topelem(stack);
  e->tag = TAG_TYPE;
  e->val.type = tau;
  e->loc = *loc;
}

void tstack_push_term_by_name(tstack_t *stack, char *s, loc_t *loc) {
  stack_elem_t *e;
  term_t t;

  t = yices_get_term_by_name(s);
  if (t == NULL_TERM) push_exception(stack, loc, s, TSTACK_UNDEF_TERM);

  e = tstack_get_topelem(stack);
  e->tag = TAG_TERM;
  e->val.term = t;
  e->loc = *loc;
}


/*
 * Push primitive types or terms
 */
void tstack_push_bool_type(tstack_t *stack, loc_t *loc) {
  stack_elem_t *e;

  e = tstack_get_topelem(stack);
  e->tag = TAG_TYPE;
  e->val.type = yices_bool_type();
  e->loc = *loc;
}

void tstack_push_true(tstack_t *stack, loc_t *loc) {
  stack_elem_t *e;

  e = tstack_get_topelem(stack);
  e->tag = TAG_TERM;
  e->val.term = yices_true();
  e->loc = *loc;
}

void tstack_push_false(tstack_t *stack, loc_t *loc) {
  stack_elem_t *e;

  e = tstack_get_topelem(stack);
  e->tag = TAG_TERM;
  e->val.term = yices_false();
  e->loc = *loc;
}


/*
 * Push an integer constant
 */
void tstack_push_int32(tstack_t *stack, int32_t x, loc_t *loc) {
  stack_elem_t *e;

  e = tstack_get_topelem(stack);
  e->tag = TAG_RATIONAL;
  e->loc = *loc;
  q_init(&e->val.rational);
  q_set32(&e->val.rational, x);
}


/*
 * Terms or types
 */
void tstack_push_term(tstack_t *stack, term_t t, loc_t *loc) {
  stack_elem_t *e;

  e = tstack_get_topelem(stack);
  e->tag = TAG_TERM;
  e->val.term = t;
  e->loc = *loc;
}

void tstack_push_type(tstack_t *stack, type_t tau, loc_t *loc) {
  stack_elem_t *e;

  e = tstack_get_topelem(stack);
  e->tag = TAG_TYPE;
  e->val.type = tau;
  e->loc = *loc;
}


/**********************
 *  INTERNAL BUFFERS  *
 *********************/

/*
 * Invariant we want to maintain:
 * - stack->abuffer is either NULL or it's pointing to the last
 *   arithmetic_buffer allocated and that buffer does not occur in the stack.
 * - if an element in the stack has tag = TAG_ARITH_BUFFER
 *   then its value is a pointer to an arithmetic buffer != stack->abuffer.
 * Same thing for stack->bvabuffer and stack->bvlbuffer.
 */
bvarith64_buffer_t *tstack_get_bva64buffer(tstack_t *stack, uint32_t bitsize) {
  bvarith64_buffer_t *tmp;

  assert(1 <= bitsize && bitsize <= 64);

  tmp = stack->bva64buffer;
  if (tmp == NULL) {
    tmp = yices_new_bvarith64_buffer(bitsize);
    stack->bva64buffer = tmp;
  } else {
    bvarith64_buffer_prepare(tmp, bitsize); // reset to zero and set size to bitsize
  }

  assert(bvarith64_buffer_is_zero(tmp) && bvarith64_buffer_bitsize(tmp) == bitsize);

  return tmp;
}

bvarith_buffer_t *tstack_get_bvabuffer(tstack_t *stack, uint32_t bitsize) {
  bvarith_buffer_t *tmp;

  assert(64 < bitsize && bitsize <= YICES_MAX_BVSIZE);

  tmp = stack->bvabuffer;
  if (tmp == NULL) {
    tmp = yices_new_bvarith_buffer(bitsize);
    stack->bvabuffer = tmp;
  } else {
    bvarith_buffer_prepare(tmp, bitsize); // reset to zero and set size
  }

  assert(bvarith_buffer_is_zero(tmp) && bvarith_buffer_bitsize(tmp) == bitsize);

  return tmp;
}

bvlogic_buffer_t *tstack_get_bvlbuffer(tstack_t *stack) {
  bvlogic_buffer_t *tmp;

  tmp = stack->bvlbuffer;
  if (tmp == NULL) {
    tmp = yices_new_bvlogic_buffer();
    stack->bvlbuffer = tmp;
  } else {
    bvlogic_buffer_clear(tmp);
  }
  return tmp;
}


/*
 * Free or recycle a buffer
 */
static void recycle_bva64buffer(tstack_t *stack, bvarith64_buffer_t *b) {
  if (stack->bva64buffer == NULL) {
    bvarith64_buffer_prepare(b, 32); // any non-zero value would work
    stack->bva64buffer = b;
  } else if (stack->bva64buffer != b) {
    yices_free_bvarith64_buffer(b);
  }
}

static void recycle_bvabuffer(tstack_t *stack, bvarith_buffer_t *b) {
  if (stack->bvabuffer == NULL) {
    bvarith_buffer_prepare(b, 100); // any non-zero value would work
    stack->bvabuffer = b;
  } else if (stack->bvabuffer != b) {
    yices_free_bvarith_buffer(b);
  }
}

static void recycle_bvlbuffer(tstack_t *stack, bvlogic_buffer_t *b) {
  if (stack->bvlbuffer == NULL) {
    bvlogic_buffer_clear(b);
    stack->bvlbuffer = b;
  } else if (stack->bvlbuffer != b) {
    yices_free_bvlogic_buffer(b);
  }
}


/*
 * Make the auxiliary buffer large enough for n terms or types
 */
void extend_aux_buffer(tstack_t *stack, uint32_t n) {
  uint32_t new_size;
  assert (stack->aux_size < n);

  new_size = stack->aux_size + 1;
  new_size += new_size;
  if (new_size < n) new_size = n;

  if (new_size  >= MAX_AUX_SIZE) {
    out_of_memory();
  }

  stack->aux_buffer = (int32_t *) safe_realloc(stack->aux_buffer, new_size * sizeof(int32_t));
  stack->aux_size  = new_size;
}


static inline int32_t *get_aux_buffer(tstack_t *stack, uint32_t n) {
  if (stack->aux_size < n) {
    extend_aux_buffer(stack, n);
  }
  return stack->aux_buffer;
}



/*********************
 *  POP OPERATIONS   *
 ********************/

/*
 * Cleanup object e (before it gets removed from the stack)
 */
static void tstack_free_val(tstack_t *stack, stack_elem_t *e) {
  uint32_t k;

  switch (e->tag) {
  case TAG_BV:
    k = (e->val.bv.bitsize + 31) >> 5;
    bvconst_free(e->val.bv.data, k);
    break;
  case TAG_RATIONAL:
    q_clear(&e->val.rational);
    break;
  case TAG_ATTRIBUTE:
    assert(stack->avtbl != NULL);
    if (e->val.aval != AVAL_NULL) {
      aval_decref(stack->avtbl, e->val.aval);
    }
    break;
  case TAG_BVARITH64_BUFFER:
    recycle_bva64buffer(stack, e->val.bvarith64_buffer);
    break;
  case TAG_BVARITH_BUFFER:
    recycle_bvabuffer(stack, e->val.bvarith_buffer);
    break;
  case TAG_BVLOGIC_BUFFER:
    recycle_bvlbuffer(stack, e->val.bvlogic_buffer);
    break;
  case TAG_BINDING:
    yices_remove_term_name(e->val.binding.symbol);
    break;
  case TAG_TYPE_BINDING:
    yices_remove_type_name(e->val.type_binding.symbol);
    break;
  default:
    break; // prevent GCC warning
  }
}


/*
 * Empty the stack and clear error data
 */
void tstack_reset(tstack_t *stack) {
  stack_elem_t *e;
  uint32_t i;

  i = stack->top;
  e = stack->elem + i;
  while (i > 0) {
    i --;
    e --;
    tstack_free_val(stack, e);
  }

  arena_reset(&stack->mem);
  stack->top = 1;
  stack->frame = 0;
  stack->top_op = NO_OP;

  stack->tvar_id = 0;

  stack->error_op = NO_OP;
  stack->error_loc.line = 0;
  stack->error_loc.column = 0;
  stack->error_string = NULL;
}


/*
 * Remove the elements above the top-frame index
 * (i.e. all the parameters in the top frame, but not the operator)
 *
 * If top-op is not BIND also close the arena scope.
 */
void tstack_pop_frame(tstack_t *stack) {
  uint32_t i, n;
  int32_t op;

  op = stack->top_op;
  n = stack->frame;

  assert(0 < n && n < stack->top);

  // restore previous frame and top_op
  i = stack->elem[n].val.opval.prev;
  stack->frame = i;
  stack->top_op = stack->elem[i].val.opval.opcode;

  // remove elements at indices n+1 to stack->top-1
  i = stack->top;
  n ++;
  while (i > n) {
    i --;
    tstack_free_val(stack, stack->elem + i);
  }
  stack->top = n;

  if (op != BIND) {
    arena_pop(&stack->mem);
  }
}


/*
 * Copy v as result in place of the current stack->frame
 * then remove all elements above the top frame index.
 *
 * Cannot be used if v is a string/symbol.
 */
void copy_result_and_pop_frame(tstack_t *stack, stack_elem_t *v) {
  uint32_t i, n;
  int32_t op;

  op = stack->top_op;
  n = stack->frame;

  assert(0 < n && n < stack->top);
  assert(&stack->elem[n] < v && v < stack->elem + stack->top);
  assert(v->tag != TAG_SYMBOL);

  // restore previous frame and top_op
  i = stack->elem[n].val.opval.prev;
  stack->frame = i;
  stack->top_op = stack->elem[i].val.opval.opcode;


  stack->elem[n] = *v;
  v->tag = TAG_NONE; // prevent deletion of v's value (since it's copied in elem[n])

  // remove elements at indices n+1 to stack->top-1
  i = stack->top;
  n ++;
  while (i > n) {
    i --;
    tstack_free_val(stack, stack->elem + i);
  }
  stack->top = n;

  if (op != BIND) {
    arena_pop(&stack->mem);
  }
}


/*
 * Replace the top element by its new value
 * - to be used after tstack_pop_frame to replace the operator
 *   by the result of the operation
 * - keep the loc field unchanged
 */
void set_term_result(tstack_t *stack, term_t t) {
  stack_elem_t *e;

  e = stack->elem + (stack->top - 1);
  e->tag = TAG_TERM;
  e->val.term = t;
}

void set_type_result(tstack_t *stack, type_t tau) {
  stack_elem_t *e;

  e = stack->elem + (stack->top - 1);
  e->tag = TAG_TYPE;
  e->val.type = tau;
}

// b must be stack->bva64buffer
void set_bvarith64_result(tstack_t *stack, bvarith64_buffer_t *b) {
  stack_elem_t *e;

  assert(b == stack->bva64buffer);
  stack->bva64buffer = NULL;

  e = stack->elem + (stack->top - 1);
  e->tag = TAG_BVARITH64_BUFFER;
  e->val.bvarith64_buffer = b;
}

// b must be stack->bvabuffer
void set_bvarith_result(tstack_t *stack, bvarith_buffer_t *b) {
  stack_elem_t *e;

  assert(b == stack->bvabuffer);
  stack->bvabuffer = NULL;

  e = stack->elem + (stack->top - 1);
  e->tag = TAG_BVARITH_BUFFER;
  e->val.bvarith_buffer = b;
}

// b must be stack->bvlbuffer
void set_bvlogic_result(tstack_t *stack, bvlogic_buffer_t *b) {
  stack_elem_t *e;

  assert(b == stack->bvlbuffer);
  stack->bvlbuffer = NULL;

  e = stack->elem + (stack->top - 1);
  e->tag = TAG_BVLOGIC_BUFFER;
  e->val.bvlogic_buffer = b;
}

void set_binding_result(tstack_t *stack, term_t t, char *symbol) {
  stack_elem_t *e;

  e = stack->elem + (stack->top - 1);
  e->tag = TAG_BINDING;
  e->val.binding.term = t;
  e->val.binding.symbol = symbol;
}

void set_bv64_result(tstack_t *stack, uint32_t nbits, uint64_t c) {
  stack_elem_t *e;

  e = stack->elem + (stack->top - 1);
  e->tag = TAG_BV64;
  e->val.bv64.bitsize = nbits;
  e->val.bv64.value = c;
}

void set_bv_result(tstack_t *stack, uint32_t nbits, uint32_t *bv) {
  stack_elem_t *e;

  e = stack->elem + (stack->top - 1);
  e->tag = TAG_BV;
  e->val.bv.bitsize = nbits;
  e->val.bv.data = bv;
}

void set_type_binding_result(tstack_t *stack, type_t tau, char *symbol) {
  stack_elem_t *e;

  e = stack->elem + (stack->top - 1);
  e->tag = TAG_TYPE_BINDING;
  e->val.type_binding.type = tau;
  e->val.type_binding.symbol = symbol;
}


void set_aval_result(tstack_t *stack, aval_t v) {
  stack_elem_t *e;

  assert(stack->avtbl != NULL);

  e = stack->elem + (stack->top - 1);
  e->tag = TAG_ATTRIBUTE;
  e->val.aval = v;

  if (v != AVAL_NULL) {
    aval_incref(stack->avtbl, v);
  }
}



// no result: remove the top element
static inline void no_result(tstack_t *stack) {
  stack->top --;
}



#if 0

/*
 * Print element e (for debugging)
 */
static void print_elem(tstack_t *stack, stack_elem_t *e) {
  switch (e->tag) {
  case TAG_NONE:
    printf("<none>");
    break;

  case TAG_OP:
    printf("<op: code = %"PRId32", mult = %"PRIu32", prev = %"PRIu32">", e->val.opval.opcode,
           e->val.opval.multiplicity,e->val.opval.prev);
    break;

  case TAG_OPCODE:
    printf("<opcode: %"PRId32">", e->val.op);
    break;

  case TAG_SYMBOL:
    printf("<symbol: %s>", e->val.string);
    break;

  case TAG_BV64:
    printf("<bitvector: ");
    bvconst64_print(stdout, e->val.bv64.value, e->val.bv64.bitsize);
    printf(">");
    break;

  case TAG_BV:
    printf("<bitvector: ");
    bvconst_print(stdout, e->val.bv.data, e->val.bv.bitsize);
    printf(">");
    break;

  case TAG_RATIONAL:
    printf("<rational: ");
    q_print(stdout, &e->val.rational);
    printf(">");
    break;

  case TAG_TERM:
    printf("<term: ");
    print_term_id(stdout, e->val.term);
    printf(">");
    break;

  case TAG_TYPE:
    printf("<type: ");
    print_type_id(stdout, e->val.term);
    printf(">");
    break;

  case TAG_MACRO:
    printf("<macro: %"PRId32">", e->val.macro);
    break;

  case TAG_ARITH_BUFFER:
    printf("<arith-buffer: ");
    print_arith_buffer(stdout, e->val.arith_buffer);
    printf(">");
    break;

  case TAG_BVARITH64_BUFFER:
    printf("<bvarith64-buffer: ");
    print_bvarith64_buffer(stdout, e->val.bvarith64_buffer);
    printf(">");
    break;

  case TAG_BVARITH_BUFFER:
    printf("<bvarith-buffer: ");
    print_bvarith_buffer(stdout, e->val.bvarith_buffer);
    printf(">");
    break;

  case TAG_BVLOGIC_BUFFER:
    printf("<bvlogic-buffer: ");
    print_bvlogic_buffer(stdout, e->val.bvlogic_buffer);
    printf(">");
    break;

  case TAG_BINDING:
    printf("<binding: %s --> ", e->val.binding.symbol);
    print_term_id(stdout, e->val.binding.term);
    printf(">");
    break;

  case TAG_TYPE_BINDING:
    printf("<type-binding: %s --> ", e->val.type_binding.symbol);
    print_type_id(stdout, e->val.type_binding.type);
    printf(">");
    break;

  default:
    printf("<error>");
    break;
  }
}

#endif



/***************************************
 *  EVALUATION OF INDIVIDUAL COMMANDS  *
 **************************************/

/*
 * Auxiliary functions
 */
static int invalid_tag(tag_t tg) {
  int error_code;

  switch (tg) {
  case TAG_SYMBOL:
    error_code = TSTACK_NOT_A_SYMBOL;
    break;

  case TAG_RATIONAL:
    error_code = TSTACK_NOT_A_RATIONAL;
    break;

  case TAG_TYPE:
    error_code = TSTACK_NOT_A_TYPE;
    break;

  default:
    error_code = TSTACK_INTERNAL_ERROR;
  }

  return error_code;
}

void check_tag(tstack_t *stack, stack_elem_t *e, tag_t tg) {
  if (e->tag != tg) raise_exception(stack, e, invalid_tag(tg));
}

void check_op(tstack_t *stack, int32_t op) {
  if (stack->top_op != op) {
    raise_exception(stack, stack->elem + stack->frame, TSTACK_INTERNAL_ERROR);
  }
}

void check_size(tstack_t *stack, bool cond) {
  if (! cond) {
    raise_exception(stack, stack->elem + stack->frame, TSTACK_INVALID_FRAME);
  }
}

void check_all_tags(tstack_t *stack, stack_elem_t *e, stack_elem_t *end, tag_t tg) {
  while (e < end) {
    check_tag(stack, e, tg);
    e ++;
  }
}

static void check_type(tstack_t *stack, type_t tau) {
  if (tau == NULL_TYPE) report_yices_error(stack);
}

static void check_term(tstack_t *stack, term_t t) {
  if (t == NULL_TERM) report_yices_error(stack);
}



/*
 * Check whether n string s1...sn are all distinct (for small n).
 */
// pair: string + hash code
typedef struct tagged_string_s {
  uint32_t hash;
  char *string;
} tagged_string_t;


/*
 * Add string s to array a, (as last element)
 * n = number of elements currently in a
 * return true if s is already in the array, false otherwise
 */
static bool check_duplicate_string(tagged_string_t *a, int32_t n, char *s) {
  uint32_t h;
  int32_t i;

  h = jenkins_hash_string(s);
  for (i=0; i<n; i++) {
    if (a[i].hash == h && strcmp(s, a[i].string) == 0) {
      return true;
    }
  }
  a[i].hash = h;
  a[i].string = s;
  return false;
}


/*
 * Check whether all names in a list of variable bindings are distinct
 * - names are in f[0] .. f[n-1]
 * - all are bindings
 */
void check_distinct_binding_names(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  uint32_t i;
  tagged_string_t check[n];

  // check for duplicate strings in the sequence
  for (i=0; i<n; i++) {
    assert(f[i].tag == TAG_BINDING);
    if (check_duplicate_string(check, i, f[i].val.binding.symbol)) {
      raise_exception(stack, f+i, TSTACK_DUPLICATE_VAR_NAME);
    }
  }
}


/*
 * Same thing for type-variable bindings
 */
void check_distinct_type_binding_names(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  uint32_t i;
  tagged_string_t check[n];

  // check for duplicate strings in the sequence
  for (i=0; i<n; i++) {
    assert(f[i].tag == TAG_TYPE_BINDING);
    if (check_duplicate_string(check, i, f[i].val.type_binding.symbol)) {
      raise_exception(stack, f+i, TSTACK_DUPLICATE_TYPE_VAR_NAME);
    }
  }
}




/*
 * CONVERSIONS
 */

/*
 * Convert element e to a term or raise an exception
 */
term_t get_term(tstack_t *stack, stack_elem_t *e) {
  uint64_t c;
  term_t t;

  switch (e->tag) {
  case TAG_TERM:
    t = e->val.term;
    break;

  case TAG_SYMBOL:
    t = yices_get_term_by_name(e->val.string);
    if (t == NULL_TERM) {
      raise_exception(stack, e, TSTACK_UNDEF_TERM);
    }
    break;

  case TAG_BV64:
    c = norm64(e->val.bv64.value, e->val.bv64.bitsize);
    t = yices_bvconst64_term(e->val.bv64.bitsize, c);
    break;

  case TAG_BV:
    bvconst_normalize(e->val.bv.data, e->val.bv.bitsize);
    t = yices_bvconst_term(e->val.bv.bitsize, e->val.bv.data);
    break;

  case TAG_BVARITH64_BUFFER:
    t = bvarith64_buffer_get_term(e->val.bvarith64_buffer);
    break;

  case TAG_BVARITH_BUFFER:
    t = bvarith_buffer_get_term(e->val.bvarith_buffer);
    break;

  case TAG_BVLOGIC_BUFFER:
    t = bvlogic_buffer_get_term(e->val.bvlogic_buffer);
    break;

  default:
    raise_exception(stack, e, TSTACK_INTERNAL_ERROR);
    break;
  }

  return t;
}



/*
 * Return integer value of e (e must have rational tag)
 * Raise an exception if e is too large or is not an integer.
 */
int32_t get_integer(tstack_t *stack, stack_elem_t *e) {
  int32_t v;

  assert(e->tag == TAG_RATIONAL);

  if (q_get32(&e->val.rational, &v)) {
    return v;
  }

  if (q_is_integer(&e->val.rational)) {
    raise_exception(stack, e, TSTACK_INTEGER_OVERFLOW);
  } else {
    raise_exception(stack, e, TSTACK_NOT_AN_INTEGER);
  }
}


/*
 * Bitsize of element e
 * - raise an exception if e is not a bitvector element
 * - also raise an exception if e is an empty bvlogic buffer
 */
static uint32_t elem_bitsize(tstack_t *stack, stack_elem_t *e) {
  term_t t;
  uint32_t n;

  switch (e->tag) {
  case TAG_BV64:
    n = e->val.bv64.bitsize;
    break;

  case TAG_BV:
    n = e->val.bv.bitsize;
    break;

  case TAG_TERM:
    t = e->val.term;
    if (! yices_check_bv_term(t)) {
      report_yices_error(stack);
    }
    n = term_bitsize(__yices_globals.terms, t);
    break;

  case TAG_BVARITH64_BUFFER:
    n = bvarith64_buffer_bitsize(e->val.bvarith64_buffer);
    break;

  case TAG_BVARITH_BUFFER:
    n = bvarith_buffer_bitsize(e->val.bvarith_buffer);
    break;

  case TAG_BVLOGIC_BUFFER:
    if (! yices_check_bvlogic_buffer(e->val.bvlogic_buffer)) {
      report_yices_error(stack);
    }
    n = bvlogic_buffer_bitsize(e->val.bvlogic_buffer);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVARITH_ERROR);
    break;
  }

  assert(0 < n && n <= YICES_MAX_BVSIZE);

  return n;
}



/*
 * Verify that element e is a bitvector term of bitsize equal to n
 * - e must have tag = TAG_TERM
 * - raise an exception if t is not
 */
static void check_bv_term(tstack_t *stack, stack_elem_t *e, uint32_t n) {
  term_t t;

  assert(e->tag == TAG_TERM);
  t = e->val.term;

  if (! yices_check_bv_term(t)) {
    report_yices_error(stack);
  }
  if (term_bitsize(__yices_globals.terms, t) != n) {
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
  }
}



/*
 * BIT-VECTOR ARITHMETIC: BITSIZE BETWEEN 1 and 64
 */

/*
 * Add element e to buffer b.
 * - raise an exception if e is not a bitvector of equal size as b
 */
void bva64_add_elem(tstack_t *stack, bvarith64_buffer_t *b, stack_elem_t *e) {
  uint32_t n;
  term_t t;

  n = bvarith64_buffer_bitsize(b);

  assert(1 <= n && n <= 64);

  switch (e->tag) {
  case TAG_BV64:
    if (e->val.bv64.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvarith64_buffer_add_const(b, e->val.bv64.value);
    break;

  case TAG_BV:
    assert(e->val.bv.bitsize != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_TERM:
    check_bv_term(stack, e, n);
    bvarith64_buffer_add_term(b, __yices_globals.terms, e->val.term);
    break;

  case TAG_BVARITH64_BUFFER:
    if (bvarith64_buffer_bitsize(e->val.bvarith64_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvarith64_buffer_add_buffer(b, e->val.bvarith64_buffer);
    break;

  case TAG_BVARITH_BUFFER:
    assert(bvarith_buffer_bitsize(e->val.bvarith_buffer) != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_BVLOGIC_BUFFER:
    if (bvlogic_buffer_bitsize(e->val.bvlogic_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    // convert e to a term then add to b
    t = bvlogic_buffer_get_term(e->val.bvlogic_buffer);
    bvarith64_buffer_add_term(b, __yices_globals.terms, t);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVARITH_ERROR);
    break;
  }
}


/*
 * Subtract element e from buffer b.
 * - raise an exception if e is not a bitvector of equal size as b
 */
void bva64_sub_elem(tstack_t *stack, bvarith64_buffer_t *b, stack_elem_t *e) {
  uint32_t n;
  term_t t;

  n = bvarith64_buffer_bitsize(b);

  assert(1 <= n && n <= 64);

  switch (e->tag) {
  case TAG_BV64:
    if (e->val.bv64.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvarith64_buffer_sub_const(b, e->val.bv64.value);
    break;

  case TAG_BV:
    assert(e->val.bv.bitsize != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_TERM:
    check_bv_term(stack, e, n);
    bvarith64_buffer_sub_term(b, __yices_globals.terms, e->val.term);
    break;

  case TAG_BVARITH64_BUFFER:
    if (bvarith64_buffer_bitsize(e->val.bvarith64_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvarith64_buffer_sub_buffer(b, e->val.bvarith64_buffer);
    break;

  case TAG_BVARITH_BUFFER:
    assert(bvarith_buffer_bitsize(e->val.bvarith_buffer) != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_BVLOGIC_BUFFER:
    if (bvlogic_buffer_bitsize(e->val.bvlogic_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    // convert e to a term then add to b
    t = bvlogic_buffer_get_term(e->val.bvlogic_buffer);
    bvarith64_buffer_sub_term(b, __yices_globals.terms, t);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVARITH_ERROR);
    break;
  }
}



/*
 * Multiply b by element e
 * - raise an exception if e is not a bitvector of equal size as b
 *   or if there's a degree overflow
 */
void bva64_mul_elem(tstack_t *stack, bvarith64_buffer_t *b, stack_elem_t *e) {
  term_table_t *terms;
  uint32_t n;
  term_t t;

  n = bvarith64_buffer_bitsize(b);

  assert(1 <= n && n <= 64);

  switch (e->tag) {
  case TAG_BV64:
    if (e->val.bv64.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvarith64_buffer_mul_const(b, e->val.bv64.value);
    break;

  case TAG_BV:
    assert(e->val.bv.bitsize != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_TERM:
    check_bv_term(stack, e, n);
    t = e->val.term;
    terms = __yices_globals.terms;
    if (! yices_check_bvmul64_term(b, t)) {
      report_yices_error(stack);
    }
    bvarith64_buffer_mul_term(b, terms, t);
    break;

  case TAG_BVARITH64_BUFFER:
    if (bvarith64_buffer_bitsize(e->val.bvarith64_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    if (! yices_check_bvmul64_buffer(b, e->val.bvarith64_buffer)) {
      report_yices_error(stack);
    }
    bvarith64_buffer_mul_buffer(b, e->val.bvarith64_buffer);
    break;

  case TAG_BVARITH_BUFFER:
    assert(bvarith_buffer_bitsize(e->val.bvarith_buffer) != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_BVLOGIC_BUFFER:
    if (bvlogic_buffer_bitsize(e->val.bvlogic_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    // convert e to a term then add to b
    t = bvlogic_buffer_get_term(e->val.bvlogic_buffer);
    if (! yices_check_bvmul64_term(b, t)) {
      report_yices_error(stack);
    }
    bvarith64_buffer_mul_term(b, __yices_globals.terms, t);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVARITH_ERROR);
    break;
  }
}





/*
 * BIT-VECTOR ARITHMETIC: BITSIZE > 64
 */

/*
 * Add element e to buffer b.
 * - raise an exception if e is not a bitvector of equal size as b
 */
void bva_add_elem(tstack_t *stack, bvarith_buffer_t *b, stack_elem_t *e) {
  uint32_t n;
  term_t t;

  n = bvarith_buffer_bitsize(b);

  assert(n > 64);

  switch (e->tag) {
  case TAG_BV64:
    assert(e->val.bv64.bitsize != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_BV:
    if (e->val.bv.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvarith_buffer_add_const(b, e->val.bv.data);
    break;

  case TAG_TERM:
    check_bv_term(stack, e, n);
    bvarith_buffer_add_term(b, __yices_globals.terms, e->val.term);
    break;

  case TAG_BVARITH64_BUFFER:
    assert(bvarith64_buffer_bitsize(e->val.bvarith64_buffer) != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_BVARITH_BUFFER:
    if (bvarith_buffer_bitsize(e->val.bvarith_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvarith_buffer_add_buffer(b, e->val.bvarith_buffer);
    break;

  case TAG_BVLOGIC_BUFFER:
    if (bvlogic_buffer_bitsize(e->val.bvlogic_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    // convert e to a term then add to b
    t = bvlogic_buffer_get_term(e->val.bvlogic_buffer);
    bvarith_buffer_add_term(b, __yices_globals.terms, t);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVARITH_ERROR);
    break;
  }
}


/*
 * Subtract element e from buffer b.
 * - raise an exception if e is not a bitvector of equal size as b
 */
void bva_sub_elem(tstack_t *stack, bvarith_buffer_t *b, stack_elem_t *e) {
  uint32_t n;
  term_t t;

  n = bvarith_buffer_bitsize(b);

  assert(n > 64);

  switch (e->tag) {
  case TAG_BV64:
    assert(e->val.bv64.bitsize != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_BV:
    if (e->val.bv.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvarith_buffer_sub_const(b, e->val.bv.data);
    break;

  case TAG_TERM:
    check_bv_term(stack, e, n);
    bvarith_buffer_sub_term(b, __yices_globals.terms, e->val.term);
    break;

  case TAG_BVARITH64_BUFFER:
    assert(bvarith64_buffer_bitsize(e->val.bvarith64_buffer) != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_BVARITH_BUFFER:
    if (bvarith_buffer_bitsize(e->val.bvarith_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvarith_buffer_sub_buffer(b, e->val.bvarith_buffer);
    break;

  case TAG_BVLOGIC_BUFFER:
    if (bvlogic_buffer_bitsize(e->val.bvlogic_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    // convert e to a term then add to b
    t = bvlogic_buffer_get_term(e->val.bvlogic_buffer);
    bvarith_buffer_sub_term(b, __yices_globals.terms, t);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVARITH_ERROR);
    break;
  }
}


/*
 * Multiply b by element e
 * - raise an exception if e is not a bitvector of equal size as b
 *   or if there's a degree overflow
 */
void bva_mul_elem(tstack_t *stack, bvarith_buffer_t *b, stack_elem_t *e) {
  term_table_t *terms;
  uint32_t n;
  term_t t;

  n = bvarith_buffer_bitsize(b);

  assert(n > 64);

  switch (e->tag) {
  case TAG_BV64:
    assert(e->val.bv64.bitsize != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_BV:
    if (e->val.bv.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvarith_buffer_mul_const(b, e->val.bv.data);
    break;

  case TAG_TERM:
    check_bv_term(stack, e, n);
    t = e->val.term;
    terms = __yices_globals.terms;
    if (! yices_check_bvmul_term(b, t)) {
      report_yices_error(stack);
    }
    bvarith_buffer_mul_term(b, terms, t);
    break;

  case TAG_BVARITH64_BUFFER:
    assert(bvarith64_buffer_bitsize(e->val.bvarith64_buffer) != n);
    raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    break;

  case TAG_BVARITH_BUFFER:
    if (bvarith_buffer_bitsize(e->val.bvarith_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    if (! yices_check_bvmul_buffer(b, e->val.bvarith_buffer)) {
      report_yices_error(stack);
    }
    bvarith_buffer_mul_buffer(b, e->val.bvarith_buffer);
    break;

  case TAG_BVLOGIC_BUFFER:
    if (bvlogic_buffer_bitsize(e->val.bvlogic_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    // convert e to a term then add to b
    t = bvlogic_buffer_get_term(e->val.bvlogic_buffer);
    if (! yices_check_bvmul_term(b, t)) {
      report_yices_error(stack);
    }
    bvarith_buffer_mul_term(b, __yices_globals.terms, t);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVARITH_ERROR);
    break;
  }
}



/*
 * BV-NEG (for both 64/generic bitsizes)
 */

/*
 * Store the opposite of term t in e:
 * - raise an exception if t is not a bitvector.
 * - overwrite the current value of e.
 */
static void copy_bvneg_term(tstack_t *stack, stack_elem_t *e, term_t t) {
  bvarith_buffer_t *b;
  bvarith64_buffer_t *b64;
  term_table_t *terms;
  uint32_t *tmp;
  uint32_t n, k;

  terms = __yices_globals.terms;
  if (! yices_check_bv_term(t)) {
    report_yices_error(stack);
  }

  n = term_bitsize(terms, t);

  switch (term_kind(terms, t)) {
  case BV64_CONSTANT:
    // copy the opposite of t's value into e:
    assert(n == bvconst64_term_desc(terms, t)->bitsize);
    e->tag = TAG_BV64;
    e->val.bv64.bitsize = n;
    e->val.bv64.value = - bvconst64_term_desc(terms, t)->value;
    break;

  case BV_CONSTANT:
    assert(n == bvconst_term_desc(terms, t)->bitsize);
    // allocate a buffer for the result
    k = (n + 31) >> 5; // number of words
    tmp = bvconst_alloc(k);
    bvconst_negate2(tmp, k, bvconst_term_desc(terms, t)->data); // tmp := - data
    // store the result as a BV element
    e->tag = TAG_BV;
    e->val.bv.bitsize = n;
    e->val.bv.data = tmp;
    break;

  default:
    if (n <= 64) {
      b64 = tstack_get_bva64buffer(stack, n);
      assert(bvarith64_buffer_is_zero(b64));
      bvarith64_buffer_sub_term(b64, terms, t);

      // overwrite e
      e->tag = TAG_BVARITH64_BUFFER;
      e->val.bvarith64_buffer = b64;

      // reset stack->bva64buffer to NULL
      assert(b64 == stack->bva64buffer);
      stack->bva64buffer = NULL;

    } else {
      b = tstack_get_bvabuffer(stack, n);
      assert(bvarith_buffer_is_zero(b));
      bvarith_buffer_sub_term(b, terms, t);

      // overwrite e
      e->tag = TAG_BVARITH_BUFFER;
      e->val.bvarith_buffer = b;

      // reset stack->bvabuffer to NULL
      assert(b == stack->bvabuffer);
      stack->bvabuffer = NULL;
    }
    break;
  }

}

/*
 * Negate element e in place. Raise an exception if e is not a bitvector element.
 */
void bvneg_elem(tstack_t *stack, stack_elem_t *e) {
  bvlogic_buffer_t *b;
  uint32_t k;
  term_t t;

  switch (e->tag) {
  case TAG_BV64:
    e->val.bv64.value = - e->val.bv64.value;
    break;

  case TAG_BV:
    k = (e->val.bv.bitsize + 31) >> 5; // number of words
    bvconst_negate(e->val.bv.data, k);
    break;

  case TAG_TERM:
    t = e->val.term;
    copy_bvneg_term(stack, e, t);
    break;

  case TAG_BVARITH64_BUFFER:
    bvarith64_buffer_negate(e->val.bvarith64_buffer);
    break;

  case TAG_BVARITH_BUFFER:
    bvarith_buffer_negate(e->val.bvarith_buffer);
    break;

  case TAG_BVLOGIC_BUFFER:
    b = e->val.bvlogic_buffer;
    if (! yices_check_bvlogic_buffer(b)){
      report_yices_error(stack);
    }
    // convert to a term then negate.
    t = bvlogic_buffer_get_term(b);
    recycle_bvlbuffer(stack, b); // since e is going to be overwritten
    copy_bvneg_term(stack, e, t);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVARITH_ERROR);
    break;
  }
}




/*
 * BITVECTOR LOGICAL OPERATIONS
 */

/*
 * Copy element e in bvlogic buffer b
 * Raise an exception if e is not a bitvector
 */
void bvl_set_elem(tstack_t *stack, bvlogic_buffer_t *b, stack_elem_t *e) {
  term_t t;

  switch (e->tag) {
  case TAG_BV64:
    bvlogic_buffer_set_constant64(b, e->val.bv64.bitsize, e->val.bv64.value);
    break;

  case TAG_BV:
    bvlogic_buffer_set_constant(b, e->val.bv.bitsize, e->val.bv.data);
    break;

  case TAG_TERM:
    t = e->val.term;
    if (! yices_check_bv_term(t)) { // not a bitvector
      report_yices_error(stack);
    }
    bvlogic_buffer_set_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVARITH64_BUFFER:
    t = bvarith64_buffer_get_term(e->val.bvarith64_buffer);
    bvlogic_buffer_set_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVARITH_BUFFER:
    t = bvarith_buffer_get_term(e->val.bvarith_buffer);
    bvlogic_buffer_set_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVLOGIC_BUFFER:
    bvlogic_buffer_set_buffer(b, e->val.bvlogic_buffer);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVLOGIC_ERROR);
    break;
  }
}



/*
 * Copy element segment [i .. j] of e into bvlogic buffer b
 * Raise an exception if e is not a bitvector.
 * - i and j must be valid (i.e., 0 <= i <= j < e's bitsize)
 */
void bvl_set_slice_elem(tstack_t *stack, bvlogic_buffer_t *b, uint32_t i, uint32_t j, stack_elem_t *e) {
  term_t t;

  assert(i <= j);

  switch (e->tag) {
  case TAG_BV64:
    assert(j < e->val.bv64.bitsize);
    bvlogic_buffer_set_slice_constant64(b, i, j, e->val.bv64.value);
    break;

  case TAG_BV:
    assert(j < e->val.bv.bitsize);
    bvlogic_buffer_set_slice_constant(b, i, j, e->val.bv.data);
    break;

  case TAG_TERM:
    t = e->val.term;
    if (! yices_check_bv_term(t)) { // not a bitvector
      report_yices_error(stack);
    }
    bvlogic_buffer_set_slice_term(b, __yices_globals.terms, i, j, t);
    break;

  case TAG_BVARITH64_BUFFER:
    t = bvarith64_buffer_get_term(e->val.bvarith64_buffer);
    bvlogic_buffer_set_slice_term(b, __yices_globals.terms, i, j, t);
    break;

  case TAG_BVARITH_BUFFER:
    t = bvarith_buffer_get_term(e->val.bvarith_buffer);
    bvlogic_buffer_set_slice_term(b, __yices_globals.terms, i, j, t);
    break;

  case TAG_BVLOGIC_BUFFER:
    bvlogic_buffer_set_slice_buffer(b, i, j, e->val.bvlogic_buffer);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVLOGIC_ERROR);
    break;
  }
}



/*
 * Check whether e is a bitvector constant
 */
bool elem_is_bvconst(stack_elem_t *e) {
  term_kind_t k;

  switch (e->tag) {
  case TAG_BV64:
  case TAG_BV:
    return true;

  case TAG_TERM:
    k = term_kind(__yices_globals.terms, e->val.term);
    return k == BV64_CONSTANT || k == BV_CONSTANT;

  case TAG_BVARITH64_BUFFER:
    bvarith64_buffer_normalize(e->val.bvarith64_buffer);
    return bvarith64_buffer_is_constant(e->val.bvarith64_buffer);

  case TAG_BVARITH_BUFFER:
    bvarith_buffer_normalize(e->val.bvarith_buffer);
    return bvarith_buffer_is_constant(e->val.bvarith_buffer);

  case TAG_BVLOGIC_BUFFER:
    return bvlogic_buffer_is_constant(e->val.bvlogic_buffer);

  default:
    return false;
  }
}


/*
 * Copy term t into c:
 * - t must be a BV_CONSTANT or BV64_CONSTANT
 */
static void bvconstant_copy_term(bvconstant_t *c, term_t t) {
  term_table_t *terms;
  bvconst_term_t *d;
  bvconst64_term_t *d64;

  terms = __yices_globals.terms;
  if (term_kind(terms, t) == BV64_CONSTANT) {
    d64 = bvconst64_term_desc(terms, t);
    bvconstant_copy64(c, d64->bitsize, d64->value);
  } else {
    d = bvconst_term_desc(terms, t);
    bvconstant_copy(c, d->bitsize, d->data);
  }
}


/*
 * Copy the constant value of e into c
 * - e must satisfy elem_is_bvconst(e)
 */
void bvconst_set_elem(bvconstant_t *c, stack_elem_t *e) {
  assert(elem_is_bvconst(e));

  switch (e->tag) {
  case TAG_BV64:
    bvconstant_copy64(c, e->val.bv64.bitsize, e->val.bv64.value);
    break;

  case TAG_BV:
    bvconstant_copy(c, e->val.bv.bitsize, e->val.bv.data);
    break;

  case TAG_TERM:
    bvconstant_copy_term(c, e->val.term);
    break;

  case TAG_BVARITH_BUFFER:
    bvarith_buffer_copy_constant(e->val.bvarith_buffer, c);
    break;

  case TAG_BVLOGIC_BUFFER:
    bvlogic_buffer_get_constant(e->val.bvlogic_buffer, c);
    break;

  default: // should not happen
    assert(false);
    break;
  }
}


/*
 * Bitwise operations between buffer b and element e
 */
void bvand_elem(tstack_t *stack, bvlogic_buffer_t *b, stack_elem_t *e) {
  uint32_t n;
  term_t t;

  n = bvlogic_buffer_bitsize(b);

  switch (e->tag) {
  case TAG_BV64:
    if (e->val.bv64.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_and_constant64(b, e->val.bv64.bitsize, e->val.bv64.value);
    break;

  case TAG_BV:
    if (e->val.bv.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_and_constant(b, e->val.bv.bitsize, e->val.bv.data);
    break;

  case TAG_TERM:
    check_bv_term(stack, e, n);
    bvlogic_buffer_and_term(b, __yices_globals.terms, e->val.term);
    break;

  case TAG_BVARITH64_BUFFER:
    if (bvarith64_buffer_bitsize(e->val.bvarith64_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    t = bvarith64_buffer_get_term(e->val.bvarith64_buffer);
    bvlogic_buffer_and_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVARITH_BUFFER:
    if (bvarith_buffer_bitsize(e->val.bvarith_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    t = bvarith_buffer_get_term(e->val.bvarith_buffer);
    bvlogic_buffer_and_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVLOGIC_BUFFER:
    if (bvlogic_buffer_bitsize(e->val.bvlogic_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_and_buffer(b, e->val.bvlogic_buffer);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVLOGIC_ERROR);
  }
}

void bvor_elem(tstack_t *stack, bvlogic_buffer_t *b, stack_elem_t *e) {
  uint32_t n;
  term_t t;

  n = bvlogic_buffer_bitsize(b);

  switch (e->tag) {
  case TAG_BV64:
    if (e->val.bv64.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_or_constant64(b, e->val.bv64.bitsize, e->val.bv64.value);
    break;

  case TAG_BV:
    if (e->val.bv.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_or_constant(b, e->val.bv.bitsize, e->val.bv.data);
    break;

  case TAG_TERM:
    check_bv_term(stack, e, n);
    bvlogic_buffer_or_term(b, __yices_globals.terms, e->val.term);
    break;

  case TAG_BVARITH64_BUFFER:
    if (bvarith64_buffer_bitsize(e->val.bvarith64_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    t = bvarith64_buffer_get_term(e->val.bvarith64_buffer);
    bvlogic_buffer_or_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVARITH_BUFFER:
    if (bvarith_buffer_bitsize(e->val.bvarith_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    t = bvarith_buffer_get_term(e->val.bvarith_buffer);
    bvlogic_buffer_or_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVLOGIC_BUFFER:
    if (bvlogic_buffer_bitsize(e->val.bvlogic_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_or_buffer(b, e->val.bvlogic_buffer);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVLOGIC_ERROR);
  }
}

void bvxor_elem(tstack_t *stack, bvlogic_buffer_t *b, stack_elem_t *e) {
  uint32_t n;
  term_t t;

  n = bvlogic_buffer_bitsize(b);

  switch (e->tag) {
  case TAG_BV64:
    if (e->val.bv64.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_xor_constant64(b, e->val.bv64.bitsize, e->val.bv64.value);
    break;

  case TAG_BV:
    if (e->val.bv.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_xor_constant(b, e->val.bv.bitsize, e->val.bv.data);
    break;

  case TAG_TERM:
    check_bv_term(stack, e, n);
    bvlogic_buffer_xor_term(b, __yices_globals.terms, e->val.term);
    break;

  case TAG_BVARITH64_BUFFER:
    if (bvarith64_buffer_bitsize(e->val.bvarith64_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    t = bvarith64_buffer_get_term(e->val.bvarith64_buffer);
    bvlogic_buffer_xor_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVARITH_BUFFER:
    if (bvarith_buffer_bitsize(e->val.bvarith_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    t = bvarith_buffer_get_term(e->val.bvarith_buffer);
    bvlogic_buffer_xor_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVLOGIC_BUFFER:
    if (bvlogic_buffer_bitsize(e->val.bvlogic_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_xor_buffer(b, e->val.bvlogic_buffer);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVLOGIC_ERROR);
    break;
  }
}

void bvcomp_elem(tstack_t *stack, bvlogic_buffer_t *b, stack_elem_t *e) {
  uint32_t n;
  term_t t;

  n = bvlogic_buffer_bitsize(b);

  switch (e->tag) {
  case TAG_BV64:
    if (e->val.bv64.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_comp_constant64(b, e->val.bv64.bitsize, e->val.bv64.value);
    break;

  case TAG_BV:
    if (e->val.bv.bitsize != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_comp_constant(b, e->val.bv.bitsize, e->val.bv.data);
    break;

  case TAG_TERM:
    check_bv_term(stack, e, n);
    bvlogic_buffer_comp_term(b, __yices_globals.terms, e->val.term);
    break;

  case TAG_BVARITH64_BUFFER:
    if (bvarith64_buffer_bitsize(e->val.bvarith64_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    t = bvarith64_buffer_get_term(e->val.bvarith64_buffer);
    bvlogic_buffer_comp_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVARITH_BUFFER:
    if (bvarith_buffer_bitsize(e->val.bvarith_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    t = bvarith_buffer_get_term(e->val.bvarith_buffer);
    bvlogic_buffer_comp_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVLOGIC_BUFFER:
    if (bvlogic_buffer_bitsize(e->val.bvlogic_buffer) != n) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_comp_buffer(b, e->val.bvlogic_buffer);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVLOGIC_ERROR);
    break;
  }
}


// add e to the right of b (i.e., high-order bits are from b, low-order bits from e)
void bvconcat_elem(tstack_t *stack, bvlogic_buffer_t *b, stack_elem_t *e) {
  term_t t;

  switch (e->tag) {
  case TAG_BV64:
    bvlogic_buffer_concat_right_constant64(b, e->val.bv64.bitsize, e->val.bv64.value);
    break;

  case TAG_BV:
    bvlogic_buffer_concat_right_constant(b, e->val.bv.bitsize, e->val.bv.data);
    break;

  case TAG_TERM:
    t = e->val.term;
    if (! yices_check_bv_term(t)) {
      report_yices_error(stack);
    }
    bvlogic_buffer_concat_right_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVARITH64_BUFFER:
    t = bvarith64_buffer_get_term(e->val.bvarith64_buffer);
    bvlogic_buffer_concat_right_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVARITH_BUFFER:
    t = bvarith_buffer_get_term(e->val.bvarith_buffer);
    bvlogic_buffer_concat_right_term(b, __yices_globals.terms, t);
    break;

  case TAG_BVLOGIC_BUFFER:
    bvlogic_buffer_concat_right_buffer(b, e->val.bvlogic_buffer);
    break;

  default:
    raise_exception(stack, e, TSTACK_BVLOGIC_ERROR);
    break;
  }
}





/*
 * Every eval function takes three parameters
 * - the stack
 * - f = an array of stack elements.
 * - n = size of this array
 *
 * f is set to the start of the arguments on the top frame,
 * n = the number of arguments
 *
 * For example, if the stack contains a frame with operator code MK_AND
 * and 4 arguments, then the top frame is [MK_AND <arg1> ... <arg4>]
 *
 * tstack_eval will invoke eval_mk_and(stack, f, n)
 * with f pointing to array [<arg1> .... <arg4>] and n = 4
 *
 * With every eval function, there's a check function that takes the
 * same parameters and raises an exception if the arguments or frame
 * are incorrect.
 */


/*
 * COMMANDS MAPPING NAMES TO TYPES OR TERMS
 */

/*
 * [define-type <symbol> <type> ]
 */
static void check_define_type(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, DEFINE_TYPE);
  check_size(stack, n == 2);
  check_tag(stack, f, TAG_SYMBOL);
  if (n == 2) check_tag(stack, f+1, TAG_TYPE);
}

static void eval_define_type(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  type_t tau;

  tau = f[1].val.type;
  yices_set_type_name(tau, f[0].val.string);

  tstack_pop_frame(stack);
  no_result(stack);
}

/*
 * [define-term <symbol> <type> ]
 * [define-term <symbol> <type> <term> ]
 */
static void check_define_term(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, DEFINE_TERM);
  check_size(stack, (n == 2 || n == 3));
  check_tag(stack, f, TAG_SYMBOL);
  check_tag(stack, f+1, TAG_TYPE);
  // no need to check val[f+2]: get_term will raise an exception if
  // it can't be converted to a term.
}

static void eval_define_term(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  type_t tau;
  term_t t;

  tau = f[1].val.type;
  if (n == 2) {
    t = yices_new_uninterpreted_term(tau);
  } else {
    t = get_term(stack, f+2);
    if (! is_subtype(__yices_globals.types, term_type(__yices_globals.terms, t), tau)) {
      raise_exception(stack, f+2, TSTACK_TYPE_ERROR_IN_DEFTERM);
    }
  }
  yices_set_term_name(t, f[0].val.string);

  tstack_pop_frame(stack);
  no_result(stack);
}


/*
 * [bind <string> <term>]
 */
static void check_bind(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, BIND);
  check_size(stack, n == 2);
  check_tag(stack, f, TAG_SYMBOL);
}

static void eval_bind(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t;
  char *name;

  name = f[0].val.string;
  t = get_term(stack, f+1);
  yices_set_term_name(t, name);
  tstack_pop_frame(stack);
  set_binding_result(stack, t, name);
}


/*
 * [let <binding> ... <binding> <term>]
 */
static void check_let(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, LET);
  check_size(stack, n>=2);
  check_all_tags(stack, f, f + (n-1), TAG_BINDING);
}

static void eval_let(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  copy_result_and_pop_frame(stack, f + (n-1));
}


/*
 * TYPE CONSTRUCTORS
 */

/*
 * [mk-bv-type <rational>]
 */
static void check_mk_bv_type(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_TYPE);
  check_size(stack, n == 1);
  check_tag(stack, f, TAG_RATIONAL);
}

static void eval_mk_bv_type(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  int32_t size;
  type_t tau;

  size = get_integer(stack, f);
  if (size <= 0) {
    raise_exception(stack, f, TSTACK_NONPOSITIVE_BVSIZE);
  }
  tau = yices_bv_type(size);
  check_type(stack, tau);

  tstack_pop_frame(stack);
  set_type_result(stack, tau);
}


/*
 * TERM CONSTRUCTORS
 */

/*
 * [mk-ite <term> <term> <term>]
 */
static void check_mk_ite(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_ITE);
  check_size(stack, n == 3);
}

static void eval_mk_ite(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t cond, left, right, t;

  cond = get_term(stack, f);
  left = get_term(stack, f+1);
  right = get_term(stack, f+2);
  t = yices_ite(cond, left, right);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-eq <term> <term>]
 */
static void check_mk_eq(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_EQ);
  check_size(stack, n == 2);
}

static void eval_mk_eq(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t left, right, t;

  left = get_term(stack, f);
  right = get_term(stack, f+1);
  t = yices_eq(left, right);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-diseq <term> <term>]
 */
static void check_mk_diseq(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_DISEQ);
  check_size(stack, n == 2);
}

static void eval_mk_diseq(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t left, right, t;

  left = get_term(stack, f);
  right = get_term(stack, f+1);
  t = yices_neq(left, right);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-distinct <term> ... <term>]
 */
static void check_mk_distinct(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_DISTINCT);
  check_size(stack, n >= 2);
}

static void eval_mk_distinct(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t *arg, t;
  uint32_t i;

  arg = get_aux_buffer(stack, n);

  for (i=0; i<n; i++) {
    arg[i] = get_term(stack, f+i);
  }
  t = yices_distinct(n, arg);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}

/*
 * [mk-not <term>]
 */
static void check_mk_not(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_NOT);
  check_size(stack, n == 1);
}

static void eval_mk_not(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t;

  t = yices_not(get_term(stack, f));
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-or <term> ... <term>]
 */
static void check_mk_or(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_OR);
  check_size(stack, n >= 1);
}

// use buffer here rather than
static void eval_mk_or(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t *arg, t;
  uint32_t i;

  arg = get_aux_buffer(stack, n);

  for (i=0; i<n; i++) {
    arg[i] = get_term(stack, f+i);
  }
  t = yices_or(n, arg);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-and <term> ... <term>]
 */
static void check_mk_and(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_AND);
  check_size(stack, n >= 1);
}

static void eval_mk_and(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t *arg, t;
  uint32_t i;

  arg = get_aux_buffer(stack, n);

  for (i=0; i<n; i++) {
    arg[i] = get_term(stack, f+i);
  }
  t = yices_and(n, arg);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-xor <term> ... <term>]
 */
static void check_mk_xor(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_XOR);
  check_size(stack, n >= 1);
}

static void eval_mk_xor(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t *arg, t;
  uint32_t i;

  arg = get_aux_buffer(stack, n);

  for (i=0; i<n; i++) {
    arg[i] = get_term(stack, f+i);
  }
  t = yices_xor(n, arg);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-iff <term> ... <term>]
 */
static void check_mk_iff(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_IFF);
  check_size(stack, n >= 1);
}

static void eval_mk_iff(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t;
  uint32_t i;

  t = get_term(stack, f);
  for (i=1; i<n; i++) {
    t = yices_iff(t, get_term(stack, f+i));
    check_term(stack, t);
  }
  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-implies <term> <term>]
 */
static void check_mk_implies(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_IMPLIES);
  check_size(stack, n == 2);
}

static void eval_mk_implies(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t left, right, t;

  left = get_term(stack, f);
  right = get_term(stack, f+1);
  t = yices_implies(left, right);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * BITVECTOR ARITHMETIC
 */

/*
 * Build bitvector constant defined by val and size:
 * - size = number of bits
 * - val = value
 * - f = frame index: it's used for error reporting only
 * Raise an exception if size <= 0 or size > MAX_BV_SIZE or val is not a non-negative
 * integer
 *
 * Warning: val is a pointer inside a stack element that is reset by q_clear on pop_frame.
 * So we must make a copy before calling pop_frame
 */
void mk_bv_const_core(tstack_t *stack, stack_elem_t *f, int32_t size, rational_t *val) {
  uint32_t k;
  uint32_t *tmp;
  uint64_t c;

  if (size <= 0) {
    raise_exception(stack, f, TSTACK_NONPOSITIVE_BVSIZE);
  }

  if (! yices_check_bvsize((uint32_t) size)) {
    report_yices_error(stack);
  }

  if (! q_is_integer(val) || ! q_is_nonneg(val)) {
    raise_exception(stack, f, TSTACK_INVALID_BVCONSTANT);
  }

  if (size <= 64) {
    c =  bvconst64_from_q(size, val);
    assert(c == norm64(c, size));

    tstack_pop_frame(stack);
    set_bv64_result(stack, size, c);

  } else {
    k = (size + 31) >> 5;
    tmp = bvconst_alloc(k);
    bvconst_set_q(tmp, k, val);
    bvconst_normalize(tmp, size);

    tstack_pop_frame(stack);
    set_bv_result(stack, size, tmp);
  }
}


/*
 * Sign-extend: bv = bitvector, idx = number of bits to add
 */
void mk_bv_sign_extend_core(tstack_t *stack, stack_elem_t *bv, stack_elem_t *idx) {
  int32_t i;
  bvlogic_buffer_t *b;

  i = get_integer(stack, idx);
  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, bv);
  if (! yices_check_bvextend(b, i)) {
    report_yices_error(stack);
  }
  bvlogic_buffer_sign_extend(b, i + bvlogic_buffer_bitsize(b));

  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * Zero-extend: bv, idx as for sign-extend
 */
void mk_bv_zero_extend_core(tstack_t *stack, stack_elem_t *bv, stack_elem_t *idx) {
  int32_t i;
  bvlogic_buffer_t *b;

  i = get_integer(stack, idx);
  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, bv);
  if (! yices_check_bvextend(b, i)) {
    report_yices_error(stack);
  }
  bvlogic_buffer_zero_extend(b, i + bvlogic_buffer_bitsize(b));

  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}




/*
 * [mk-bv-const <size> <value>]
 */
static void check_mk_bv_const(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_CONST);
  check_size(stack, n == 2);
  check_tag(stack, f, TAG_RATIONAL);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_const(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  int32_t size;
  rational_t *val;

  size = get_integer(stack, f);
  val = &f[1].val.rational;
  mk_bv_const_core(stack, f, size, val);
}


/*
 * [mk-bv-add <bv> ... <bv>]
 */
static void check_mk_bv_add(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_ADD);
  check_size(stack, n >= 1);
}

static void eval_mk_bv_add(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  uint32_t i, bitsize;
  bvarith_buffer_t *b;
  bvarith64_buffer_t *b64;

  bitsize = elem_bitsize(stack, f);
  if (bitsize <= 64) {
    b64 = tstack_get_bva64buffer(stack, bitsize);
    for (i=0; i<n; i++) {
      bva64_add_elem(stack, b64, f+i);
    }
    tstack_pop_frame(stack);
    set_bvarith64_result(stack, b64);

  } else {
    b = tstack_get_bvabuffer(stack, bitsize);
    for (i=0; i<n; i++) {
      bva_add_elem(stack, b, f+i);
    }
    tstack_pop_frame(stack);
    set_bvarith_result(stack, b);
  }
}


/*
 * [mk-bv-sub <bv> ... <bv>]
 */
static void check_mk_bv_sub(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SUB);
  check_size(stack, n >= 2);
}

static void eval_mk_bv_sub(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  uint32_t i, bitsize;
  bvarith_buffer_t *b;
  bvarith64_buffer_t *b64;

  bitsize = elem_bitsize(stack, f);
  if (bitsize <= 64) {
    b64 = tstack_get_bva64buffer(stack, bitsize);
    bva64_add_elem(stack, b64, f);
    for (i=1; i<n; i++) {
      bva64_sub_elem(stack, b64, f+i);
    }

    tstack_pop_frame(stack);
    set_bvarith64_result(stack, b64);

  } else {
    b = tstack_get_bvabuffer(stack, bitsize);
    bva_add_elem(stack, b, f);
    for (i=1; i<n; i++) {
      bva_sub_elem(stack, b, f+i);
    }

    tstack_pop_frame(stack);
    set_bvarith_result(stack, b);
  }
}


/*
 * [mk-bv-mul <bv> ... <bv>]
 */
static void check_mk_bv_mul(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_MUL);
  check_size(stack, n >= 1);
}

static void eval_mk_bv_mul(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  uint32_t i, bitsize;
  bvarith_buffer_t *b;
  bvarith64_buffer_t *b64;

  bitsize = elem_bitsize(stack, f);
  if (bitsize <= 64) {
    b64 = tstack_get_bva64buffer(stack, bitsize);
    bva64_add_elem(stack, b64, f);
    for (i=1; i<n; i++) {
      bva64_mul_elem(stack, b64, f+i);
    }

    tstack_pop_frame(stack);
    set_bvarith64_result(stack, b64);

  } else {
    b = tstack_get_bvabuffer(stack, bitsize);
    bva_add_elem(stack, b, f);
    for (i=1; i<n; i++) {
      bva_mul_elem(stack, b, f+i);
    }

    tstack_pop_frame(stack);
    set_bvarith_result(stack, b);
  }
}


/*
 * [mk-bv-neg <bv>]
 */
static void check_mk_bv_neg(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_NEG);
  check_size(stack, n == 1);
}

static void eval_mk_bv_neg(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvneg_elem(stack, f);
  copy_result_and_pop_frame(stack, f);
}


/*
 * [mk-bv-pow <bv> <integer]
 */
static void check_mk_bv_pow(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_POW);
  check_size(stack, n == 2);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_pow(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  int32_t exponent;
  term_t t;

  exponent = get_integer(stack, f+1);
  if (exponent < 0) {
    raise_exception(stack, f+1, TSTACK_NEGATIVE_EXPONENT);
  }

  /*
   * Note: we could avoid creating the intermediate term t
   * by calling directly the exponentiation functions for
   * arith_buffers, rationals, etc.?
   */
  t = get_term(stack, f);
  t = yices_bvpower(t, exponent);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * BITVECTOR LOGIC
 */

/*
 * [mk-bv-not <bv>]
 */
static void check_mk_bv_not(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_NOT);
  check_size(stack, n == 1);
}

static void eval_mk_bv_not(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;

  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  bvlogic_buffer_not(b);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}



/*
 * [mk-bv-and <bv> ... <bv>]
 */
static void check_mk_bv_and(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_AND);
  check_size(stack, n >= 1);
}

static void eval_mk_bv_and(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  uint32_t i;

  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  for (i=1; i<n; i++) {
    bvand_elem(stack, b, f+i);
  }
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-or <bv> ... <bv>]
 */
static void check_mk_bv_or(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_OR);
  check_size(stack, n >= 1);
}

static void eval_mk_bv_or(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  uint32_t i;

  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  for (i=1; i<n; i++) {
    bvor_elem(stack, b, f+i);
  }
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-xor <bv> ... <bv>]
 */
static void check_mk_bv_xor(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_XOR);
  check_size(stack, n >= 1);
}

static void eval_mk_bv_xor(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  uint32_t i;

  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  for (i=1; i<n; i++) {
    bvxor_elem(stack, b, f+i);
  }
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-nand <bv> ... <bv>]
 */
static void check_mk_bv_nand(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_NAND);
  check_size(stack, n >= 1);
}

static void eval_mk_bv_nand(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  uint32_t i;

  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  for (i=1; i<n; i++) {
    bvand_elem(stack, b, f+i);
  }
  bvlogic_buffer_not(b);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-nor <bv> ... <bv>]
 */
static void check_mk_bv_nor(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_NOR);
  check_size(stack, n >= 1);
}

static void eval_mk_bv_nor(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  uint32_t i;

  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  for (i=1; i<n; i++) {
    bvor_elem(stack, b, f+i);
  }
  bvlogic_buffer_not(b);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-xnor <bv> ... <bv>]
 */
static void check_mk_bv_xnor(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_XNOR);
  check_size(stack, n >= 1);
}

static void eval_mk_bv_xnor(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  uint32_t i;

  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  for (i=1; i<n; i++) {
    bvxor_elem(stack, b, f+i);
  }
  bvlogic_buffer_not(b);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-shift-left0 <bv> <rational>]
 */
static void check_mk_bv_shift_left0(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SHIFT_LEFT0);
  check_size(stack, n == 2);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_shift_left0(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  int32_t index;
  bvlogic_buffer_t *b;

  index = get_integer(stack, f+1);
  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  if (! yices_check_bitshift(b, index)) {
    report_yices_error(stack);
  }
  bvlogic_buffer_shift_left0(b, index);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-shift-left1 <bv> <rational>]
 */
static void check_mk_bv_shift_left1(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SHIFT_LEFT1);
  check_size(stack, n == 2);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_shift_left1(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  int32_t index;
  bvlogic_buffer_t *b;

  index = get_integer(stack, f+1);
  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  if (! yices_check_bitshift(b, index)) {
    report_yices_error(stack);
  }
  bvlogic_buffer_shift_left1(b, index);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-shift-right0 <bv> <rational>]
 */
static void check_mk_bv_shift_right0(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SHIFT_RIGHT0);
  check_size(stack, n == 2);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_shift_right0(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  int32_t index;
  bvlogic_buffer_t *b;

  index = get_integer(stack, f+1);
  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  if (! yices_check_bitshift(b, index)) {
    report_yices_error(stack);
  }
  bvlogic_buffer_shift_right0(b, index);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-shift-right1 <bv> <rational>]
 */
static void check_mk_bv_shift_right1(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SHIFT_RIGHT1);
  check_size(stack, n == 2);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_shift_right1(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  int32_t index;
  bvlogic_buffer_t *b;

  index = get_integer(stack, f+1);
  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  if (! yices_check_bitshift(b, index)) {
    report_yices_error(stack);
  }
  bvlogic_buffer_shift_right1(b, index);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-ashift-right <bv> <rational>]
 */
static void check_mk_bv_ashift_right(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_ASHIFT_RIGHT);
  check_size(stack, n == 2);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_ashift_right(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  int32_t index;
  bvlogic_buffer_t *b;

  index = get_integer(stack, f+1);
  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  if (! yices_check_bitshift(b, index)) {
    report_yices_error(stack);
  }
  bvlogic_buffer_ashift_right(b, index);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-rotate-left <bv> <rational>]
 */
static void check_mk_bv_rotate_left(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_ROTATE_LEFT);
  check_size(stack, n == 2);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_rotate_left(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  int32_t index;
  bvlogic_buffer_t *b;

  index = get_integer(stack, f+1);
  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  if (! yices_check_bitshift(b, index)) {
    report_yices_error(stack);
  }
  // we known 0 <= index <= bitsize of b
  if (index < bvlogic_buffer_bitsize(b)) {
    bvlogic_buffer_rotate_left(b, index);
  }
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}

/*
 * [mk-bv-rotate-right <bv> <rational>]
 */
static void check_mk_bv_rotate_right(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_ROTATE_RIGHT);
  check_size(stack, n == 2);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_rotate_right(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  int32_t index;
  bvlogic_buffer_t *b;

  index = get_integer(stack, f+1);
  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  if (! yices_check_bitshift(b, index)) {
    report_yices_error(stack);
  }
  // we known 0 <= index <= bitsize of b
  if (index < bvlogic_buffer_bitsize(b)) {
    bvlogic_buffer_rotate_right(b, index);
  }
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-extract <rational> <rational> <bv>]
 */
static void check_mk_bv_extract(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_EXTRACT);
  check_size(stack, n == 3);
  check_tag(stack, f, TAG_RATIONAL);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_extract(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  int32_t i, j;
  uint32_t size;

  // the syntax is (bv-extract end begin bv)
  i = get_integer(stack, f);        // end index
  j = get_integer(stack, f+1);      // start index
  size = elem_bitsize(stack, f+2);  // vector
  if (! yices_check_bvextract(size, j, i)) {
    report_yices_error(stack);
  }

  if (i == 0 && j == size-1) {
    // (bv-extract 0 size-1 bv) = bv
    copy_result_and_pop_frame(stack, f+2);
  } else {
    // copy slice [i .. j] into b
    b = tstack_get_bvlbuffer(stack);
    bvl_set_slice_elem(stack, b, j, i, f+2);
    tstack_pop_frame(stack);
    set_bvlogic_result(stack, b);
  }
}


/*
 * [mk-bv-concat <bv> ... <bv>]
 */
static void check_mk_bv_concat(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_CONCAT);
  check_size(stack, n >= 1);
}

static void eval_mk_bv_concat(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  uint32_t i;

  b = tstack_get_bvlbuffer(stack);
  for (i=0; i<n; i++) {
    bvconcat_elem(stack, b, f+i);
  }
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-repeat <bv> <rational>]
 */
static void check_mk_bv_repeat(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_REPEAT);
  check_size(stack, n == 2);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_repeat(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  int32_t i;

  i = get_integer(stack, f+1);
  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  // check for overflow or for i <= 0
  if (! yices_check_bvrepeat(b, i)) {
    report_yices_error(stack);
  }
  bvlogic_buffer_repeat_concat(b, i);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}




/*
 * [mk-bv-sign-extend <bv> <rational>]
 * rational n = number of bits to add
 */
static void check_mk_bv_sign_extend(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SIGN_EXTEND);
  check_size(stack, n == 2);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_sign_extend(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  mk_bv_sign_extend_core(stack, f, f+1);
}



/*
 * [mk-bv-zero-extend <bv> <rational>]
 * rational n = number of bits to add
 */
static void check_mk_bv_zero_extend(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_ZERO_EXTEND);
  check_size(stack, n == 2);
  check_tag(stack, f+1, TAG_RATIONAL);
}

static void eval_mk_bv_zero_extend(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  mk_bv_zero_extend_core(stack, f, f+1);
}




/*
 * BITVECTOR ATOMS
 */

/*
 * [mk-bv-ge <bv> <bv>]
 */
static void check_mk_bv_ge(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_GE);
  check_size(stack, n == 2);
}

static void eval_mk_bv_ge(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvge_atom(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-bv-gt <bv> <bv>]
 */
static void check_mk_bv_gt(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_GT);
  check_size(stack, n == 2);
}

static void eval_mk_bv_gt(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvgt_atom(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-bv-le <bv> <bv>]
 */
static void check_mk_bv_le(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_LE);
  check_size(stack, n == 2);
}

static void eval_mk_bv_le(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvle_atom(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-bv-lt <bv> <bv>]
 */
static void check_mk_bv_lt(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_LT);
  check_size(stack, n == 2);
}

static void eval_mk_bv_lt(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvlt_atom(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-bv-sge <bv> <bv>]
 */
static void check_mk_bv_sge(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SGE);
  check_size(stack, n == 2);
}

static void eval_mk_bv_sge(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvsge_atom(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-bv-sgt <bv> <bv>]
 */
static void check_mk_bv_sgt(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SGT);
  check_size(stack, n == 2);
}

static void eval_mk_bv_sgt(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvsgt_atom(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-bv-sle <bv> <bv>]
 */
static void check_mk_bv_sle(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SLE);
  check_size(stack, n == 2);
}

static void eval_mk_bv_sle(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvsle_atom(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-bv-slt <bv> <bv>]
 */
static void check_mk_bv_slt(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SLT);
  check_size(stack, n == 2);
}

static void eval_mk_bv_slt(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvslt_atom(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}




/*
 * NEW BITVECTOR OPERATORS IN SMT-LIB
 */

/*
 * Shift operators: [shift <bv1> <bv2>]
 * - bv1 and bv2 must be bitvectors of the same size
 * - shift bv1
 * - the number of bits to shift = value of vv2
 *
 * bv-shl: shift left padding with 0
 * bv-lshr: logical shift right (padding with 0)
 * bv-ashr: arithmetic shift (padding with sign bit)
 */

/*
 * [mk-bv-shl <bv1> <bv2>]
 */
static void check_mk_bv_shl(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SHL);
  check_size(stack, n == 2);
}

static void eval_mk_bv_shl(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  bvconstant_t *c;
  stack_elem_t *e;
  term_t t1, t2, t;

  e = f + 1;
  if (! elem_is_bvconst(e)) {
    // variable shift
    t1 = get_term(stack, f);
    t2 = get_term(stack, f+1);
    t = yices_bvshl(t1, t2);
    check_term(stack, t);

    tstack_pop_frame(stack);
    set_term_result(stack, t);

  } else {
    // constant shift amount

    b = tstack_get_bvlbuffer(stack);
    bvl_set_elem(stack, b, f);
    c = &stack->bvconst_buffer;
    bvconst_set_elem(c, e);

    if (c->bitsize != bvlogic_buffer_bitsize(b)) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_shl_constant(b, c->bitsize, c->data);
    tstack_pop_frame(stack);
    set_bvlogic_result(stack, b);
  }
}


/*
 * [mk-bv-lshr <bv1> <bv2>]
 */
static void check_mk_bv_lshr(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_LSHR);
  check_size(stack, n == 2);
}

static void eval_mk_bv_lshr(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  bvconstant_t *c;
  stack_elem_t *e;
  term_t t1, t2, t;

  e = f + 1;
  if (! elem_is_bvconst(e)) {
    // variable shift
    t1 = get_term(stack, f);
    t2 = get_term(stack, f+1);
    t = yices_bvlshr(t1, t2);
    check_term(stack, t);

    tstack_pop_frame(stack);
    set_term_result(stack, t);

  } else {
    // constant shift amount

    b = tstack_get_bvlbuffer(stack);
    bvl_set_elem(stack, b, f);
    c = &stack->bvconst_buffer;
    bvconst_set_elem(c, e);

    if (c->bitsize != bvlogic_buffer_bitsize(b)) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_lshr_constant(b, c->bitsize, c->data);
    tstack_pop_frame(stack);
    set_bvlogic_result(stack, b);
  }
}

/*
 * [mk-bv-ashr <bv1> <bv2>]
 */
static void check_mk_bv_ashr(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_ASHR);
  check_size(stack, n == 2);
}

static void eval_mk_bv_ashr(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;
  bvconstant_t *c;
  stack_elem_t *e;
  term_t t1, t2, t;

  e = f + 1;
  if (! elem_is_bvconst(e)) {
    // variable shift
    t1 = get_term(stack, f);
    t2 = get_term(stack, f+1);
    t = yices_bvashr(t1, t2);
    check_term(stack, t);

    tstack_pop_frame(stack);
    set_term_result(stack, t);

  } else {
    // constant shift amount

    b = tstack_get_bvlbuffer(stack);
    bvl_set_elem(stack, b, f);
    c = &stack->bvconst_buffer;
    bvconst_set_elem(c, e);

    if (c->bitsize != bvlogic_buffer_bitsize(b)) {
      raise_exception(stack, e, TSTACK_INCOMPATIBLE_BVSIZES);
    }
    bvlogic_buffer_ashr_constant(b, c->bitsize, c->data);
    tstack_pop_frame(stack);
    set_bvlogic_result(stack, b);
  }
}


/*
 * Unsigned division and remainder
 */

/*
 * [mk-bv-div <bv1> <bv2>]
 */
static void check_mk_bv_div(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_DIV);
  check_size(stack, n == 2);
}

static void eval_mk_bv_div(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvdiv(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}

/*
 * [mk-bv-rem <bv1> <bv2>]
 */
static void check_mk_bv_rem(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_REM);
  check_size(stack, n == 2);
}

static void eval_mk_bv_rem(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvrem(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}




/*
 * Signed division and remainder
 */

/*
 * [mk-bv-sdiv <bv1> <bv2>]
 */
static void check_mk_bv_sdiv(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SDIV);
  check_size(stack, n == 2);
}

static void eval_mk_bv_sdiv(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvsdiv(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}

/*
 * [mk-bv-srem <bv1> <bv2>]
 */
static void check_mk_bv_srem(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SREM);
  check_size(stack, n == 2);
}

static void eval_mk_bv_srem(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvsrem(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}


/*
 * [mk-bv-smod <bv1> <bv2>]
 */
static void check_mk_bv_smod(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_SMOD);
  check_size(stack, n == 2);
}

static void eval_mk_bv_smod(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t1, t2, t;

  t1 = get_term(stack, f);
  t2 = get_term(stack, f+1);
  t = yices_bvsmod(t1, t2);
  check_term(stack, t);

  tstack_pop_frame(stack);
  set_term_result(stack, t);
}



/*
 * Redor/redand/bvcomp
 */

/*
 * [mk-bv-redor <bv1>]
 */
static void check_mk_bv_redor(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_REDOR);
  check_size(stack, n == 1);
}

static void eval_mk_bv_redor(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;

  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  if (! yices_check_bvlogic_buffer(b)) {
    report_yices_error(stack); // empty buffer
  }
  bvlogic_buffer_redor(b);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-redand <bv1>]
 */
static void check_mk_bv_redand(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_REDAND);
  check_size(stack, n == 1);
}

static void eval_mk_bv_redand(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;

  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  if (! yices_check_bvlogic_buffer(b)) {
    report_yices_error(stack); // empty buffer
  }
  bvlogic_buffer_redand(b);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}


/*
 * [mk-bv-comp <bv1> <bv2>]
 */
static void check_mk_bv_comp(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, MK_BV_COMP);
  check_size(stack, n == 2);
}

static void eval_mk_bv_comp(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  bvlogic_buffer_t *b;

  b = tstack_get_bvlbuffer(stack);
  bvl_set_elem(stack, b, f);
  bvcomp_elem(stack, b, f+1);
  tstack_pop_frame(stack);
  set_bvlogic_result(stack, b);
}










/*
 * BUILD RESULT
 */

/*
 * [build-term <term>]
 */
static void check_build_term(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, BUILD_TERM);
  check_size(stack, n == 1);
}

static void eval_build_term(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  term_t t;

  t = get_term(stack, f);
  stack->result.term = t;
  tstack_pop_frame(stack);
  no_result(stack);
}

/*
 * [build-type <type>]
 */
static void check_build_type(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  check_op(stack, BUILD_TYPE);
  check_size(stack, n == 1);
  check_tag(stack, f, TAG_TYPE);
}

static void eval_build_type(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  stack->result.type = f[0].val.type;;
  tstack_pop_frame(stack);
  no_result(stack);
}



/*
 * Not supported or other error
 */
static void eval_error(tstack_t *stack, stack_elem_t *f, uint32_t n) {
  raise_exception(stack, f, TSTACK_INVALID_OP);
}


/*
 * Eval the top-level operation
 */
void tstack_eval(tstack_t *stack) {
  uint32_t n;
  int32_t op;
  stack_elem_t *f;

  n = stack->frame;
  f = stack->elem + n;

  if (f->val.opval.multiplicity > 0) {
    // decrement and do nothing. This is a special associative operator
    f->val.opval.multiplicity --;

  } else {
    // pass start frame and frame size to the check and eval function
    op = stack->top_op;
    assert(stack->top > stack->frame);
    n = stack->top - stack->frame - 1;
    f ++;

    stack->op_table.check[op](stack, f, n);
    stack->op_table.eval[op](stack, f, n);
  }
}



/*
 * Direct calls of check[op] and eval[op]
 */
void call_tstack_check(tstack_t *stack, int32_t op, stack_elem_t *f, uint32_t n) {
  int32_t saved_op;

  assert(0 <= op && op < stack->op_table.num_ops && stack->op_table.check[op] != NULL);

  /*
   * we save top_op and replace it by op to prevent 'bad_opcode exception'
   * raised by 'check[op]
   */
  saved_op = stack->top_op;
  stack->top_op = op;
  stack->op_table.check[op](stack, f, n);
  stack->top_op = saved_op;
}

void call_tstack_eval(tstack_t *stack, int32_t op, stack_elem_t *f, uint32_t n) {
  assert(0 <= op && op < stack->op_table.num_ops && stack->op_table.eval[op] != NULL);
  stack->top_op = op;
  stack->op_table.eval[op](stack, f, n);
}




/*
 * PREDEFINED OPERATION TABLE
 */
static const uint8_t assoc[NUM_BASE_OPCODES] = {
  0, // NO_OP
  0, // DEFINE_TYPE
  0, // DEFINE_TERM
  0, // BIND
  1, // LET
  0, // MK_BV_TYPE
  0, // MK_ITE
  0, // MK_EQ
  0, // MK_DISEQ
  0, // MK_DISTINCT
  0, // MK_NOT
  1, // MK_OR
  1, // MK_AND
  1, // MK_XOR
  0, // MK_IFF
  0, // MK_IMPLIES
  0, // MK_BV_CONST
  1, // MK_BV_ADD
  0, // MK_BV_SUB
  1, // MK_BV_MUL
  0, // MK_BV_NEG
  0, // MK_BV_POW
  0, // MK_BV_DIV
  0, // MK_BV_REM
  0, // MK_BV_SDIV
  0, // MK_BV_SREM
  0, // MK_BV_SMOD
  0, // MK_BV_NOT
  1, // MK_BV_AND
  1, // MK_BV_OR
  1, // MK_BV_XOR
  1, // MK_BV_NAND
  1, // MK_BV_NOR
  1, // MK_BV_XNOR
  0, // MK_BV_SHIFT_LEFT0
  0, // MK_BV_SHIFT_LEFT1
  0, // MK_BV_SHIFT_RIGHT0
  0, // MK_BV_SHIFT_RIGHT1
  0, // MK_BV_ASHIFT_RIGHT
  0, // MK_BV_ROTATE_LEFT
  0, // MK_BV_ROTATE_RIGHT
  0, // MK_BV_SHL
  0, // MK_BV_LSHR
  0, // MK_BV_ASHR
  0, // MK_BV_EXTRACT
  1, // MK_BV_CONCAT
  0, // MK_BV_REPEAT
  0, // MK_BV_SIGN_EXTEND
  0, // MK_BV_ZERO_EXTEND
  0, // MK_BV_REDAND
  0, // MK_BV_REDOR
  0, // MK_BV_COMP
  0, // MK_BV_GE
  0, // MK_BV_GT
  0, // MK_BV_LE
  0, // MK_BV_LT
  0, // MK_BV_SGE
  0, // MK_BV_SGT
  0, // MK_BV_SLE
  0, // MK_BV_SLT
  0, // BUILD_TERM
  0, // BUILD_TYPE
};

static const check_fun_t check[NUM_BASE_OPCODES] = {
  eval_error, // NO_OP
  check_define_type,
  check_define_term,
  check_bind,
  check_let,
  check_mk_bv_type,
  check_mk_ite,
  check_mk_eq,
  check_mk_diseq,
  check_mk_distinct,
  check_mk_not,
  check_mk_or,
  check_mk_and,
  check_mk_xor,
  check_mk_iff,
  check_mk_implies,
  check_mk_bv_const,
  check_mk_bv_add,
  check_mk_bv_sub,
  check_mk_bv_mul,
  check_mk_bv_neg,
  check_mk_bv_pow,
  check_mk_bv_div,
  check_mk_bv_rem,
  check_mk_bv_sdiv,
  check_mk_bv_srem,
  check_mk_bv_smod,
  check_mk_bv_not,
  check_mk_bv_and,
  check_mk_bv_or,
  check_mk_bv_xor,
  check_mk_bv_nand,
  check_mk_bv_nor,
  check_mk_bv_xnor,
  check_mk_bv_shift_left0,
  check_mk_bv_shift_left1,
  check_mk_bv_shift_right0,
  check_mk_bv_shift_right1,
  check_mk_bv_ashift_right,
  check_mk_bv_rotate_left,
  check_mk_bv_rotate_right,
  check_mk_bv_shl,
  check_mk_bv_lshr,
  check_mk_bv_ashr,
  check_mk_bv_extract,
  check_mk_bv_concat,
  check_mk_bv_repeat,
  check_mk_bv_sign_extend,
  check_mk_bv_zero_extend,
  check_mk_bv_redand,
  check_mk_bv_redor,
  check_mk_bv_comp,
  check_mk_bv_ge,
  check_mk_bv_gt,
  check_mk_bv_le,
  check_mk_bv_lt,
  check_mk_bv_sge,
  check_mk_bv_sgt,
  check_mk_bv_sle,
  check_mk_bv_slt,
  check_build_term,
  check_build_type,
};

static const eval_fun_t eval[NUM_BASE_OPCODES] = {
  NULL, // NO_OP
  eval_define_type,
  eval_define_term,
  eval_bind,
  eval_let,
  eval_mk_bv_type,
  eval_mk_ite,
  eval_mk_eq,
  eval_mk_diseq,
  eval_mk_distinct,
  eval_mk_not,
  eval_mk_or,
  eval_mk_and,
  eval_mk_xor,
  eval_mk_iff,
  eval_mk_implies,
  eval_mk_bv_const,
  eval_mk_bv_add,
  eval_mk_bv_sub,
  eval_mk_bv_mul,
  eval_mk_bv_neg,
  eval_mk_bv_pow,
  eval_mk_bv_div,
  eval_mk_bv_rem,
  eval_mk_bv_sdiv,
  eval_mk_bv_srem,
  eval_mk_bv_smod,
  eval_mk_bv_not,
  eval_mk_bv_and,
  eval_mk_bv_or,
  eval_mk_bv_xor,
  eval_mk_bv_nand,
  eval_mk_bv_nor,
  eval_mk_bv_xnor,
  eval_mk_bv_shift_left0,
  eval_mk_bv_shift_left1,
  eval_mk_bv_shift_right0,
  eval_mk_bv_shift_right1,
  eval_mk_bv_ashift_right,
  eval_mk_bv_rotate_left,
  eval_mk_bv_rotate_right,
  eval_mk_bv_shl,
  eval_mk_bv_lshr,
  eval_mk_bv_ashr,
  eval_mk_bv_extract,
  eval_mk_bv_concat,
  eval_mk_bv_repeat,
  eval_mk_bv_sign_extend,
  eval_mk_bv_zero_extend,
  eval_mk_bv_redand,
  eval_mk_bv_redor,
  eval_mk_bv_comp,
  eval_mk_bv_ge,
  eval_mk_bv_gt,
  eval_mk_bv_le,
  eval_mk_bv_lt,
  eval_mk_bv_sge,
  eval_mk_bv_sgt,
  eval_mk_bv_sle,
  eval_mk_bv_slt,
  eval_build_term,
  eval_build_type,
};


/*
 * Initialize the stack
 * - n = size of the operator table (must be >= NUM_BASE_OPCODES)
 * - the op_table is initialized: all default operators are defined
 */
void init_tstack(tstack_t *stack, uint32_t n) {
  op_table_t *table;
  uint32_t i;

  assert(n >= NUM_BASE_OPCODES);

  alloc_tstack(stack, n);
  table = &stack->op_table;
  for (i=0; i<NUM_BASE_OPCODES; i++) {
    table->assoc[i] = assoc[i];
    table->eval[i] = eval[i];
    table->check[i] = check[i];
  }
  table->num_ops = i;
}


/*
 * Delete the stack
 */
void delete_tstack(tstack_t *stack) {
  tstack_reset(stack);

  safe_free(stack->elem);
  stack->elem = NULL;

  delete_op_table(&stack->op_table);

  delete_arena(&stack->mem);

  safe_free(stack->aux_buffer);
  stack->aux_buffer = NULL;

  delete_bvconstant(&stack->bvconst_buffer);

  if (stack->bva64buffer != NULL) {
    yices_free_bvarith64_buffer(stack->bva64buffer);
    stack->bva64buffer = NULL;
  }

  if (stack->bvabuffer != NULL) {
    yices_free_bvarith_buffer(stack->bvabuffer);
    stack->bvabuffer = NULL;
  }

  if (stack->bvlbuffer != NULL) {
    yices_free_bvlogic_buffer(stack->bvlbuffer);
    stack->bvlbuffer = NULL;
  }
}


/*
 * Add or replace an operator
 * - op = operator code
 * - asssoc = whether op is associative or not
 * - eval. check = evaluator and checker functions
 * - op must be non-negative and less than the operator's table size
 *   (as set in init_tstack)
 *
 * If op is between 0 and stack->op_table.num_ops then the
 * current values for op are replaced. If op is larger than
 * num_ops, then a new operation is added.
 */
void tstack_add_op(tstack_t *stack, int32_t op, bool assoc, eval_fun_t eval, check_fun_t check) {
  uint32_t i, nops;

  assert(0 <= op && op < stack->op_table.size);

  i = op;
  stack->op_table.assoc[i] = assoc;
  stack->op_table.eval[i] = eval;
  stack->op_table.check[i] = check;

  nops = stack->op_table.num_ops;
  if (i >= nops) {
    stack->op_table.num_ops = i+1;
  }
}
