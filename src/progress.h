#ifndef PROGRESS_H
#define PROGRESS_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef struct {
    const char *label;
    size_t total;
    int last_percent;
    bool draw;
    clock_t start;
} Progress;

void progress_start(Progress *p, const char *label, size_t total, bool enabled);
void progress_update(Progress *p, size_t done);

void progress_finish(Progress *p);

#endif
