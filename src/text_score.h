#ifndef TEXT_SCORE_H
#define TEXT_SCORE_H

#include "score.h"

/* Loads a text score file into `score`.  */
void text_score_load(Score *score, const char *path, double default_bpm,
                     double humanize_ms);

#endif
