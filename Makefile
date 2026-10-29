CC     = gcc
CFLAGS = -std=gnu99 -O2 -Wall -Wextra -MMD -MP

SRC = src/main.c src/common.c src/options.c src/score.c src/text_score.c \
      src/midi.c src/synth.c src/effects.c src/wav.c src/progress.c \
      src/play_win.c src/play_posix.c
OBJ = $(SRC:.c=.o)

ifeq ($(OS),Windows_NT)
    TARGET = simple_mid_player.exe
    LDLIBS = -lm -lwinmm
else
    TARGET = simple_mid_player
    LDLIBS = -lm
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(OBJ:.o=.d) $(TARGET)

-include $(OBJ:.o=.d)
