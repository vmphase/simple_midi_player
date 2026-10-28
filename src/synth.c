#include "synth.h"
#include "common.h"

#include <math.h>
#include <stdlib.h>

#define AUDIBLE_FLOOR 5e-5

static const double PROMPT_WEIGHT = 0.75;
static const double AFTER_WEIGHT = 0.25;

static const double DETUNE_3_STRINGS[3] = {-0.0004, 0.0003, 0.0001};
static const double DETUNE_2_STRINGS[2] = {-0.00025, 0.00025};

/* Sums all partials of the note into `buf`. */
static void add_partials(double *buf, long len, const Note *note,
                         const SynthParams *p, double f0, double t60) {
    const double inharmonicity = 2e-4 * exp(0.05 * (note->midi - 40));
    /* decay rate that gives -60 dB after t60 seconds (ln 1000 = 6.9078) */
    const double base_rate = 6.9078 / t60;
    const double tilt = p->tone_dark * (0.34 - 0.16 * note->vel);
    const double tau =
        fmax(p->prompt_tau * exp(-0.022 * (note->midi - 36)), 0.15);

    for (int n = 1; n <= 40; n++) {
        double freq = n * f0 * sqrt(1.0 + inharmonicity * n * n);
        if (freq > 16000.0)
            break;

        /* The hammer hits 1/8 of the way along the string, so partials with a
         * node there (every 8th) are weak. Higher partials fade with 1/n. */
        double amp =
            (fabs(sin(PI * n * 0.125)) * 0.8 + 0.2) / n * exp(-(n - 1) * tilt);
        if (n == 1)
            amp *= 1.0 + 0.6 / (1.0 + f0 / 300.0);

        double slow_rate = base_rate * (1.0 + 0.3 * (n - 1));
        double fast_tau =
            tau / (1.0 + 0.45 * (n - 1)); /* upper ones die first */

        /* time until both decay stages fall below the audible floor */
        double slow_time =
            amp * AFTER_WEIGHT > AUDIBLE_FLOOR
                ? log(amp * AFTER_WEIGHT / AUDIBLE_FLOOR) / slow_rate
                : 0;
        double fast_time =
            amp * PROMPT_WEIGHT > AUDIBLE_FLOOR
                ? fast_tau * log(amp * PROMPT_WEIGHT / AUDIBLE_FLOOR)
                : 0;
        double active_time = fmax(slow_time, fast_time);
        if (active_time <= 0)
            continue;

        long plen = (long)(active_time * SAMPLE_RATE) + 1;
        if (plen > len)
            plen = len;

        /* per-sample multipliers of the two exponential envelopes */
        double fast_decay = exp(-1.0 / (fast_tau * SAMPLE_RATE));
        double slow_decay = exp(-slow_rate / SAMPLE_RATE);

        int strings = (n <= 6) ? 3 : (n <= 12 ? 2 : 1);
        double string_amp = amp / strings;

        for (int s = 0; s < strings; s++) {
            double detune = strings == 3   ? DETUNE_3_STRINGS[s]
                            : strings == 2 ? DETUNE_2_STRINGS[s]
                                           : 0.0;
            /* Sine oscillator by rotating a point on the unit circle: much
             * cheaper than calling sin() for every sample. */
            double w = 2.0 * PI * freq * (1.0 + detune) / SAMPLE_RATE;
            double cos_w = cos(w), sin_w = sin(w);

            double phase = PI / 2 + (frand() - 0.5) * 0.6;
            double c = cos(phase), sn = sin(phase);
            double fast = 1.0, slow = 1.0;

            for (long i = 0; i < plen; i++) {
                buf[i] += string_amp * sn *
                          (PROMPT_WEIGHT * fast + AFTER_WEIGHT * slow);
                double next_c = c * cos_w - sn * sin_w;
                sn = sn * cos_w + c * sin_w;
                c = next_c;
                fast *= fast_decay;
                slow *= slow_decay;
            }
        }
    }
}

static void add_hammer_knock(double *buf, long len, double vel) {
    long knock_len = (long)(0.025 * SAMPLE_RATE);
    if (knock_len > len)
        knock_len = len;
    double noise = 0;
    for (long i = 0; i < knock_len; i++) {
        noise += 0.12 * ((frand() * 2 - 1) - noise);
        buf[i] += noise * 0.16 * vel * exp(-i / (0.004 * SAMPLE_RATE));
    }
}

void synth_render_note(const Note *note, const SynthParams *p, float *left,
                       float *right, long total) {
    double f0 = 440.0 * exp2((note->midi - 69) / 12.0);
    double t60 = fmax(30.0 * exp(-0.035 * (note->midi - 40)), 1.2);
    double life = fmin(note->dur + p->release * 7.0, t60);

    long start = (long)(note->start * SAMPLE_RATE);
    long len = (long)(life * SAMPLE_RATE);
    if (start >= total || len <= 0)
        return;
    if (start + len > total)
        len = total - start;

    double *buf = xcalloc((size_t)len, sizeof *buf);
    add_partials(buf, len, note, p, f0, t60);
    add_hammer_knock(buf, len, note->vel);

    /* low notes slightly to the left, high ones to the right (equal power) */
    double pan = clampd((note->midi - 60) / 60.0 * 0.5, -0.5, 0.5);
    double gain_l = cos((pan + 1.0) * PI / 4.0);
    double gain_r = sin((pan + 1.0) * PI / 4.0);
    double loudness = pow(note->vel, 1.6) * 0.5;

    long attack =
        (long)(0.0006 * SAMPLE_RATE); /* avoids a click at the start */
    long fade_out = (long)(0.05 * SAMPLE_RATE); /* ...and at the cut-off */
    for (long i = 0; i < len; i++) {
        double t = (double)i / SAMPLE_RATE;
        double env = 1.0;
        if (i < attack)
            env = (double)i / attack;
        if (t > note->dur)
            env *= exp(-(t - note->dur) / p->release);
        if (len - i < fade_out)
            env *= (double)(len - i) / fade_out;

        double v = buf[i] * env * loudness;
        left[start + i] += (float)(v * gain_l);
        right[start + i] += (float)(v * gain_r);
    }
    free(buf);
}
