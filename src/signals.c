#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include "signals.h"
#include "execute.h"

extern pid_t foreground_pid;
extern int foreground_job_id;

void setup_signal_handlers() {
    struct sigaction sa_int, sa_tstp;
    
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa_int, NULL);
    
    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    sigaction(SIGTSTP, &sa_tstp, NULL);
    
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
}

void sigint_handler(int sig) {
    if (foreground_pid > 0) {
        kill(-foreground_pid, SIGINT);
    }
}

void sigtstp_handler(int sig) {
    if (foreground_pid > 0) {
        kill(-foreground_pid, SIGTSTP);
        job* current_job = find_job_by_pid(foreground_pid);
        if (!current_job) {
            extern char current_command_str[];
            const char* name = current_command_str[0] ? current_command_str : "unknown";
            add_background_job_with_notification(foreground_pid, name, 0);
            current_job = find_job_by_pid(foreground_pid);
        }
        
        if (current_job) {
            current_job->state = JOB_STOPPED;
            const char* cmd = current_job->command ? current_job->command : "unknown";
            char temp[1024];
            const char* to_print = cmd;
            size_t n = strlen(cmd);
            if (n < sizeof(temp)) {
                strncpy(temp, cmd, sizeof(temp) - 1);
                temp[sizeof(temp) - 1] = '\0';
                while (n > 0 && (temp[n-1] == ' ' || temp[n-1] == '\t')) n--, temp[n] = '\0';
                if (n > 0 && (temp[n-1] == '&' || temp[n-1] == ';')) {
                    temp[--n] = '\0';
                    while (n > 0 && (temp[n-1] == ' ' || temp[n-1] == '\t')) temp[--n] = '\0';
                }
                to_print = temp;
            }
            printf("[%d] Stopped %s\n", current_job->job_id, to_print);
        }
        
        foreground_pid = 0;
        foreground_job_id = 0;
        
        fflush(stdout);
    }
    else{
        fflush(stdout);
    }
}

void handle_eof() {
    printf("logout\n");
    job* jobs = get_jobs();
    int job_count = get_job_count();
    
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active) {
            kill(jobs[i].pid, SIGKILL);
        }
    }   
    exit(0);
}