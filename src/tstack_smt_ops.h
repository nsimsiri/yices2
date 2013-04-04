/*
 * TERM-STACK OPERATIONS TO SUPPORT SMT-LIB 1.2
 */

/*
 * The term_stack data structures and main operations
 * are defined in "term_stack.h". To support the 
 * SMT-LIB notation (version 1.2), we redefine the 
 * following operations:
 *
 * Yices 2 Version                      SMT 1.2 Version
 *
 * [mk-eq <term> <term> ]            [mk-eq <term> .... <term>]
 * [mk-bv-const <size> <value>]      [mk-bv-const <value> <size>]
 * [mk-bv-rotate <bv> <index>]       [mk-bv-rotate <index> <bv>]
 * [mk-bv-repeat <bv> <index>]       [mk-bv-repeat <index> <bv>]
 * [mk-zero-extend <bv> <number>]    [mk-zero-extend <number> <bv> ]
 * [mk-sign-extend <bv> <number>]    [mk-sign-extend <number> <bv> ]
 *
 * NOTE: older version of SMT-LIB, was using the same convention
 * as Yices for sign-extend. We don't support it.
 */

#ifndef __TSTACK_SMT_OPS
#define __TSTACK_SMT_OPS

#include "term_stack2.h"

/*
 * Initialize stack for SMT-LIB 1.2:
 * - this redefines the operations above
 */
extern void tstack_enable_smt(tstack_t *stack);


#endif /* __TSTACK_SMT_OPTS */
