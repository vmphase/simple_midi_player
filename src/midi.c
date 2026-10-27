#include "midi.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* At the same tick, pedal changes go first, then note-offs, then note-ons. */
enum { EV_PEDAL = 1, EV_NOTE_OFF = 2, EV_NOTE_ON = 3 };

typedef struct {
    unsigned long tick;
    int type, channel, key, value;
} MidiEvent;

typedef struct {
    unsigned long tick;
    double us_per_beat;
    double sec; /* time of this tempo change, filled in later */
} TempoChange;

typedef struct {
    Vec(MidiEvent) events;
    Vec(TempoChange) tempos;
    int division; /* ticks per quarter note */
} Midi;

static unsigned long be32(const uint8_t *p) {
    return ((unsigned long)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static unsigned be16(const uint8_t *p) { return (p[0] << 8) | p[1]; }

/* Variable-length quantity: 7 bits per byte, high bit = "more follows". */
static unsigned long read_vlq(const uint8_t **pp, const uint8_t *end) {
    unsigned long v = 0;
    int c;
    do {
        if (*pp >= end)
            return v;
        c = *(*pp)++;
        v = (v << 7) | (c & 0x7F);
    } while (c & 0x80);
    return v;
}

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f)
        die("cannot open MIDI file '%s'", path);
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0)
        die("cannot read '%s'", path);

    uint8_t *buf = xcalloc((size_t)n, 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n)
        die("read error in '%s'", path);
    fclose(f);
    *size = (size_t)n;
    return buf;
}

static void add_event(Midi *m, unsigned long tick, int type, int ch, int key,
                      int value) {
    MidiEvent ev = {tick, type, ch, key, value};
    vec_push(m->events, ev);
}

static void parse_track(Midi *m, const uint8_t *q, const uint8_t *end,
                        bool *first_tempo_is_default) {
    unsigned long tick = 0;
    int running = 0;

    while (q < end) {
        tick += read_vlq(&q, end);
        if (q >= end)
            break;

        int status = *q;
        if (status & 0x80) {
            q++;
            if (status < 0xF0)
                running = status;
        } else {
            status = running;
        }

        if (status == 0xFF) { /* meta event */
            if (q >= end)
                break;
            int type = *q++;
            unsigned long len = read_vlq(&q, end);
            if (len > (unsigned long)(end - q))
                break;
            if (type == 0x51 && len == 3) { /* set tempo */
                double us = (q[0] << 16) | (q[1] << 8) | q[2];
                if (tick == 0 && *first_tempo_is_default) {
                    m->tempos.items[0].us_per_beat = us;
                    *first_tempo_is_default = false;
                } else {
                    TempoChange t = {tick, us, 0};
                    vec_push(m->tempos, t);
                }
            }
            q += len;
        } else if (status == 0xF0 || status == 0xF7) { /* sysex */
            unsigned long len = read_vlq(&q, end);
            if (len > (unsigned long)(end - q))
                break;
            q += len;
        } else {
            int kind = status & 0xF0, ch = status & 0x0F;
            int data_bytes = (kind == 0xC0 || kind == 0xD0) ? 1 : 2;
            if (end - q < data_bytes)
                break;

            if (kind == 0x80) {
                add_event(m, tick, EV_NOTE_OFF, ch, q[0], 0);
            } else if (kind == 0x90) {
                /* with velocity 0 means note-off */
                if (q[1] == 0)
                    add_event(m, tick, EV_NOTE_OFF, ch, q[0], 0);
                else
                    add_event(m, tick, EV_NOTE_ON, ch, q[0], q[1]);
            } else if (kind == 0xB0 && q[0] == 64) { /* sustain pedal */
                add_event(m, tick, EV_PEDAL, ch, 0, q[1]);
            }
            q += data_bytes;
        }
    }
}

static int compare_events(const void *a, const void *b) {
    const MidiEvent *x = a, *y = b;
    if (x->tick != y->tick)
        return x->tick < y->tick ? -1 : 1;
    return x->type - y->type;
}

