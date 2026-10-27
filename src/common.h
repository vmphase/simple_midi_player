#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdlib.h>

#define PI 3.14159265
#define SAMPLE_RATE 44100

#if defined(__GNUC__)
#define NORETURN __attribute__((noreturn))
#define PRINTF_LIKE __attribute__((format(printf, 1, 2)))
#else
#define NORETURN
#define PRINTF_LIKE
#endif

/* Prints "Error: ..." to stderr and exits with status 1. */
void die(const char *fmt, ...) NORETURN PRINTF_LIKE;

/* malloc-style helpers that never return NULL. */
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);

/* Uniform random number in [0, 1]. */
double frand(void);

double clampd(double x, double lo, double hi);

/* Tiny growable array. */
#define Vec(T)                                                                 \
    struct {                                                                   \
        T *items;                                                              \
        size_t len, cap;                                                       \
    }

#define vec_push(v, x)                                                         \
    do {                                                                       \
        if ((v).len == (v).cap) {                                              \
            (v).cap = (v).cap ? (v).cap * 2 : 64;                              \
            (v).items = xrealloc((v).items, (v).cap * sizeof *(v).items);      \
        }                                                                      \
        (v).items[(v).len++] = (x);                                            \
    } while (0)

#define vec_free(v)                                                            \
    do {                                                                       \
        free((v).items);                                                       \
        (v).items = NULL;                                                      \
        (v).len = (v).cap = 0;                                                 \
    } while (0)

#endif
