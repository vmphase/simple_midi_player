#ifndef OPTIONS_H
#define OPTIONS_H

#include <stdbool.h>

typedef struct {
    const char *input;  /* .mid/.midi file or text score (required) */
    const char *output; /* WAV file to write; default: input name with .wav */

    double bpm;         /* default tempo of text scores */
    double speed;       /* tempo multiplier, applies to MIDI too */
    double humanize_ms; /* random timing wobble of text scores */
    double start, end;  /* render only [start, end) seconds; end <= 0 = all */
    int transpose;      /* semitones */

    double reverb;     /* wet amount, 0 = dry */
    double tone_dark;  /* >1 warmer, <1 brighter */
    double body_boost; /* low-end warmth */
    double cut_hz;     /* treble roll-off */
    double prompt_tau; /* attack decay speed, smaller = more percussive */

    double tail; /* seconds of silence kept after the last note */
    double gain; /* peak level after normalizing, 0..1 */
    bool mono;

    unsigned seed;
    bool info;  /* print score statistics and exit */
    bool quiet; /* no progress output */
    bool play;  /* play the WAV when finished */
    bool gm;    /* Windows only: play through the GM synth, no WAV */
} Options;

bool options_parse(Options *opt, int argc, char **argv);

#endif
