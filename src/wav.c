#include "wav.h"
#include "common.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

/* WAV files are little-endian whatever the CPU is, so write byte by byte. */
static void put_le(FILE *f, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; i++)
        fputc((value >> (8 * i)) & 0xFF, f);
}

static void put_sample(FILE *f, float x) {
    long v = lrintf(x * 32767.0f);
    if (v > 32767)
        v = 32767;
    if (v < -32768)
        v = -32768;
    put_le(f, (uint32_t)(v & 0xFFFF), 2);
}

void wav_write(const char *path, const float *left, const float *right,
               long frames) {
    FILE *f = fopen(path, "wb");
    if (!f)
        die("cannot open output file '%s'", path);

    int channels = right ? 2 : 1;
    uint32_t data_size = (uint32_t)(frames * channels * 2);

    fwrite("RIFF", 1, 4, f);
    put_le(f, 36 + data_size, 4);
    fwrite("WAVEfmt ", 1, 8, f);
    put_le(f, 16, 4); /* size of the fmt chunk */
    put_le(f, 1, 2);  /* PCM */
    put_le(f, (uint32_t)channels, 2);
    put_le(f, SAMPLE_RATE, 4);
    put_le(f, (uint32_t)(SAMPLE_RATE * channels * 2), 4); /* bytes per second */
    put_le(f, (uint32_t)(channels * 2), 2);               /* bytes per frame */
    put_le(f, 16, 2);                                     /* bits per sample */
    fwrite("data", 1, 4, f);
    put_le(f, data_size, 4);

    for (long i = 0; i < frames; i++) {
        put_sample(f, left[i]);
        if (right)
            put_sample(f, right[i]);
    }

    if (ferror(f) || fclose(f) != 0)
        die("failed writing '%s'", path);
}
