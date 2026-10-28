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
