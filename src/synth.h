#ifndef SYNTH_H
#define SYNTH_H

#include "score.h"

typedef struct {
    double tone_dark;  /* >1 warmer, <1 brighter */
    double prompt_tau; /* initial decay of a note, smaller = more percussive */
    double release;    /* damper fall time after the key/pedal is released */
} SynthParams;

/* Adds one piano note to the stereo buffers (`total` samples each). */
void synth_render_note(const Note *note, const SynthParams *params, float *left,
                       float *right, long total);

#endif
