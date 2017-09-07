#include "context.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* glibc stack convention */
#define STACK_ALIGN_BYTES   16

/* v should be as simple as a straightforward address 
 * such that the compiler can load it in rdi in one 
 * instruction without interfering with other registers */
#define CONTEXT_WRITE_RBP(v)   asm ("movq %0, %%rbp" : : "D"(v));
#define CONTEXT_WRITE_RAX(v)   asm ("movq %0, %%rax" : : "D"(v));

context_t*
context_create()
{
    context_t *res = (context_t*) malloc(sizeof(context_t));
    res->call_frame = NULL;
    res->frame_len  = 0;
    return res;
}

void
context_destroy(context_t *cxt)
{
    free(cxt->call_frame);
    free(cxt);
}

uint64_t
context_save(context_t *cxt, uint64_t frame_start)
{
    uint64_t rbp;
    CONTEXT_READ_RBP(rbp);

    cxt->frame_start = frame_start;

    size_t nframe_len = frame_start - rbp;
    if ( nframe_len != cxt->frame_len ){
        if ( cxt->frame_len < nframe_len ){
            free(cxt->call_frame);
            cxt->call_frame = (uint8_t*) malloc(nframe_len);
        }
        cxt->frame_len = nframe_len;
    }
    memcpy(cxt->call_frame, (void*)rbp, cxt->frame_len);

    CONTEXT_WRITE_RAX(cxt);
    
    /* pure assembly starts, do not use C variable any more */
    asm ( 
          /* save rflags first */
          "pushfq\n"
          "pop      128(%rax)\n"
          
          "movq     %rbx,   8(%rax)\n"
          "movq     %r10,   64(%rax)\n"
          "movq     %r11,   72(%rax)\n"
          "movq     %r12,   80(%rax)\n"
          "movq     %r13,   88(%rax)\n"
          "movq     %r14,   96(%rax)\n"
          "movq     %r15,   104(%rax)\n"

          /* save rip right before ret */
          "jmp      lb1\n"
          "lb2:\n"
          "pop      136(%rax)\n"
          "movq     $0,     %rax\n"
          "jmp      lb3\n"
          "lb1:\n"
          "call     lb2\n"

          /* leave this function */
          /* this is the address for the saved rip */
          /* note we don't need to save rsp pointer,
           * because it's immediately changed after return to lb3 */
          "lb3:\n"
          "movq     %rbp,   %rsp\n"
          "pop      %rbp\n"
          "ret\n"
        );

    /* never reach here, suppress compiler warning */
    return 0;
}


void
_context_resume(context_t *cxt, uint64_t real_frame_start)
{
    uint64_t nrbp = real_frame_start - cxt->frame_len;
    memcpy((void*)nrbp, cxt->call_frame, cxt->frame_len);

    /* adjust all rbp values involved */
    uint64_t cur_shi = 0;
    while ( cur_shi != cxt->frame_len ){
        uint64_t old_value = *(uint64_t*)(cxt->call_frame + cur_shi);
        *(uint64_t*)(nrbp + cur_shi) = old_value + (real_frame_start - cxt->frame_start);
        cur_shi = *(uint64_t*)(cur_shi + cxt->call_frame) - (cxt->frame_start - cxt->frame_len);
    }

    CONTEXT_WRITE_RAX(cxt);

    CONTEXT_WRITE_RBP(nrbp);

    asm (
          /* now restore assigned rflags */
          "push     128(%rax)\n"
          "popfq\n"

          "movq     8(%rax),    %rbx\n"
          "movq     64(%rax),   %r10\n"
          "movq     72(%rax),   %r11\n"
          "movq     80(%rax),   %r12\n"
          "movq     88(%rax),   %r13\n"
          "movq     96(%rax),   %r14\n"
          "movq     104(%rax),  %r15\n"

          /* restore rip */
          "push     136(%rax)\n"
          "movq     $1,         %rax\n"
          "ret\n"
        );
} 

#define MAXOFTWO(a, b)      ((a) < (b) ? (b) : (a))
#define ROUNDUP(p, base)    (((p) + (base) - 1) / (base) * (base))

void
context_resume(context_t *cxt, uint64_t real_frame_start)
{
    /* to be sure the stack at this function frame is big enough */
    /* otherwise, memcpy will destroy the running stack */
    uint64_t rsp;
    CONTEXT_READ_RSP(rsp);

    size_t bufsz = MAXOFTWO(cxt->frame_len - (real_frame_start - rsp) , 0);
    bufsz = ROUNDUP(bufsz, STACK_ALIGN_BYTES) + 256;

    asm ( "subq     %0, %%rsp\n"
        : "=r"(bufsz)
        );

    _context_resume(cxt, real_frame_start);

    /* never return */
}

uint64_t
context_resume_zombie(context_t *cxt)
{
    uint64_t r;
    CONTEXT_READ_RBP(r);
    context_resume(cxt, r);

    /* never return by itself, however, the replacer
     * will behave as if this function is returned */
    return 0;
}

// #define DEBUG_CONTEXT
#ifdef  DEBUG_CONTEXT

static context_t *cxt;
static uint64_t frame_start;

static int cond1, cond2;

static int
show_msg()
{
    printf("show_msg running...\n");
    if ( !cond1 ){
        printf("cond1 failed, yield...\n");
        int r = context_save(cxt, frame_start);
        if ( r == 0 ) {
            /* normal */
            return -1;
        } 
    }

    /* now cond1 is satisfied */
    printf("cond1 ok! processing task1...\n");

    
    if ( !cond2 ){
        printf("cond2 failed, yield...\n");
        int r = context_save(cxt, frame_start);
        if ( r == 0 ) {
            /* normal */
            return -2;
        } 
    }

    /* now cond2 is satisfied */
    printf("cond2 ok! processing task2...\n");

    printf("show_msg done...\n");
    return 0;
}

static void
test()
{
    CONTEXT_READ_RBP(frame_start);
    cxt = context_create();

    int r;
    if ( (r = show_msg()) == -1 ){
        cond1 = 1;
        printf("master validated cond1...\n");
        context_resume(cxt, frame_start);
    } else if ( r == -2 ){
        cond2 = 1;
        printf("master validated cond2...\n");
        context_resume(cxt, frame_start);
    } else {
        printf("master: show_msg done...\n");
    }

    context_destroy(cxt);
    printf("end of master\n");
}

#define YIELDABLDEND        1000

static uint64_t
yieldable(context_t *cxt)
{
    YIELD_INIT(cxt);

    for ( int i = 12; i < 23; i++ ){
        printf("hello i = %d\n", i);
        YIELD(cxt, i);
    }

    printf("yieldable end...\n");
    return YIELDABLDEND;
}

static void
test_yield()
{
    context_t *cxt = context_create();

    yieldable(cxt);

    uint64_t r;
    while ( (r = GETYIELD(cxt)) != YIELDABLDEND ){
        printf("r = %llu\n", r);
    }
    printf("end of test_yield...\n");
}

void
context_inline_test()
{
    test();
    printf("*********\n");
    test_yield();
}

#endif  /* DEBUG_CONTEXT */
