#ifndef SCORE_H
#define SCORE_H

#include "common.h"

#include <stdbool.h>

/* One note, with all times in seconds. */
typedef struct {
    int midi;     /* 60 = middle C */
    double vel;   /* 0..1 */
    double start; /* seconds */
    double dur;   /* seconds the key is held */
    bool grace;   /* short ornament that sounds just before the beat */
} Note;

/* A stretch of time during which the sustain pedal is held down. */
typedef struct {
    double down, up; /* up is INFINITY if the pedal is never released */
} PedalSpan;

typedef Vec(Note) NoteVec;
typedef Vec(PedalSpan) PedalVec;

typedef struct {
    NoteVec notes;
    PedalVec pedal;
    double release; /* how fast a released key's sound dies */
    double length;  /* end of the last note, in seconds */
    bool pedal_is_down;
    double pedal_since;
} Score;

void score_add_note(Score *s, Note note);

/* Feed pedal events in chronological order while loading. */
void score_pedal_event(Score *s, double time, bool down);
void score_pedal_close(Score *s);

/* Shift every note by `semitones`.*/
void score_transpose(Score *s, int semitones);

/* Multiply all times by `factor` (0.5 = twice as fast). */
void score_scale_time(Score *s, double factor);

/* Keep notes starting in [from, to) and move them so `from` becomes 0.
 * to <= 0 means "until the end". */
void score_trim(Score *s, double from, double to);

void score_finish(Score *s);
void score_free(Score *s);

#endif
