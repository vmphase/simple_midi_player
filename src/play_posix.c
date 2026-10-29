#ifndef _WIN32

#define _POSIX_C_SOURCE 200809L

#include "player.h"

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

/* Command-line players, in order of preference. The WAV path is appended. */
static const char *const PLAYERS[][8] = {
#ifdef __APPLE__
    {"afplay"},
#else
    {"paplay"},      {"pw-play"},
    {"aplay", "-q"}, {"ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet"},
    {"play", "-q"}, /* SoX */
#endif
};

/* Runs one player. Returns 1 on success, 0 if it is missing or failed, -1 if
 * it was interrupted by a signal and we should stop trying. */
static int run_player(const char *const *cmd, const char *path) {
    const char *argv[12];
    int n = 0;
    for (; cmd[n]; n++)
        argv[n] = cmd[n];
    argv[n++] = path;
    argv[n] = NULL;

    pid_t pid = fork();
    if (pid < 0)
        return 0;
    if (pid == 0) {
        execvp(argv[0], (char *const *)argv);
        _exit(127); /* not installed */
    }

    int status;
    if (waitpid(pid, &status, 0) < 0)
        return 0;
    if (WIFSIGNALED(status))
        return -1;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool player_play_wav(const char *path) {
    for (size_t i = 0; i < sizeof PLAYERS / sizeof PLAYERS[0]; i++) {
        int result = run_player(PLAYERS[i], path);
        if (result != 0)
            return result > 0;
    }
    return false;
}

bool player_play_gm(const Score *score) {
    (void)score;
    fprintf(stderr, "--gm only works on Windows.\n");
    return false;
}

#endif