static void build_tempo_map(Midi *m) {
    TempoChange *t = m->tempos.items;
    for (size_t i = 1; i < m->tempos.len; i++) {
        TempoChange cur = t[i];
        size_t j = i;
        while (j > 0 && t[j - 1].tick > cur.tick) {
            t[j] = t[j - 1];
            j--;
        }
        t[j] = cur;
    }
    t[0].sec = 0;
    for (size_t i = 1; i < m->tempos.len; i++)
        t[i].sec = t[i - 1].sec + (double)(t[i].tick - t[i - 1].tick) *
                                      t[i - 1].us_per_beat / 1e6 / m->division;
}

void midi_load(Score *score, const char *path) {
    size_t size;
    uint8_t *buf = read_file(path, &size);
    if (size < 14 || memcmp(buf, "MThd", 4))
        die("'%s' is not a MIDI file", path);

    Midi m = {0};
    unsigned header_len = (unsigned)be32(buf + 4);
    int ntracks = (int)be16(buf + 10);
    m.division = (int)be16(buf + 12);
    if (m.division & 0x8000)
        die("SMPTE time division is not supported (%s)", path);

    TempoChange initial = {0, 500000, 0};
    vec_push(m.tempos, initial);
    bool first_tempo_is_default = true;

    const uint8_t *end = buf + size, *p = buf + 8 + header_len;
    for (int t = 0; t < ntracks && p + 8 <= end; t++) {
        if (memcmp(p, "MTrk", 4))
            break;
        unsigned long len = be32(p + 4);
        const uint8_t *track_end = p + 8 + len;
        if (len > (unsigned long)(end - (p + 8)))
            track_end = end;
        parse_track(&m, p + 8, track_end, &first_tempo_is_default);
        p = track_end;
    }
    free(buf);

    build_tempo_map(&m);
    qsort(m.events.items, m.events.len, sizeof(MidiEvent), compare_events);

    static double on_time[16][128], on_vel[16][128];
    static bool active[16][128];
    size_t seg = 0;
    double last_time = 0;

    for (size_t i = 0; i < m.events.len; i++) {
        const MidiEvent *ev = &m.events.items[i];
        while (seg + 1 < m.tempos.len &&
               m.tempos.items[seg + 1].tick <= ev->tick)
            seg++;
        const TempoChange *tc = &m.tempos.items[seg];
        double t = tc->sec + (double)(ev->tick - tc->tick) * tc->us_per_beat /
                                 1e6 / m.division;
        last_time = t;

        int ch = ev->channel, key = ev->key & 127;
        if (ev->type == EV_PEDAL) {
            score_pedal_event(score, t, ev->value >= 64);
            continue;
        }

        if (ev->type == EV_NOTE_ON && active[ch][key]) {
            /* same key struck again before release: end the old note */
            Note n = {key, on_vel[ch][key], on_time[ch][key],
                      t - on_time[ch][key], false};
            score_add_note(score, n);
        }
        if (ev->type == EV_NOTE_OFF && active[ch][key]) {
            Note n = {key, on_vel[ch][key], on_time[ch][key],
                      t - on_time[ch][key], false};
            score_add_note(score, n);
            active[ch][key] = false;
        } else if (ev->type == EV_NOTE_ON) {
            active[ch][key] = true;
            on_time[ch][key] = t;
            on_vel[ch][key] = ev->value / 127.0;
        }
    }

    /* notes that never got a note-off last until the end */
    for (int c = 0; c < 16; c++)
        for (int k = 0; k < 128; k++)
            if (active[c][k]) {
                Note n = {k, on_vel[c][k], on_time[c][k],
                          last_time - on_time[c][k], false};
                score_add_note(score, n);
            }

    /* very short notes are inaudible */
    for (size_t i = 0; i < score->notes.len; i++)
        if (score->notes.items[i].dur < 0.03)
            score->notes.items[i].dur = 0.03;

    score_pedal_close(score);
    vec_free(m.events);
    vec_free(m.tempos);
}
