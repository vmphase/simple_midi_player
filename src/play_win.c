#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h> /* needs -lwinmm */

#include "player.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif

bool player_play_wav(const char *path) {
    return PlaySoundA(path, NULL, SND_FILENAME | SND_SYNC) != 0;
}

/* One MIDI message with the time at which to send it. */
typedef struct {
    double time;
    int order; /* note-off (0), pedal (1), note-on (2) */
    DWORD msg;
} TimedMsg;

static int compare_msgs(const void *a, const void *b) {
    const TimedMsg *x = a, *y = b;
    if (x->time != y->time)
        return x->time < y->time ? -1 : 1;
    return x->order - y->order;
}

static void add_msg(TimedMsg **list, size_t *len, double time, int order,
                    DWORD msg) {
    if (!isfinite(time))
        return;
    (*list)[(*len)++] = (TimedMsg){time, order, msg};
}

bool player_play_gm(const Score *score) {
    size_t cap = score->notes.len * 2 + score->pedal.len * 2;
    TimedMsg *msgs = malloc((cap ? cap : 1) * sizeof *msgs);
    if (!msgs)
        return false;
    size_t n = 0;

    /* Without pedal data, let notes ring a little after the key is released. */
    double extra = score->pedal.len ? 0.0 : 0.25;
    for (size_t i = 0; i < score->notes.len; i++) {
        const Note *note = &score->notes.items[i];
        int vel = (int)(note->vel * 127.0 + 0.5);
        vel = vel < 1 ? 1 : (vel > 127 ? 127 : vel);
        add_msg(&msgs, &n, note->start, 2,
                0x90 | (DWORD)note->midi << 8 | (DWORD)vel << 16);
        add_msg(&msgs, &n, note->start + note->dur + extra, 0,
                0x80 | (DWORD)note->midi << 8);
    }
    for (size_t i = 0; i < score->pedal.len; i++) {
        add_msg(&msgs, &n, score->pedal.items[i].down, 1,
                0xB0 | 64 << 8 | 127 << 16);
        add_msg(&msgs, &n, score->pedal.items[i].up, 1, 0xB0 | 64 << 8);
    }
    qsort(msgs, n, sizeof *msgs, compare_msgs);

    HMIDIOUT out;
    if (midiOutOpen(&out, MIDI_MAPPER, 0, 0, CALLBACK_NULL) !=
        MMSYSERR_NOERROR) {
        fprintf(stderr, "Error: cannot open the Windows MIDI synth\n");
        free(msgs);
        return false;
    }
    midiOutShortMsg(out, 0xC0);
    midiOutShortMsg(out, 0xB0 | 91 << 8 | 40 << 16); /* a little reverb */

    timeBeginPeriod(1); /* 1 ms timer resolution for accurate note timing */
    Sleep(300);
    DWORD t0 = timeGetTime();
    for (size_t i = 0; i < n; i++) {
        for (;;) {
            double wait = msgs[i].time - (timeGetTime() - t0) / 1000.0;
            if (wait <= 0)
                break;
            if (wait > 0.003)
                Sleep((DWORD)((wait - 0.002) * 1000.0));
        }
        midiOutShortMsg(out, msgs[i].msg);
    }
    Sleep(3500); /* let the last notes ring out */
    midiOutReset(out);
    midiOutClose(out);
    timeEndPeriod(1);

    free(msgs);
    return true;
}

#endif
