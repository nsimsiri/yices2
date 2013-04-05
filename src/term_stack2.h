/*
 * Stack-based API for building terms and types
 * Intended to support parsing.
 *
 * The stack contains a nested sequence of frames.  Each frame
 * consists of an operator (term or type constructor) and a sequence
 * of arguments. The arguments are string (that may denote symbols),
 * bindings, rationals or bitvector constants, buffers, terms, or types. 
 *
 * Bindings are pairs <name, term>. They record temporary bindings from
 * names to terms (for let, forall, exists). The binding of name to term
 * is erased when the binding is deleted.
 *
 * To help reporting errors, each element on the stack has location
 * information.
 *
 * Each operation is defined by an op code and implemented by two functions:
 * - one for checking types and number of arguments
 * - one for applying the operation to the arguments
 * Both have the following signature:
 * - void check_op(tstack_t *stack, stack_elem_t *f, uint32_t n)
 * - void eval_op(tstack_t *stack, stack_elem_t *f, uint32_t n)
 * f is the start of a frame in stack->elem
 * n = the size of the frame = number of arguments
 *
 * For example, if the stack contains a frame with operator code MK_AND 
 * and 4 arguments, then the top frame is [MK_AND <arg1> ... <arg4>]
 *
 * tstack_eval will invoke eval_mk_and(stack, f, n)
 * with f pointing to array [<arg1> .... <arg4>] and n = 4
 *
 * The check function must raise an exception if the arguments or 
 * frame are incorrect. The eval function must replace the frame
 * by the result of the operation.
 *
 * The module implements basic operations. More can be added.
 */

#ifndef __TERM_STACK2_H
#define __TERM_STACK2_H

#include <stdint.h>
#include <stdbool.h>
#include <setjmp.h>

#include "arena.h"
#include "terms.h"

#include "bvlogic_buffers.h"

/*
 * Objects on the stack
 * - tag = identifies the object's type
 * - val = union type
 * - loc = location information for error reporting
 *
 * For operators, we record an opcode, a multiplicity index (for associative
 * operators), and the index of the previous operator on the stack.
 */
typedef enum tag_enum {
  TAG_NONE,
  TAG_OP,               // operator
  TAG_SYMBOL,           // symbol
  TAG_STRING,           // string constant
  TAG_BV64,             // bit-vector constant (1 to 64 bits)
  TAG_BV,               // bit-vector constant (more than 64 bits)
  TAG_RATIONAL,         // rational constant
  TAG_TERM,             // term index + polarity (from the global term table)
  TAG_TYPE,             // type index (from the global type table)
  TAG_MACRO,            // type macro (index in the type table)
  TAG_ARITH_BUFFER,     // polynomial buffer (rational coefficients)
  TAG_BVARITH64_BUFFER, // polynomial buffer (bitvector coefficients, 1 to 64 bits)
  TAG_BVARITH_BUFFER,   // polynomial buffer (bitvector coefficients, more than 64 bits)
  TAG_BVLOGIC_BUFFER,   // array of bits
  TAG_BINDING,          // pair <name, term>
  TAG_TYPE_BINDING,     // pair <name, type>
} tag_t;

#define NUM_TAGS (TAG_TYPE_BINDING+1)

// operator
typedef struct opval_s {
  int32_t opcode;
  uint32_t multiplicity;
  uint32_t prev;
} opval_t;

// binding 
typedef struct binding_s {
  term_t term;
  char *symbol;
} binding_t;

// type binding
typedef struct type_binding_s {
  type_t type;
  char *symbol;
} type_binding_t;

// location: line + column number
typedef struct loc_s {
  uint32_t line; 
  uint32_t column;
} loc_t;

// two variant representations for bitvector constants
// one for bitsize between 1 and 64
// one for bitsize > 64
typedef struct bv64_s {
  uint32_t bitsize; // size in bits
  uint64_t value;   // value (padded to 64 bits)
} bv64_t;

