#include "text_score.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const double GRACE_LEAD = 0.07; /* grace note sounds this early (s) */
static const double GRACE_LEN = 0.14;  /* ...and lasts this long (s) */

/* Everything below is measured in beats until the very end, when the tempo
 * map turns beats into seconds. */
typedef struct {
    int midi;
    double start, dur, vel;
    bool grace;
} BeatNote;

typedef struct {
    double beat;
    double bpm0, bpm1; /* tempo before/after; they differ for a ramp */
    double len;        /* ramp length in beats, 0 = instant change */
} TempoEvent;

typedef struct {
    double beat;
    double secs; /* extra silence (fermata) inserted at this beat */
} Pause;

typedef struct {
    double beat;
    bool down;
} PedalEvent;

/* A piece of the timeline with a constant or linearly changing tempo. */
typedef struct {
    double b0, b1;     /* beat range */
    double bpm0, bpm1; /* tempo at both ends */
    double s0;         /* time in seconds at b0 */
} Segment;

typedef struct {
    Vec(BeatNote) notes;
    Vec(TempoEvent) tempo;
    Vec(Pause) pauses;
    Vec(PedalEvent) pedals;
    Vec(Segment) segs;
    double voice_end[2]; /* length of the right and left hand in beats */
    int voices;
} Composition;

/* "C#4" -> MIDI number. Octave -1 is MIDI 0..11, so C4 = 60. */
static int parse_pitch(const char *s) {
    static const int semitone[7] = {9, 11, 0, 2, 4, 5, 7}; /* A B C D E F G */
    int letter = toupper((unsigned char)*s) - 'A';
    if (letter < 0 || letter > 6)
        die("bad note letter in '%s'", s);

    int midi = semitone[letter];
    s++;
    if (*s == '#') {
        midi++;
        s++;
    } else if (*s == 'b') {
        midi--;
        s++;
    }
    if (!isdigit((unsigned char)*s) && *s != '-')
        die("missing octave in '%s'", s);
    return 12 * (atoi(s) + 1) + midi;
}

/* Durations are letters (w h q e s t = whole .. 32nd, dots allowed: "q.")
 * or plain beat counts ("1.5"), several of which may be joined with '+'. */
static double parse_duration(const char *s) {
    double total = 0;
    while (*s) {
        double beats = 0;
        if (isdigit((unsigned char)*s) ||
            (*s == '.' && isdigit((unsigned char)s[1]))) {
            char *end;
            beats = strtod(s, &end);
            s = end;
        } else {
            switch (tolower((unsigned char)*s)) {
            case 'w':
                beats = 4;
                break;
            case 'h':
                beats = 2;
                break;
            case 'q':
                beats = 1;
                break;
            case 'e':
                beats = 0.5;
                break;
            case 's':
                beats = 0.25;
                break;
            case 't':
                beats = 0.125;
                break;
            default:
                die("bad duration '%s'", s);
            }
            s++;
            /* each dot adds half of the previous addition */
            double dot = beats / 2;
            while (*s == '.') {
                beats += dot;
                dot /= 2;
                s++;
            }
        }
        total += beats;
        if (*s == '+')
            s++;
    }
    return total;
}

static void add_beat_note(Composition *c, int midi, double start, double dur,
                          double vel, bool grace) {
    BeatNote n = {midi, start, dur, fmin(vel, 1.0), grace};
    vec_push(c->notes, n);
}

/* Parses one hand. Tokens, separated by spaces:
 *   C#4:q  E4+G4:h  R:e    note / chord / rest
 *   gC5                    grace note
 *   v80                    velocity (0..127) for the notes that follow
 *   Pd Pu Pc               pedal down / up / change (up, then down)
 *   T90  T90~60:4          tempo, or a ramp to 60 BPM over 4 beats
 *   ^1.5                   pause of 1.5 seconds at this point
 *   |                      bar line, ignored
 * Tempo, pause and pedal marks are global, so write them in one hand only. */
