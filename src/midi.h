#ifndef MIDI_H
#define MIDI_H

#include "score.h"

/* Loads a Standard MIDI File (0 and 1). Drums (ch 10) are skipped */
void midi_load(Score *score, const char *path);

#endif