typedef struct bv_s { 
  uint32_t bitsize; // size in bits
  uint32_t *data;   // value as an array of 32bit words
} bv_t;


// element on the stack
typedef struct stack_elem_s {
  tag_t tag;
  union {
    opval_t opval;
    char *symbol;
    char *string;
    bv64_t bv64;
    bv_t bv;
    rational_t rational;
    term_t term;
    type_t type;
    int32_t macro;
    arith_buffer_t *arith_buffer;
    bvarith64_buffer_t *bvarith64_buffer;
    bvarith_buffer_t *bvarith_buffer;
    bvlogic_buffer_t *bvlogic_buffer;
    binding_t binding;
    type_binding_t type_binding;
  } val;
  loc_t loc;
} stack_elem_t ;


/*
 * Operator table
 * - num ops = number of operators
 * - for each op:
 *   assoc[op] = true if op is to be treated as an associative operator
 *   check[op] = check function
 *   eval[op] = evaluation function
 * - size = size of arrays assoc, check, and eval
 */
typedef struct tstack_s tstack_t;
// type of evaluator and check functions
typedef void (*eval_fun_t)(tstack_t *stack, stack_elem_t *f, uint32_t n);
typedef eval_fun_t check_fun_t;

typedef struct op_table_s {
  uint8_t *assoc;
  eval_fun_t *eval;
  check_fun_t *check;
  uint32_t num_ops;
  uint32_t size;
} op_table_t;

#define MAX_OP_TABLE_SIZE (UINT32_MAX/sizeof(eval_fun_t))

/*
 * Stack:
 * - array of stack_elements
 * - top = top of the stack
 *   elements are stored at indices 0 ... top-1
 *   a bottom marker is stored at index 0
 * - frame = index of the top-frame, element at that index must be an operator
 * - top_op = opcode of the element at index frame
 *
 * - mem = arena for allocation of strings/symbols
 *
 * - auxiliary buffers for internal computations
 * - a global counter for creating fresh variables
 * - a longjmp buffer for simulating exceptions
 *
 * - result: some operations store a term or type result in
 *   stack->term_result or stack->type_result
 *
 * - diagnosis data for error reporting is stored in
 *   error_loc = loc[i] if error occurred on element i 
 *   error_op = operator being evaluated when the error occurred 
 *          (or NO_OP if the error occurred on a push operation)
 *   error_string = null-terminated string value if the erroneous
 *          argument is a string (or NULL).
 */
struct tstack_s {
  stack_elem_t *elem;

  uint32_t top;
  uint32_t size;
  uint32_t frame;
  int32_t top_op;

  // operator table
  op_table_t op_table;

  arena_t mem;

  // vector to store types or terms
  int32_t *aux_buffer;
  uint32_t aux_size;

  // buffer to convert stack elements to bitvector constants
  bvconstant_t bvconst_buffer;

  // dynamically allocated buffers
  arith_buffer_t *abuffer;
  bvarith64_buffer_t *bva64buffer;
  bvarith_buffer_t *bvabuffer;
  bvlogic_buffer_t *bvlbuffer;  

  // counter for type-variable creation
  uint32_t tvar_id;

  // result of BUILD_TERM/BUILD_TYPE
  union {
    term_t term;
    type_t type;
  } result;

  jmp_buf env;
  loc_t error_loc;
  int32_t error_op;
  char *error_string;
};


/*
 * Default and maximal size
 */
#define DEFAULT_TERM_STACK_SIZE 256
#define MAX_TERM_STACK_SIZE (UINT32_MAX/64)

/*
 * Default and maximal size of the t_aux vector
 */
#define DEFAULT_AUX_SIZE 256
#define MAX_AUX_SIZE (UINT32_MAX/4)


/*
 * Exception handling via setjmp and longjmp:
 * -----------------------------------------
 * To set the handler call setjmp(stack->env)
 * The exception handler is called on any error
 * via longjmp(stack->env, error_code).
 *
 * When an exception is raised, the stack may be in an inconsistent
 * state. Do not do any operations on the stack without calling 
 * tstack_reset first.
 */

