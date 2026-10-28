CC     = gcc
CFLAGS = -std=gnu99 -O2 -Wall -Wextra

SRC = src/main.c src/common.c src/options.c src/score.c src/text_score.c \
      src/midi.c src/synth.c src/effects.c src/wav.c src/progress.c \
      src/play_win.c src/play_posix.c
OBJ = $(SRC:.c=.o)

ifeq ($(OS),Windows_NT)
    TARGET = simple_mid_player.exe
else
    TARGET = simple_mid_player
endif
LDLIBS = -lm

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
