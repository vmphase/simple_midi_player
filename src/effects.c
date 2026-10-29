#include "effects.h"
#include "common.h"

#include <math.h>
#include <stdlib.h>

void effects_body_eq(float *x, long n, double body_boost, double cut_hz) {
    /* one-pole filter coefficients: 1 - e^(-2*pi*f/sr) */
    double lowpass = 1.0 - exp(-2.0 * PI * cut_hz / SAMPLE_RATE);
    double lows = 1.0 - exp(-2.0 * PI * 220.0 / SAMPLE_RATE);
    double bass = 0, stage1 = 0, stage2 = 0;

    for (long i = 0; i < n; i++) {
        bass += lows * (x[i] - bass); /* everything below ~220 Hz */
        double v = x[i] + body_boost * bass;
        stage1 += lowpass * (v - stage1);
        stage2 += lowpass * (stage1 - stage2);
        x[i] = (float)stage2;
    }
}

/* Feedback delay line with a low-pass in the loop. */
typedef struct {
    float *buf;
    int size, pos;
    float feedback, damp, lp;
} Comb;

/* Delay line that smears the echoes without changing the tone. */
typedef struct {
    float *buf;
    int size, pos;
} AllPass;

static Comb comb_new(int size) {
    return (Comb){.buf = xcalloc((size_t)size, sizeof(float)),
                  .size = size,
                  .feedback = 0.82f,
                  .damp = 0.25f};
}

static float comb_process(Comb *c, float in) {
    float out = c->buf[c->pos];
    c->lp = out * (1.0f - c->damp) + c->lp * c->damp;
    c->buf[c->pos] = in + c->lp * c->feedback;
    if (++c->pos >= c->size)
        c->pos = 0;
    return out;
}

static AllPass allpass_new(int size) {
    return (AllPass){.buf = xcalloc((size_t)size, sizeof(float)), .size = size};
}

static float allpass_process(AllPass *a, float in) {
    float delayed = a->buf[a->pos];
    a->buf[a->pos] = in + delayed * 0.5f;
    if (++a->pos >= a->size)
        a->pos = 0;
    return delayed - in;
}

void effects_reverb(float *left, float *right, long n, double wet) {
    /* Delay lengths in samples; prime-ish numbers avoid metallic ringing.
     * The right channel is a bit longer for stereo width. */
    static const int comb_sizes[4] = {1557, 1617, 1491, 1422};
    Comb comb_l[4], comb_r[4];
    for (int k = 0; k < 4; k++) {
        comb_l[k] = comb_new(comb_sizes[k]);
        comb_r[k] = comb_new(comb_sizes[k] + 23);
    }
    AllPass ap_l[2] = {allpass_new(225), allpass_new(556)};
    AllPass ap_r[2] = {allpass_new(248), allpass_new(579)};

    const float wet_f = (float)wet;
    for (long i = 0; i < n; i++) {
        float in_l = left[i] * 0.12f, in_r = right[i] * 0.12f;
        float rev_l = 0, rev_r = 0;
        for (int k = 0; k < 4; k++) {
            rev_l += comb_process(&comb_l[k], in_l);
            rev_r += comb_process(&comb_r[k], in_r);
        }
        rev_l = allpass_process(&ap_l[1], allpass_process(&ap_l[0], rev_l));
        rev_r = allpass_process(&ap_r[1], allpass_process(&ap_r[0], rev_r));

        left[i] = left[i] * (1.0f - wet_f * 0.5f) + rev_l * wet_f * 2.0f;
        right[i] = right[i] * (1.0f - wet_f * 0.5f) + rev_r * wet_f * 2.0f;
    }

    for (int k = 0; k < 4; k++) {
        free(comb_l[k].buf);
        free(comb_r[k].buf);
    }
    for (int k = 0; k < 2; k++) {
        free(ap_l[k].buf);
        free(ap_r[k].buf);
    }
}
