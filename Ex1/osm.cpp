#ifndef _OSM_C
#define _OSM_C


#include "osm.h"
#include <sys/time.h>
#include <cstdio>
#include <cmath>
#define UNROLLING_FACTOR 10

/* calling a system call that does nothing */
#define OSM_NULLSYSCALL asm volatile( "int $0x80 " : : \
"a" (0xffffffff) /* no such syscall */, "b" (0), "c" (0), "d" (0) /*:\
"eax", "ebx", "ecx", "edx"*/)


/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time(unsigned int iterations) {
    if (iterations == 0) {
        return -1;
    }
    struct timeval before_time;
    struct timeval after_time;
    gettimeofday(&before_time, NULL);
    int x = 0;
    int rounded_up = ceil(iterations / UNROLLING_FACTOR);
    for (int i = 0; i < rounded_up; i++) {
        x = x + 2;
        x = x + 2;
        x = x + 2;
        x = x + 2;
        x = x + 2;
        x = x + 2;
        x = x + 2;
        x = x + 2;
        x = x + 2;
        x = x + 2;
    }
    gettimeofday(&after_time, NULL);
    double nano_sec = (double)(after_time.tv_sec - before_time.tv_sec) * 1e9;
    double nano_usec = (double)(after_time.tv_usec - before_time.tv_usec) * 1e3;

    return ((nano_sec + nano_usec) / (rounded_up * UNROLLING_FACTOR));
}

void FooNoArg() {
    return;
}

/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time(unsigned int iterations) {
    if (iterations == 0) {
        return -1;
    }
    struct timeval before_time;
    struct timeval after_time;
    gettimeofday(&before_time, NULL);
    int rounded_up = ceil(iterations / UNROLLING_FACTOR);
    for (int i = 0; i < rounded_up; i++) {
        FooNoArg();
        FooNoArg();
        FooNoArg();
        FooNoArg();
        FooNoArg();
        FooNoArg();
        FooNoArg();
        FooNoArg();
        FooNoArg();
        FooNoArg();
    }
    gettimeofday(&after_time, NULL);
    double nano_sec = (double)(after_time.tv_sec - before_time.tv_sec) * 1e9;
    double nano_usec = (double)(after_time.tv_usec - before_time.tv_usec) * 1e3;

    return ((nano_sec + nano_usec) / (rounded_up * UNROLLING_FACTOR));
}


/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_syscall_time(unsigned int iterations) {
    if (iterations == 0) {
        return -1;
    }
    struct timeval before_time;
    struct timeval after_time;
    gettimeofday(&before_time, NULL);
    int rounded_up = ceil(iterations / UNROLLING_FACTOR);
    for (int i = 0; i < rounded_up; i++) {
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
        OSM_NULLSYSCALL;
    }
    gettimeofday(&after_time, NULL);
    double nano_sec = (double)(after_time.tv_sec - before_time.tv_sec) * 1e9;
    double nano_usec = (double)(after_time.tv_usec - before_time.tv_usec) * 1e3;

    return ((nano_sec + nano_usec) / (rounded_up * UNROLLING_FACTOR));
}

#endif