static void parse_voice(Composition *c, const char *src) {
    double t = 0, vel = 0.7;
    char tok[128];
    const char *p = src;

    while (*p) {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p)
            break;
        size_t n = 0;
        while (*p && !isspace((unsigned char)*p) && n < sizeof tok - 1)
            tok[n++] = *p++;
        tok[n] = '\0';

        if (tok[0] == '|') {
            continue;
        } else if (tok[0] == 'v' && isdigit((unsigned char)tok[1])) {
            vel = atoi(tok + 1) / 127.0;
        } else if (tok[0] == 'T' && isdigit((unsigned char)tok[1])) {
            char *end;
            TempoEvent ev = {.beat = t, .len = 0};
            ev.bpm0 = ev.bpm1 = strtod(tok + 1, &end);
            if (*end == '~') {
                ev.bpm1 = strtod(end + 1, &end);
                if (*end == ':')
                    ev.len = strtod(end + 1, &end);
            }
            vec_push(c->tempo, ev);
        } else if (tok[0] == '^') {
            Pause pause = {t, atof(tok + 1)};
            vec_push(c->pauses, pause);
        } else if (tok[0] == 'P' && strchr("duc", tok[1]) && !tok[2]) {
            if (tok[1] != 'd') {
                PedalEvent up = {t, false};
                vec_push(c->pedals, up);
            }
            if (tok[1] != 'u') {
                PedalEvent down = {t, true};
                vec_push(c->pedals, down);
            }
        } else if (tok[0] == 'g' && tok[1]) {
            add_beat_note(c, parse_pitch(tok + 1), t, 0, vel, true);
        } else {
            char *colon = strchr(tok, ':');
            if (!colon)
                die("expected PITCH:DURATION, got '%s'", tok);
            *colon = '\0';
            double beats = parse_duration(colon + 1);

            if (strcmp(tok, "R") != 0) { /* "R" is a rest */
                /* a chord is several pitches joined with '+' */
                char *pitch = tok;
                while (pitch) {
                    char *plus = strchr(pitch, '+');
                    if (plus)
                        *plus = '\0';
                    add_beat_note(c, parse_pitch(pitch), t, beats, vel, false);
                    pitch = plus ? plus + 1 : NULL;
                }
            }
            t += beats;
        }
    }

    if (c->voices < 2)
        c->voice_end[c->voices++] = t;
}

static int compare_tempo(const void *a, const void *b) {
    double d = ((const TempoEvent *)a)->beat - ((const TempoEvent *)b)->beat;
    return d < 0 ? -1 : d > 0;
}

static int compare_pedal(const void *a, const void *b) {
    const PedalEvent *x = a, *y = b;
    if (x->beat != y->beat)
        return x->beat < y->beat ? -1 : 1;
    return (int)x->down - (int)y->down;
}

/* Seconds from the start of the segment to beat `b`. While the tempo moves
 * linearly with the beat number, a beat takes 60/bpm seconds, so the time is
 * the integral of 60 / (bpm0 + k*x), which is a logarithm. */
static double segment_secs(const Segment *g, double b) {
    double len = g->b1 - g->b0, x = b - g->b0;
    double k = len > 0 ? (g->bpm1 - g->bpm0) / len : 0; /* BPM per beat */
    if (fabs(k) < 1e-12)
        return 60.0 * x / g->bpm0;
    return 60.0 / k * log((g->bpm0 + k * x) / g->bpm0);
}

static void add_segment(Composition *c, double b0, double b1, double bpm0,
                        double bpm1) {
    Segment g = {b0, b1, bpm0, bpm1, 0};
    if (c->segs.len) {
        const Segment *prev = &c->segs.items[c->segs.len - 1];
        g.s0 = prev->s0 + segment_secs(prev, prev->b1);
    }
    vec_push(c->segs, g);
}

