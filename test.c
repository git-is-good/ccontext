#include "context.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

const size_t testsz = 1000;

uint64_t
prime_generator(context_t *cxt)
{
    YIELD_INIT(cxt);

    const size_t default_sz = 64;
    size_t cap = default_sz, sz = 0;
    uint64_t *primes = (uint64_t*) malloc(sizeof(uint64_t) * cap);

    for ( size_t i = 2; ; i++ ){
        int isprime = 1;
        for ( size_t j = 0; j < sz; j++ ){
            if ( i % primes[j] == 0 ){
                isprime = 0;
                break;
            }
        }
        if ( isprime ){
            if ( sz + 1 > cap ){
                cap *= 2;
                primes = (uint64_t*) realloc(primes, sizeof(uint64_t) * cap);
            }
            primes[sz++] = i;
            YIELD(cxt, i);
        }
    }
}

void
prime_generator2()
{
    const size_t default_sz = 64;
    size_t cap = default_sz, sz = 0;
    uint64_t *primes = (uint64_t*) malloc(sizeof(uint64_t) * cap);

    for ( size_t i = 2; i < testsz; i++ ){
        int isprime = 1;
        for ( size_t j = 0; j < sz; j++ ){
            if ( i % primes[j] == 0 ){
                isprime = 0;
                break;
            }
        }
        if ( isprime ){
            if ( sz + 1 > cap ){
                cap *= 2;
                primes = (uint64_t*) realloc(primes, sizeof(uint64_t) * cap);
            }
            primes[sz++] = i;
        //    printf("p[%lu] = %lu\n", sz - 1, i);
        }
    }
}

void
test1()
{
    time_t st = clock();
    prime_generator2();
    printf("prime_generator2: %ld\n", clock() - st);

    context_t *cxt = context_create();
    prime_generator(cxt);

    st = clock();
    for ( size_t i = 0; i < testsz; i++ ){
        uint64_t p = GETYIELD(cxt);
        printf("p[%lu] = %llu\n", i, p);
    }
    printf("prime_generator: %ld\n", clock() - st);
    context_destroy(cxt);

#ifdef DEBUG_CONTEXT
    void context_inline_test();
    context_inline_test();
#endif /* DEBUG_CONTEXT */

}

/* test for different frame_start */

void
_dump_test2(int i, context_t *cxt)
{
    void *rbp;
    CONTEXT_READ_RBP(rbp);
    printf("frame_start at %p...", rbp);

    uint64_t p = GETYIELD(cxt);
    printf("prime = %llu\n", p);

    if ( i > 0 ) _dump_test2(i - 1, cxt);
}


void
test2()
{
    context_t *cxt = context_create();
    prime_generator(cxt);

    _dump_test2(10, cxt);
    context_destroy(cxt);
}

int main(){
    test1();
    test2();
}

