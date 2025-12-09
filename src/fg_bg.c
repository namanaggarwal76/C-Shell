// ai generated code starts here

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <termios.h>
#include <string.h>
#include "fg_bg.h"
#include "execute.h"
#include "signals.h"

int fg_command(arg_node* args) {
    job* jobs = get_jobs();
    int job_index = -1;
    
    if (!args) {
        // No job number provided, use most recent
        job_index = find_most_recent_job();
    } else {
        int job_id = atoi(args->value);
        job* target_job = find_job_by_id(job_id);
        if (target_job) {
            job_index = target_job - jobs;
        }
    }
    
    if (job_index == -1 || !jobs[job_index].active) {
        printf("No such job\n");
        return 1;
    }
    
    job* target_job = &jobs[job_index];
    
    // Print the command being brought to foreground
    printf("%s\n", target_job->command);
    
    // Set as foreground process
    foreground_pid = target_job->pid;
    foreground_job_id = target_job->job_id;
    
    // If job is stopped, send SIGCONT
    if (target_job->state == JOB_STOPPED) {
        kill(target_job->pid, SIGCONT);
    }
    
    target_job->state = JOB_RUNNING;
    
    // Give terminal control to the process group
    tcsetpgrp(STDIN_FILENO, target_job->pid);
    
    // Wait for the job to complete or stop
    int status;
    waitpid(target_job->pid, &status, WUNTRACED);
    
    // Take back terminal control
    tcsetpgrp(STDIN_FILENO, getpgrp());
    
    if (WIFSTOPPED(status)) {
        target_job->state = JOB_STOPPED;
        printf("[%d] Stopped %s\n", target_job->job_id, target_job->command);
    } else {
        // Job completed, remove from list
        free(target_job->command);
        target_job->active = 0;
        target_job->state = JOB_TERMINATED;
    }
    
    foreground_pid = 0;
    foreground_job_id = 0;
    
    return 1;
}

int bg_command(arg_node* args) {
    job* jobs = get_jobs();
    int job_index = -1;
    
    if (!args) {
        // No job number provided, use most recent
        job_index = find_most_recent_job();
    } else {
        int job_id = atoi(args->value);
        job* target_job = find_job_by_id(job_id);
        if (target_job) {
            job_index = target_job - jobs;
        }
    }
    
    if (job_index == -1 || !jobs[job_index].active) {
        printf("No such job\n");
        return 1;
    }
    
    job* target_job = &jobs[job_index];
    
    if (target_job->state == JOB_RUNNING) {
        printf("Job already running\n");
        return 1;
    }
    
    if (target_job->state == JOB_STOPPED) {
        kill(target_job->pid, SIGCONT);
        target_job->state = JOB_RUNNING;
        const char* full = target_job->command ? target_job->command : "";
            // Ensure exactly one trailing ' &'
            char msg[PATH_MAX];
            size_t n = strlen(full);
            int has_amp = (n >= 1 && full[n-1] == '&');
            if (has_amp) {
                snprintf(msg, sizeof(msg), "[%d] %s\n", target_job->job_id, full);
            } else {
                snprintf(msg, sizeof(msg), "[%d] %s &\n", target_job->job_id, full);
            }
            fputs(msg, stdout);
    }
    
    return 1;
}

// ai generated code ends here