static void build_tempo_map(Composition *c, double default_bpm) {
    if (c->tempo.len)
        qsort(c->tempo.items, c->tempo.len, sizeof(TempoEvent), compare_tempo);

    double beat = 0, bpm = default_bpm;
    for (size_t i = 0; i < c->tempo.len; i++) {
        const TempoEvent *ev = &c->tempo.items[i];
        double at = fmax(ev->beat, beat);
        if (at > beat)
            add_segment(c, beat, at, bpm, bpm); /* steady stretch */
        beat = at;
        if (ev->len > 0) {
            add_segment(c, beat, beat + ev->len, ev->bpm0, ev->bpm1);
            beat += ev->len;
        }
        bpm = ev->bpm1;
    }
    add_segment(c, beat, 1e9, bpm, bpm); /* last tempo lasts forever */
}

static double beat_to_sec(const Composition *c, double beat) {
    size_t i = 0;
    while (i < c->segs.len - 1 && beat >= c->segs.items[i].b1)
        i++;
    double secs = c->segs.items[i].s0 + segment_secs(&c->segs.items[i], beat);
    for (size_t k = 0; k < c->pauses.len; k++)
        if (c->pauses.items[k].beat <= beat + 1e-9)
            secs += c->pauses.items[k].secs;
    return secs;
}

static void convert(Composition *c, Score *score, double default_bpm,
                    double humanize_ms) {
    build_tempo_map(c, default_bpm);

    for (size_t i = 0; i < c->notes.len; i++) {
        const BeatNote *bn = &c->notes.items[i];
        double start = beat_to_sec(c, bn->start);
        Note n = {.midi = bn->midi, .vel = bn->vel, .grace = bn->grace};

        if (bn->grace) {
            n.start = start - GRACE_LEAD;
            n.dur = GRACE_LEN;
        } else {
            n.dur = beat_to_sec(c, bn->start + bn->dur) - start;
            n.start = start + (frand() * 2 - 1) * humanize_ms / 1000.0;
            n.vel = fmin(n.vel * (1.0 + (frand() * 2 - 1) * 0.04), 1.0);
        }
        n.start = fmax(n.start, 0);
        score_add_note(score, n);
    }

    if (c->pedals.len)
        qsort(c->pedals.items, c->pedals.len, sizeof(PedalEvent),
              compare_pedal);
    for (size_t i = 0; i < c->pedals.len; i++)
        score_pedal_event(score, beat_to_sec(c, c->pedals.items[i].beat),
                          c->pedals.items[i].down);
    score_pedal_close(score);
}

typedef Vec(char) Text;

static void append_line(Text *text, const char *line) {
    for (; *line; line++)
        vec_push(*text, *line);
    vec_push(*text, ' ');
}

/* Reads "RH:" and "LH:" sections; '#' starts a comment line. */
static void read_score_file(const char *path, Text *rh, Text *lh) {
    FILE *f = fopen(path, "r");
    if (!f)
        die("cannot open score file '%s'", path);

    Text *current = NULL;
    char line[4096];
    while (fgets(line, sizeof line, f)) {
        char *p = line;
        while (isspace((unsigned char)*p))
            p++;
        if (*p == '#' || !*p)
            continue;
        if (!strncmp(p, "RH:", 3)) {
            current = rh;
            p += 3;
        } else if (!strncmp(p, "LH:", 3)) {
            current = lh;
            p += 3;
        }
        if (!current)
            die("line before any RH:/LH: prefix: '%s'", p);
        append_line(current, p);
    }
    fclose(f);

    vec_push(*rh, '\0');
    vec_push(*lh, '\0');
}

void text_score_load(Score *score, const char *path, double default_bpm,
                     double humanize_ms) {
    Composition c = {0};

    Text rh = {0}, lh = {0};
    read_score_file(path, &rh, &lh);
    parse_voice(&c, rh.items);
    parse_voice(&c, lh.items);
    vec_free(rh);
    vec_free(lh);

    if (c.voices == 2 && fabs(c.voice_end[0] - c.voice_end[1]) > 1e-6)
        fprintf(stderr,
                "Warning: right hand ends at beat %.3f but left hand at beat "
                "%.3f - check your transcription.\n",
                c.voice_end[0], c.voice_end[1]);

    convert(&c, score, default_bpm, humanize_ms);

    vec_free(c.notes);
    vec_free(c.tempo);
    vec_free(c.pauses);
    vec_free(c.pedals);
    vec_free(c.segs);
}