/*
 * Error codes
 */
typedef enum tstack_error_s {
  TSTACK_NO_ERROR = 0,
  TSTACK_INTERNAL_ERROR,
  TSTACK_OP_NOT_IMPLEMENTED,
  TSTACK_UNDEF_TERM,
  TSTACK_UNDEF_TYPE,
  TSTACK_UNDEF_MACRO,
  TSTACK_RATIONAL_FORMAT,
  TSTACK_FLOAT_FORMAT,
  TSTACK_BVBIN_FORMAT,
  TSTACK_BVHEX_FORMAT,
  TSTACK_TYPENAME_REDEF,
  TSTACK_TERMNAME_REDEF,
  TSTACK_MACRO_REDEF,
  TSTACK_DUPLICATE_SCALAR_NAME,
  TSTACK_DUPLICATE_VAR_NAME,
  TSTACK_INVALID_OP,
  TSTACK_INVALID_FRAME,
  TSTACK_INTEGER_OVERFLOW,
  TSTACK_NEGATIVE_EXPONENT,
  TSTACK_NOT_AN_INTEGER,
  TSTACK_NOT_A_STRING,
  TSTACK_NOT_A_SYMBOL,  
  TSTACK_NOT_A_RATIONAL,
  TSTACK_NOT_A_TYPE,
  TSTACK_ARITH_ERROR,
  TSTACK_DIVIDE_BY_ZERO,
  TSTACK_NON_CONSTANT_DIVISOR,
  TSTACK_NONPOSITIVE_BVSIZE,
  TSTACK_INCOMPATIBLE_BVSIZES,
  TSTACK_INVALID_BVCONSTANT,
  TSTACK_BVARITH_ERROR,
  TSTACK_BVLOGIC_ERROR,
  TSTACK_TYPE_ERROR_IN_DEFTERM,
  TSTACK_YICES_ERROR,
} tstack_error_t;

#define NUM_TSTACK_ERRORS (TSTACK_YICES_ERROR+1)


/*
 * PREDEFINED OPERATIONS
 */
enum base_opcodes {
  NO_OP,              // used as a marker

  // global definitions
  DEFINE_TYPE,        // [define-type <symbol>] or [define-type <symbol> <type>]
  DEFINE_TERM,        // [define-term <symbol> <type>] or [define-term <symbol> <type> <value> ]

  // bindings
  BIND,               // [bind <symbol> <term> ]
  DECLARE_VAR,        // [declare-var <symbol> <type> ] 
  DECLARE_TYPE_VAR,   // [declare-type-var <symbol> ]
  LET,                // [let <binding> ... <binding> <term> ]

  // type constructors
  MK_BV_TYPE,         // [mk-bv-type <rational> ]
  MK_SCALAR_TYPE,     // [mk-scalar-type <symbol> ... <symbol> ]
  MK_TUPLE_TYPE,      // [mk-tuple-type <type> ... <type> ]
  MK_FUN_TYPE,        // [mk-fun-type <type> ... <type> ]
  MK_APP_TYPE,        // [mk-app-type <macro> <type> ... <type> ]

  // basic term constructors
  MK_APPLY,           // [mk-apply <term> ... <term> ]
  MK_ITE,             // [mk-ite <term> <term> <term> ]
  MK_EQ,              // [mk-eq <term> <term> ]
  MK_DISEQ,           // [mk-diseq <term> <term> ]
  MK_DISTINCT,        // [mk-distinct <term> ... <term> ]
  MK_NOT,             // [mk-not <term> ]
  MK_OR,              // [mk-or <term> ... <term> ]
  MK_AND,             // [mk-and <term> ... <term> ]
  MK_XOR,             // [mk-xor <term> ... <term> ]
  MK_IFF,             // [mk-iff <term> <term> ]
  MK_IMPLIES,         // [mk-implies <term> <term> ]
  MK_TUPLE,           // [mk-tuple <term> ... <term> ]
  MK_SELECT,          // [mk-select <term> <rational> ]
  MK_TUPLE_UPDATE,    // [mk-tuple-update <term> <rational> <term> ]
  MK_UPDATE,          // [mk-update <term> <term> .... <term> ]
  MK_FORALL,          // [mk-forall <binding> ... <binding> <term> ]
  MK_EXISTS,          // [mk-exists <binding> ... <binding> <term> ]
  MK_LAMBDA,          // [mk-lambda <binding> ... <binding> <term> ]

