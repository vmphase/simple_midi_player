#include "common.h"
#include "effects.h"
#include "midi.h"
#include "options.h"
#include "player.h"
#include "progress.h"
#include "score.h"
#include "synth.h"
#include "text_score.h"
#include "wav.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool has_extension(const char *path, const char *ext) {
    size_t a = strlen(path), b = strlen(ext);
    if (a < b)
        return false;
    for (size_t i = 0; i < b; i++)
        if (tolower((unsigned char)path[a - b + i]) != ext[i])
            return false;
    return true;
}

static void note_name(int midi, char *out, size_t size) {
    static const char *const names[12] = {"C",  "C#", "D",  "D#", "E",  "F",
                                          "F#", "G",  "G#", "A",  "A#", "B"};
    snprintf(out, size, "%s%d", names[midi % 12], midi / 12 - 1);
}

static void print_info(const Score *s) {
    int lowest = 127, highest = 0;
    for (size_t i = 0; i < s->notes.len; i++) {
        int m = s->notes.items[i].midi;
        if (m < lowest)
            lowest = m;
        if (m > highest)
            highest = m;
    }
    char lo[16], hi[16];
    note_name(lowest, lo, sizeof lo);
    note_name(highest, hi, sizeof hi);

    printf("notes:    %lu\n", (unsigned long)s->notes.len);
    printf("range:    %s (%d) .. %s (%d)\n", lo, lowest, hi, highest);
    printf("length:   %.2f seconds\n", s->length);
    printf("pedal:    %lu press(es)\n", (unsigned long)s->pedal.len);
}

static void normalize(float *left, float *right, long n, double target) {
    float peak = 0;
    for (long i = 0; i < n; i++) {
        peak = fmaxf(peak, fabsf(left[i]));
        if (right)
            peak = fmaxf(peak, fabsf(right[i]));
    }
    if (peak <= 0)
        return;
    float gain = (float)target / peak;
    for (long i = 0; i < n; i++) {
        left[i] *= gain;
        if (right)
            right[i] *= gain;
    }
}

int main(int argc, char **argv) {
    Options opt;
    if (!options_parse(&opt, argc, argv))
        return 0;
    srand(opt.seed);

    Score score = {0};
    if (opt.input &&
        (has_extension(opt.input, ".mid") || has_extension(opt.input, ".midi")))
        midi_load(&score, opt.input);
    else
        text_score_load(&score, opt.input, opt.bpm, opt.humanize_ms);
    if (score.notes.len == 0)
        die("no notes found in %s", opt.input);

    if (opt.transpose)
        score_transpose(&score, opt.transpose);
    if (opt.speed != 1.0)
        score_scale_time(&score, 1.0 / opt.speed);
    if (opt.start > 0 || opt.end > 0)
        score_trim(&score, opt.start, opt.end);
    if (score.notes.len == 0)
        die("no notes left after --transpose/--start/--end");
    score_finish(&score);

    if (opt.info) {
        print_info(&score);
        score_free(&score);
        return 0;
    }
    if (!opt.quiet)
        printf("Loaded %s: %lu notes, %.1f seconds\n", opt.input,
               (unsigned long)score.notes.len, score.length);

    if (opt.gm) {
        if (!opt.quiet)
            printf("Playing through the Windows GM piano (no WAV is "
                   "written)...\n");
        bool ok = player_play_gm(&score);
        score_free(&score);
        return ok ? 0 : 1;
    }

    long total = (long)((score.length + opt.tail) * SAMPLE_RATE);
    float *left = xcalloc((size_t)total, sizeof *left);
    float *right = xcalloc((size_t)total, sizeof *right);

    SynthParams synth = {
        .tone_dark = opt.tone_dark,
        .prompt_tau = opt.prompt_tau,
        .release = score.release,
    };
    Progress progress;
    progress_start(&progress, "Rendering", score.notes.len, !opt.quiet);
    for (size_t i = 0; i < score.notes.len; i++) {
        synth_render_note(&score.notes.items[i], &synth, left, right, total);
        progress_update(&progress, i + 1);
    }
    progress_finish(&progress);

    if (!opt.quiet)
        printf("Mixing (EQ%s)...\n", opt.reverb > 0 ? ", reverb" : "");
    effects_body_eq(left, total, opt.body_boost, opt.cut_hz);
    effects_body_eq(right, total, opt.body_boost, opt.cut_hz);
    if (opt.reverb > 0)
        effects_reverb(left, right, total, opt.reverb);
    if (opt.mono)
        for (long i = 0; i < total; i++)
            left[i] = 0.5f * (left[i] + right[i]);
    normalize(left, opt.mono ? NULL : right, total, opt.gain);

    wav_write(opt.output, left, opt.mono ? NULL : right, total);
    if (!opt.quiet) {
        double mb = (44.0 + (double)total * (opt.mono ? 2 : 4)) / 1e6;
        printf("Saved %s (%.1f MB, %.1f s of audio)\n", opt.output, mb,
               (double)total / SAMPLE_RATE);
    }

    free(left);
    free(right);
    score_free(&score);

    if (opt.play) {
        if (!opt.quiet)
            printf("Playing %s (Ctrl+C to stop)...\n", opt.output);
        if (!player_play_wav(opt.output)) {
            fprintf(stderr,
                    "Warning: no audio player found or it failed; open %s "
                    "with any player.\n",
                    opt.output);
            return 1;
        }
    }
    return 0;
}
