#ifndef WAV_H
#define WAV_H

/* Writes 16-bit PCM at SAMPLE_RATE. Samples are floats in -1..1 (clipped). */
void wav_write(const char *path, const float *left, const float *right,
               long frames);

#endif