  // arithmetic
  MK_ADD,             // [mk-add <arith> ... <arith> ]
  MK_SUB,             // [mk-sub <arith> ... <arith> ]
  MK_NEG,             // [mk-neg <arith> ]
  MK_MUL,             // [mk-mul <arith> ... <arith> ]
  MK_DIVISION,        // [mk-division <arith> <arith> ]
  MK_POW,             // [mk-pow <arith> <integer> ]
  MK_GE,              // [mk-ge <arith> <arith> ]
  MK_GT,              // [mk-gt <arith> <arith> ]
  MK_LE,              // [mk-le <arith> <arith> ]
  MK_LT,              // [mk-lt <arith> <arith> ]

  // bitvector arithmetic
  MK_BV_CONST,        // [mk-bv-const <size> <value> ]
  MK_BV_ADD,          // [mk-bv-add <bv> ... <bv> ]
  MK_BV_SUB,          // [mk-bv-sub <bv> ... <bv> ]
  MK_BV_MUL,          // [mk-bv-mul <bv> ... <bv> ]
  MK_BV_NEG,          // [mk-bv-neg <bv> ]
  MK_BV_POW,          // [mk-bv-pow <bv> <integer> ]
  MK_BV_DIV,          // [mk-bv-div <bv> <bv> ]
  MK_BV_REM,          // [mk-bv-rem <bv> <bv> ]
  MK_BV_SDIV,         // [mk-bv-sdiv <bv> <bv> ]
  MK_BV_SREM,         // [mk-bv-srem <bv> <bv> ]
  MK_BV_SMOD,         // [mk-bv-smod <bv> <bv> ]
  
  MK_BV_NOT,          // [mk-bv-not <bv> ]
  MK_BV_AND,          // [mk-bv-and <bv> ... <bv> ]
  MK_BV_OR,           // [mk-bv-or <bv> ... <bv> ]
  MK_BV_XOR,          // [mk-bv-xor <bv> ... <bv> ]
  MK_BV_NAND,         // [mk-bv-nand <bv> ... <bv> ]
  MK_BV_NOR,          // [mk-bv-nor <bv> ... <bv> ]
  MK_BV_XNOR,         // [mk-bv-xnor <bv> ... <bv> ]

  MK_BV_SHIFT_LEFT0,   // [mk-bv-shift-left0 <bv> <integer> ]
  MK_BV_SHIFT_LEFT1,   // [mk-bv-shift-left1 <bv> <integer> ]
  MK_BV_SHIFT_RIGHT0,  // [mk-bv-shift-right0 <bv> <integer> ]
  MK_BV_SHIFT_RIGHT1,  // [mk-bv-shift-right1 <bv> <integer> ]
  MK_BV_ASHIFT_RIGHT,  // [mk-bv-ashift-right <bv> <integer> ]
  MK_BV_ROTATE_LEFT,   // [mk-bv-rotate-left <bv> <rational> ]
  MK_BV_ROTATE_RIGHT,  // [mk-bv-rotate-right <bv> <rational> ]

  MK_BV_SHL,          // [mk-bv-shl <bv> <bv> ]
  MK_BV_LSHR,         // [mk-bv-lshr <bv> <bv> ]
  MK_BV_ASHR,         // [mk-bv-ashr <bv> <bv> ]

