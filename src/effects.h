#ifndef EFFECTS_H
#define EFFECTS_H

/* Piano body (12 dB/octave). */
void effects_body_eq(float *x, long n, double body_boost, double cut_hz);

/* Small Schroeder reverb: 4 comb filters + 2 all-pass filters per channel. */
void effects_reverb(float *left, float *right, long n, double wet);

#endif
