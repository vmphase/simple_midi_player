#ifndef PLAYER_H
#define PLAYER_H

#include "score.h"

#include <stdbool.h>

/* Plays a WAV file and waits until it ends.
 * - Windows: winmm PlaySound
 * - macOS: afplay
 * - Linux: the first of paplay, pw-play, aplay, ffplay, play that works
 * Returns false if nothing could play it. */
bool player_play_wav(const char *path);

/* Windows only: plays the score live through the built-in "Microsoft GS
 * Wavetable Synth" (a sampled grand piano). Returns false elsewhere. */
bool player_play_gm(const Score *score);

#endif
