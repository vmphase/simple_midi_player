# simple_mid_player

A small multiplatform command-line piano synthesizer and MIDI player written in plain C with no external dependencies. It renders `.mid` files or a simple text score into a WAV file using additive synthesis, and can optionally play the result when it is done.

Flow:
1. Load the input: a standard MIDI file (format 0/1, drums skipped) or a text score (see below).
2. Apply `--transpose`, `--speed` and `--start`/`--end`, then let notes ring while the sustain pedal is held.
3. Synthesize every note: inharmonic partials of stiff strings, detuned string pairs, a fast "prompt" decay plus a slow aftersound, a hammer thump and pitch-based stereo.
4. Add piano-body EQ and a small Schroeder reverb, normalize the peak, and write a 16-bit 44.1 kHz WAV next to the input (`song.mid` -> `song.wav`).
5. With `--play`, play the WAV through the system player.

## Requirements

- A C99 compiler with GNU extensions (`gcc` or `clang`); MinGW-w64 on Windows
- Nothing else to build. Only for `--play`, one of the system players below

| OS      | `--play` uses                                                         |
| ------- | --------------------------------------------------------------------- |
| Windows | built-in `PlaySound` (winmm)                                          |
| macOS   | `afplay`                                                              |
| Linux   | the first one found of `paplay`, `pw-play`, `aplay`, `ffplay`, `play` |

## Build

With make:

```
make
```

Without make:

```
# Windows
gcc -std=gnu99 -O2 -Wall -Wextra src/*.c -o simple_mid_player.exe -lm -lwinmm

# Linux / macOS
gcc -std=gnu99 -O2 -Wall -Wextra src/*.c -o simple_mid_player -lm
```

## Usage

```
simple_mid_player [options] <score.mid | score.txt>
```

```
simple_mid_player song.mid                 # renders song.wav
simple_mid_player song.mid -o out.wav -p   # custom name, then play
simple_mid_player examples/demo.txt --bpm 80 --reverb 0.3
simple_mid_player song.mid --transpose -2 --speed 0.9 --start 10 --end 40
simple_mid_player song.mid --info          # only print statistics
```

### Options

| Option                        | Description                                                     |
| ----------------------------- | --------------------------------------------------------------- |
| `-o, --output FILE`           | output WAV file (default: input name with `.wav`)               |
| `-p, --play`                  | play the result when done                                       |
| `--gm`                        | Windows only: play live through the GM synth, no WAV is written |
| `--tail SEC`                  | silence kept after the last note (default 6)                    |
| `--gain X`                    | peak level after normalizing, 0..1 (default 0.9)                |
| `--mono`                      | write a mono file                                               |
| `--bpm N`                     | tempo of text scores (default 65)                               |
| `--speed X`                   | tempo multiplier, also for MIDI (default 1)                     |
| `--humanize MS`               | random timing wobble of text scores (default 5, 0 = off)        |
| `--transpose N`               | shift by N semitones                                            |
| `--start SEC`, `--end SEC`    | render only part of the piece; handy while tweaking the sound   |
| `--reverb X`                  | reverb amount, 0 = dry (default 0.15)                           |
| `--dark X`                    | above 1 = warmer, below 1 = brighter (default 0.5)              |
| `--body X`                    | low-end warmth (default 0.5)                                    |
| `--cut HZ`                    | treble roll-off frequency (default 7000)                        |
| `--tau X`                     | attack decay, smaller = more percussive (default 1.7)           |
| `--seed N`                    | random seed; the same seed gives the same file (default 12345)  |
| `--info`                      | print note count, range, length and pedal count, then exit      |
| `-q, --quiet`                 | no progress output                                              |
| `-h, --help`, `-V, --version` | help / version                                                  |

### Text score format

A score has a right hand (`RH:`) and a left hand (`LH:`). Lines starting with `#` are comments, and lines without a prefix continue the previous hand. See [examples/demo.txt](examples/demo.txt).

| Token            | Meaning                                                                          |
| ---------------- | -------------------------------------------------------------------------------- |
| `C#4:q`          | note: pitch + duration (`w h q e s t` = whole to 32nd, `q.` dotted, `1.5` beats) |
| `E4+G4+B4:h`     | chord                                                                            |
| `R:e`            | rest                                                                             |
| `gC5`            | grace note                                                                       |
| `v80`            | velocity (0..127) for the notes that follow                                      |
| `Pd` `Pu` `Pc`   | pedal down / up / change                                                         |
| `T90` `T90~60:4` | tempo, or a ramp to 60 BPM over 4 beats                                          |
| `^1.5`           | pause of 1.5 seconds                                                             |
| `\|`             | bar line, ignored                                                                |

Tempo, pause and pedal marks affect both hands, so write them in one hand only. Both hands should have the same length; a warning is printed if they differ.

## License

Apache 2.0. See the [LICENSE](LICENSE) file.
