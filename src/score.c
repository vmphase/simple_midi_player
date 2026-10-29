#include "score.h"

#include <math.h>
#include <stdio.h>

static const double RELEASE_NO_PEDAL = 0.30;
static const double RELEASE_DAMPER = 0.12;

void score_add_note(Score *s, Note note) {
    note.vel = fmin(note.vel, 1.0);
    vec_push(s->notes, note);
}

void score_pedal_event(Score *s, double time, bool down) {
    if (down && !s->pedal_is_down) {
        s->pedal_is_down = true;
        s->pedal_since = time;
    } else if (!down && s->pedal_is_down) {
        PedalSpan span = {s->pedal_since, time};
        vec_push(s->pedal, span);
        s->pedal_is_down = false;
    }
}

void score_pedal_close(Score *s) {
    if (s->pedal_is_down) {
        PedalSpan span = {s->pedal_since, INFINITY};
        vec_push(s->pedal, span);
        s->pedal_is_down = false;
    }
}

void score_transpose(Score *s, int semitones) {
    size_t kept = 0;
    for (size_t i = 0; i < s->notes.len; i++) {
        Note n = s->notes.items[i];
        n.midi += semitones;
        if (n.midi >= 0 && n.midi <= 127)
            s->notes.items[kept++] = n;
    }
    if (kept < s->notes.len)
        fprintf(stderr,
                "Warning: %lu notes left the piano range and were "
                "dropped.\n",
                (unsigned long)(s->notes.len - kept));
    s->notes.len = kept;
}

void score_scale_time(Score *s, double factor) {
    for (size_t i = 0; i < s->notes.len; i++) {
        s->notes.items[i].start *= factor;
        s->notes.items[i].dur *= factor;
    }
    for (size_t i = 0; i < s->pedal.len; i++) {
        s->pedal.items[i].down *= factor;
        s->pedal.items[i].up *= factor;
    }
}

void score_trim(Score *s, double from, double to) {
    size_t kept = 0;
    for (size_t i = 0; i < s->notes.len; i++) {
        Note n = s->notes.items[i];
        if (n.start < from || (to > 0 && n.start >= to))
            continue;
        n.start -= from;
        s->notes.items[kept++] = n;
    }
    s->notes.len = kept;

    for (size_t i = 0; i < s->pedal.len; i++) {
        s->pedal.items[i].down -= from;
        s->pedal.items[i].up -= from;
    }
}

void score_finish(Score *s) {
    s->length = 0;
    for (size_t i = 0; i < s->notes.len; i++) {
        const Note *n = &s->notes.items[i];
        s->length = fmax(s->length, n->start + n->dur);
    }

    s->release = s->pedal.len ? RELEASE_DAMPER : RELEASE_NO_PEDAL;

    for (size_t i = 0; i < s->notes.len; i++) {
        Note *n = &s->notes.items[i];
        if (n->grace)
            continue;
        double off = n->start + n->dur;
        for (size_t k = 0; k < s->pedal.len; k++) {
            const PedalSpan *p = &s->pedal.items[k];
            if (p->down <= off && off < p->up) {
                n->dur = p->up - n->start;
                break;
            }
        }
    }
}

void score_free(Score *s) {
    vec_free(s->notes);
    vec_free(s->pedal);
}
