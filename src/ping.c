#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include "ping.h"

static int parse_int_strict(const char* s, long* out) {
    if (!s || !*s) return 0;
    char* end = NULL;
    errno = 0;
    long val = strtol(s, &end, 10);
    if (errno != 0 || end == s || *end != '\0') return 0;
    *out = val;
    return 1;
}

int ping_command(arg_node* args) {
    // Expect exactly two arguments: <pid> <signal_number>
    if (!args || !args->next || (args->next && args->next->next)) {
        printf("Invalid syntax!\n");
        return 1;
    }

    long pid_l = 0;
    long sig_l = 0;

    // Parse pid; if invalid, treat as no such process to avoid signaling PGID 0
    if (!parse_int_strict(args->value, &pid_l) || pid_l <= 0 || pid_l > INT_MAX) {
        printf("No such process found\n");
        return 1;
    }

    // Parse signal number strictly; if invalid, report syntax error
    if (!parse_int_strict(args->next->value, &sig_l)) {
        printf("Invalid syntax!\n");
        return 1;
    }

    pid_t pid = (pid_t)pid_l;
    int signal_number = (int)sig_l;

    // actual signal modulo 32 (ensure non-negative)
    int actual_signal = signal_number % 32;
    if (actual_signal < 0) actual_signal += 32;

    // Try to send the signal
    if (kill(pid, actual_signal) == 0) {
        printf("Sent signal %d to process with pid %d\n", signal_number, pid);
    } else {
        if (errno == ESRCH) {
            printf("No such process found\n");
        } else {
            perror("ping");
        }
    }

    return 1;
}