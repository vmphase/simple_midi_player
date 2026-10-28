#include "progress.h"

#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

#define BAR_WIDTH 24

static double seconds_since(clock_t start) {
    return (double)(clock() - start) / CLOCKS_PER_SEC;
}

static void draw(const Progress *p, int percent) {
    char bar[BAR_WIDTH + 1];
    int filled = percent * BAR_WIDTH / 100;
    for (int i = 0; i < BAR_WIDTH; i++)
        bar[i] = i < filled ? '#' : '-';
    bar[BAR_WIDTH] = '\0';

    printf("\r%s [%s] %3d%%", p->label, bar, percent);

    double elapsed = seconds_since(p->start);
    if (percent >= 5 && percent < 100)
        printf("  ~%.0fs left", elapsed * (100 - percent) / percent);
    printf("      ");
    fflush(stdout);
}

void progress_start(Progress *p, const char *label, size_t total,
                    bool enabled) {
    *p = (Progress){.label = label,
                    .total = total,
                    .last_percent = -1,
                    .draw = enabled && isatty(fileno(stdout)),
                    .start = clock()};
    progress_update(p, 0);
}

void progress_update(Progress *p, size_t done) {
    if (!p->draw)
        return;
    int percent = p->total ? (int)(done * 100 / p->total) : 100;
    if (percent == p->last_percent)
        return;
    p->last_percent = percent;
    draw(p, percent);
}

void progress_finish(Progress *p) {
    if (!p->draw)
        return;
    printf("\r%s done in %.1fs                                        \n",
           p->label, seconds_since(p->start));
}