  MK_BV_EXTRACT,      // [mk-bv-extract <rational> <rational> <bv> ]
  MK_BV_CONCAT,       // [mk-bv-concat <bv> ... <bv> ]
  MK_BV_REPEAT,       // [mk-bv-repeat <bv> <rational> ]
  MK_BV_SIGN_EXTEND,  // [mk-bv-sign-extend <bv> <rational> ]
  MK_BV_ZERO_EXTEND,  // [mk-bv-zero-extend <bv> <rational> ]

  MK_BV_REDAND,       // [mk-bv-redand <bv> ]
  MK_BV_REDOR,        // [mk-bv-redor <bv> ]
  MK_BV_COMP,         // [mk-bv-comp <bv> <bv> ]

  MK_BV_GE,           // [mk-bv-ge <bv> <bv> ]
  MK_BV_GT,           // [mk-bv-gt <bv> <bv> ]
  MK_BV_LE,           // [mk-bv-le <bv> <bv> ]
  MK_BV_LT,           // [mk-bv-lt <bv> <bv> ]
  MK_BV_SGE,          // [mk-bv-sge <bv> <bv> ]
  MK_BV_SGT,          // [mk-bv-sgt <bv> <bv> ]
  MK_BV_SLE,          // [mk-bv-sle <bv> <bv> ]
  MK_BV_SLT,          // [mk-bv-slt <bv> <bv> ]

  // collect result
  BUILD_TERM,         // [build-term <term> ]
  BUILD_TYPE,         // [build-type <type> ]
};


#define NUM_BASE_OPCODES (BUILD_TYPE + 1)


/*
 * Initialization
 * - n = size of the operator table (must be >= NUM_BASE_OPCODES)
 * - the op_table is initialized: all default operators are defined
 */
extern void init_tstack(tstack_t *stack, uint32_t n);

/*
 * Add or replace an operator
 * - op = operator code 
 * - asssoc = whether op is associative or not
 * - eval. check = evaluator and checker functions
 * - op must be non-negative and less than the operator's table size
 *   (set in init_tstack)
 *
 * If op is between 0 and stack->op_table.num_ops then the
 * current values for op are replaced. If op is larger than
 * num_ops, then a new operation is added. 
 */
extern void tstack_add_op(tstack_t *stack, int32_t op, bool assoc, eval_fun_t eval, check_fun_t check);

/*
 * Empty the stack
 */
extern void tstack_reset(tstack_t *stack);

/*
 * Delete all
 */
extern void delete_tstack(tstack_t *stack);


/*
 * PUSH DATA OR OPERATOR
 */

/*
 * Push an operator op
 * - op = opcode
 * - loc = location
 * op is considered valid if it's between 0 and num_ops
 *
 * raise exception TSTACK_INVALID_OP if op is invalid and set 
 *  stack->error_loc = loc
 *  stack->error_op = op
 *  stack->error_string = NULL
 */
extern void tstack_push_op(tstack_t *stack, int32_t op, loc_t *loc);

/*
 * Push a string or symbol of length n
 * - tag should be either TAG_SYMBOL or TAG_STRING
 * - copy s[0] ... s[n-1] and add '\0'
 * - s must be terminated by '\0'
 */
extern void tstack_push_str(tstack_t *stack, tag_t tag, char *s, uint32_t n, loc_t *loc);

static inline void tstack_push_string(tstack_t *stack, char *s, uint32_t n, loc_t *loc) {
  tstack_push_str(stack, TAG_STRING, s, n, loc);
}

static inline void tstack_push_symbol(tstack_t *stack, char *s, uint32_t n, loc_t *loc) {
  tstack_push_str(stack, TAG_SYMBOL, s, n, loc);
}

/*
 * These functions are like push_symbol but they raise an exception if
 * the name is already used (TSTACK_TYPENAME_REDEF,
 * TSTACK_TERMNAME_REDEF, or TSTACK_MACRO_REDEF)
 */
