#ifndef SIGNALS_H
#define SIGNALS_H

#include <signal.h>
#include <sys/types.h>

extern pid_t foreground_pid;
extern int foreground_job_id;

void setup_signal_handlers();
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void handle_eof();

#endif
