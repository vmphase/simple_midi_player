#include "options.h"
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "1.0.0"

static void print_usage(const char *prog) {
    printf(
        "simple_mid_player " VERSION
        " - renders MIDI files and text scores to a piano WAV\n"
        "\n"
        "Usage: %s [options] <score.mid | score.txt>\n"
        "\n"
        "Output:\n"
        "  -o, --output FILE   output WAV file (default: input name + .wav)\n"
        "      --tail SEC      silence kept after the last note (default "
        "6)\n"
        "      --gain X        peak level after normalizing, 0..1 (default "
        "0.9)\n"
        "      --mono          write a mono file\n"
        "\n"
        "Timing:\n"
        "      --bpm N         tempo of text scores (default 65)\n"
        "      --speed X       tempo multiplier, also for MIDI (default 1)\n"
        "      --humanize MS   random timing wobble of text scores "
        "(default 5)\n"
        "      --transpose N   shift everything by N semitones\n"
        "      --start SEC     render only from this time\n"
        "      --end SEC       render only up to this time\n"
        "\n"
        "Sound:\n"
        "      --reverb X      reverb amount, 0 = dry (default 0.15)\n"
        "      --dark X        >1 warmer, <1 brighter (default 0.5)\n"
        "      --body X        low-end warmth (default 0.5)\n"
        "      --cut HZ        treble roll-off frequency (default 7000)\n"
        "      --tau X         attack decay, smaller = more percussive "
        "(default 1.7)\n"
        "\n"
        "Other:\n"
        "      --seed N        random seed (default 12345)\n"
        "      --info          print score statistics and exit\n"
        "  -q, --quiet         no progress output\n"
        "  -h, --help          show this help\n"
        "  -V, --version       show the version\n",
        prog);
}

static const char *next_arg(int argc, char **argv, int *i) {
    if (*i + 1 >= argc)
        die("option %s needs a value", argv[*i]);
    return argv[++*i];
}

static double to_double(const char *flag, const char *text) {
    char *end;
    double v = strtod(text, &end);
    if (end == text || *end)
        die("invalid number '%s' for %s", text, flag);
    return v;
}

static void check_range(const char *flag, double v, double lo, double hi) {
    if (v < lo || v > hi)
        die("%s must be between %g and %g", flag, lo, hi);
}

/* "dir/song.mid" -> "dir/song.wav" (a new string; lives until exit) */
static const char *default_output(const char *input) {
    size_t len = strlen(input);
    const char *dot = strrchr(input, '.');
    /* ignore dots in directory names and leading dots (".hidden") */
    const char *last_sep = NULL;
    for (const char *p = strpbrk(input, "/\\"); p; p = strpbrk(p + 1, "/\\"))
        last_sep = p;
    if (dot && (!last_sep || dot > last_sep + 1))
        len = (size_t)(dot - input);

    char *out = xcalloc(len + 5, 1);
    memcpy(out, input, len);
    memcpy(out + len, ".wav", 5);
    return out;
}

bool options_parse(Options *opt, int argc, char **argv) {
    *opt = (Options){
        .bpm = 65.0,
        .speed = 1.0,
        .humanize_ms = 5.0,
        .reverb = 0.15,
        .tone_dark = 0.5,
        .body_boost = 0.5,
        .cut_hz = 7000.0,
        .prompt_tau = 1.7,
        .tail = 6.0,
        .gain = 0.9,
        .seed = 12345,
    };

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            print_usage(argv[0]);
            return false;
        } else if (!strcmp(a, "-V") || !strcmp(a, "--version")) {
            puts("simple_mid_player " VERSION);
            return false;
        } else if (!strcmp(a, "-o") || !strcmp(a, "--output")) {
            opt->output = next_arg(argc, argv, &i);
        } else if (!strcmp(a, "-q") || !strcmp(a, "--quiet")) {
            opt->quiet = true;
        } else if (!strcmp(a, "--mono")) {
            opt->mono = true;
        } else if (!strcmp(a, "--info")) {
            opt->info = true;
        } else if (!strcmp(a, "--seed")) {
            opt->seed = (unsigned)to_double(a, next_arg(argc, argv, &i));
        } else if (!strcmp(a, "--tail")) {
            opt->tail = to_double(a, next_arg(argc, argv, &i));
            check_range(a, opt->tail, 0, 600);
        } else if (!strcmp(a, "--gain")) {
            opt->gain = to_double(a, next_arg(argc, argv, &i));
            check_range(a, opt->gain, 0.01, 1);
        } else if (!strcmp(a, "--bpm")) {
            opt->bpm = to_double(a, next_arg(argc, argv, &i));
            check_range(a, opt->bpm, 1, 1000);
        } else if (!strcmp(a, "--speed")) {
            opt->speed = to_double(a, next_arg(argc, argv, &i));
            check_range(a, opt->speed, 0.05, 20);
        } else if (!strcmp(a, "--humanize")) {
            opt->humanize_ms = to_double(a, next_arg(argc, argv, &i));
            check_range(a, opt->humanize_ms, 0, 100);
        } else if (!strcmp(a, "--transpose")) {
            opt->transpose = (int)to_double(a, next_arg(argc, argv, &i));
        } else if (!strcmp(a, "--start")) {
            opt->start = to_double(a, next_arg(argc, argv, &i));
            check_range(a, opt->start, 0, 1e6);
        } else if (!strcmp(a, "--end")) {
            opt->end = to_double(a, next_arg(argc, argv, &i));
        } else if (!strcmp(a, "--reverb")) {
            opt->reverb = to_double(a, next_arg(argc, argv, &i));
            check_range(a, opt->reverb, 0, 1);
        } else if (!strcmp(a, "--dark")) {
            opt->tone_dark = to_double(a, next_arg(argc, argv, &i));
            check_range(a, opt->tone_dark, 0, 5);
        } else if (!strcmp(a, "--body")) {
            opt->body_boost = to_double(a, next_arg(argc, argv, &i));
            check_range(a, opt->body_boost, 0, 5);
        } else if (!strcmp(a, "--cut")) {
            opt->cut_hz = to_double(a, next_arg(argc, argv, &i));
            check_range(a, opt->cut_hz, 200, 20000);
        } else if (!strcmp(a, "--tau")) {
            opt->prompt_tau = to_double(a, next_arg(argc, argv, &i));
            check_range(a, opt->prompt_tau, 0.05, 10);
        } else if (a[0] == '-' && a[1]) {
            die("unknown option '%s' (try --help)", a);
        } else if (opt->input) {
            die("more than one input file given ('%s' and '%s')", opt->input,
                a);
        } else {
            opt->input = a;
        }
    }

    if (!opt->input)
        die("no input file given (try --help)");
    if (!opt->output)
        opt->output = default_output(opt->input);
    if (opt->end > 0 && opt->end <= opt->start)
        die("--end must be greater than --start");
    return true;
}
