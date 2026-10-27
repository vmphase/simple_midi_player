#include "common.h"

#include <stdarg.h>
#include <stdio.h>

void die(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fputs("Error: ", stderr);
    vfprintf(stderr, fmt, args);
    fputc('\n', stderr);
    va_end(args);
    exit(1);
}

void *xcalloc(size_t count, size_t size) {
    void *p = calloc(count ? count : 1, size);
    if (!p)
        die("out of memory");
    return p;
}

void *xrealloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p)
        die("out of memory");
    return p;
}

double frand(void) { return rand() / (double)RAND_MAX; }

double clampd(double x, double lo, double hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}