extern void tstack_push_free_typename(tstack_t *stack, char *s, uint32_t n, loc_t *loc);
extern void tstack_push_free_termname(tstack_t *stack, char *s, uint32_t n, loc_t *loc);
extern void tstack_push_free_macroname(tstack_t *stack, char *s, uint32_t n, loc_t *loc);

/*
 * Find the term or type of name s and push that term or type on the stack
 *
 * raise exception TSTACK_UNDEF_TERM or TSTACK_UNDEF_TYPE if the name is
 * not mapped to a term or type.
 */
extern void tstack_push_type_by_name(tstack_t *stack, char *s, loc_t *loc);
extern void tstack_push_term_by_name(tstack_t *stack, char *s, loc_t *loc);
extern void tstack_push_macro_by_name(tstack_t *stack, char *s, loc_t *loc);

/*
 * Convert a string to a rational and push that
 * - s must be null-terminated and of rational or floating point formats
 *  (cf. rational.h, yices.h)
 *
 * raise exception TSTACK_FORMAT_... if the string s does not have 
 * the right format, and set
 *   stack->error_loc = loc
 *   stack->error_op = NO_OP
 *   stack->error_string = s
 */
extern void tstack_push_rational(tstack_t *stack, char *s, loc_t *loc);
extern void tstack_push_float(tstack_t *stack, char *s, loc_t *loc);

/*
 * Convert a string to a bitvector constant an push that
 * - n = length of the string
 * - s must be a string of binary or hexadecimal digits (no prefix)
 *
 * raise exception TSTACK_FORMAT_... if the string s does not have 
 * the right format, and set
 *   stack->error_loc = loc
 *   stack->error_op = NO_OP
 *   stack->error_string = s
 */
extern void tstack_push_bvbin(tstack_t *stack, char *s, uint32_t n, loc_t *loc);
extern void tstack_push_bvhex(tstack_t *stack, char *s, uint32_t n, loc_t *loc);

/*
 * Push primitive types or terms
 */
extern void tstack_push_bool_type(tstack_t *stack, loc_t *loc);
extern void tstack_push_int_type(tstack_t *stack, loc_t *loc);
extern void tstack_push_real_type(tstack_t *stack, loc_t *loc);
extern void tstack_push_true(tstack_t *stack, loc_t *loc);
extern void tstack_push_false(tstack_t *stack, loc_t *loc);

/*
 * Push integer constants
 */
extern void tstack_push_int32(tstack_t *stack, int32_t val, loc_t *loc);

/*
 * Push terms or types built by other means:
 * use these functions for predefined SMT-LIB terms and types
 */
extern void tstack_push_term(tstack_t *stack, term_t t, loc_t *loc);
extern void tstack_push_type(tstack_t *stack, type_t tau, loc_t *loc);
extern void tstack_push_macro(tstack_t *stack, int32_t id, loc_t *loc);


/*
 * EVALUATION
 */

/*
 * Eval: execute the operation defined by the top-level operator OP,
 * applied to all the arguments on top of OP on the stack.
 *
 * Replace [op arg1 ... argn] by the result of the operation.
 */
extern void tstack_eval(tstack_t *stack);

/*
 * Check whether the stack is empty
 */
static inline bool tstack_is_empty(tstack_t *stack) {
  return stack->top == 1;
}

/*
 * Read result. 
 *
 * Call sequence to use these functions:
 * 1) tstack_push_op(stack, BUILD_TERM, xxx)
 * 2) sequence of push/eval to build the term
 * 3) when tstack_eval evaluates the BUILD_TERM command,
 *    stack->result.term is available.
 * 
 * Same thing for types, but replace by BUILD_TYPE.
 */
static inline term_t tstack_get_term(tstack_t *stack) {
  return stack->result.term;
}

static inline term_t tstack_get_type(tstack_t *stack) {
  return stack->result.type;
}



#endif /* __TERM_STACK2_H */