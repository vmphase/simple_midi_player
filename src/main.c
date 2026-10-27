#include "common.h"
#include "midi.h"
#include "options.h"
#include "score.h"

#include <stdio.h>

int main(int argc, char **argv) {
    Options opt;
    if (!options_parse(&opt, argc, argv))
        return 0;

    Score score = {0};
    midi_load(&score, opt.input);
    if (!opt.quiet)
        printf("%s: %lu notes, %lu pedal span(s)\n", opt.input,
               (unsigned long)score.notes.len, (unsigned long)score.pedal.len);
    score_free(&score);
    return 0;
}
