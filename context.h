#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include <stdint.h>

/* THIS MODULE HAS SOME INSUPERABLE LIMITATIONS 
 * If real_frame_start == frame_start, then everything works well;
 * however, if they are not equal, it's essentially impossible
 * to implement a fully functioned context_resume.
 * 1, all stack references inside call_frame should be adapted by
 *    a offset = real_frame_start - frame_start, including 
 *    int a; &a... for a in-stack `a`, int a[var_len], etc;
 *    However, in the presence of a[var_len], even machine code
 *    analysis cannot get the address in the stack to which
 *    a lvalue is placed. You need to run it again ...
 * 2, no way to handle a non-copyable object inside the call_frame;
 *    for example, a pthread_mutex_t inside call_frame.
 *    essentially because its address is recorded in an unreachable
 *    place (pthread_mutex_t recorded in glibc library memory)
 *
 * For the YIELD functionality, I assume there are <= 5 real parameters;
 * otherwise the additional arguments will be pushed to the stack,
 * thus leading a longer call_frame to be recorded.
 * And I'm not sure the compiler convention for handling struct
 * parameters...
 * These are not real obstacles, but since the real_frame_start ==
 * frame_start restriction is inescapable, these tiny details
 * become meaningless...
 **/

/* setjmp and longjmp cannot handle the case when 
 * the old function has already returned */
/* this module provides means to resume a dispeared function */
typedef struct context_s {
    /* do not need to be recorded */
    uint64_t        rax;        /*  0    */

    uint64_t        rbx;        /*  8    */

    /* useless for caller */
    uint64_t        rcx;        /*  16   */
    uint64_t        rdx;        /*  24   */
    uint64_t        rsi;        /*  32   */
    uint64_t        rdi;        /*  40   */
    uint64_t        r8;         /*  48   */
    uint64_t        r9;         /*  56   */

    uint64_t        r10;        /*  64   */
    uint64_t        r11;        /*  72   */
    uint64_t        r12;        /*  80   */
    uint64_t        r13;        /*  88   */
    uint64_t        r14;        /*  96   */
    uint64_t        r15;        /*  104  */

    /* do not need to be recorded */
    uint64_t        rsp;        /*  112  */
    uint64_t        rbp;        /*  120  */

    uint64_t        rflags;     /*  128  */

    uint64_t        rip;        /*  136  */

    uint8_t         *call_frame;
    uint64_t        frame_start;
    uint64_t        frame_len;
} __attribute__((packed)) context_t;

/* must be called at the very beginning 
 * this is the actual frame_start when resume */
#define CONTEXT_READ_RBP(v)   asm ("movq %%rbp, %0" : "=r"(v));

#define CONTEXT_READ_RSP(v)   asm ("movq %%rsp, %0" : "=r"(v));

context_t *context_create();
void context_destroy(context_t *cxt);

/* return 0 when called normally */
/* return 1 when called by context_resume */
uint64_t context_save(context_t *cxt, uint64_t frame_start);

void context_resume(context_t *cxt, uint64_t real_frame_start);

#define YIELD_INIT(cxt) YIELD(cxt, 0)

#define YIELD(cxt, res)                         \
        do {                                    \
            uint64_t r;                         \
            CONTEXT_READ_RBP(r);                \
            if ( context_save(cxt, r) == 0 ){   \
                /* normal */                    \
                return (res);                   \
            }                                   \
        } while (0)


//TODO now only uint64_t YIELD is supported, add supportfor other types

/* the whole function frame of this zombie will be
 * destroyed, return by the replacer exactly to
 * its caller */
uint64_t context_resume_zombie(context_t *cxt);

#define GETYIELD(cxt) context_resume_zombie(cxt)


#endif /* _CONTEXT_H_ */
