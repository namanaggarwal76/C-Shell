// ai generated code starts here

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "activities.h"
#include "execute.h"

int compare_jobs(const void* a, const void* b) {
    job* job_a = (job*)a;
    job* job_b = (job*)b;
    
    if (!job_a->active && job_b->active) return 1;
    if (job_a->active && !job_b->active) return -1;
    if (!job_a->active && !job_b->active) return 0;
    
    return strcmp(job_a->command, job_b->command);
}

int activities_command(arg_node* args) {
    job* jobs = get_jobs();
    int job_count = get_job_count();
    
    job* sorted_jobs = malloc(job_count * sizeof(job));
    int active_count = 0;
    
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].active && jobs[i].state != JOB_TERMINATED) {
            sorted_jobs[active_count] = jobs[i];
            active_count++;
        }
    }
    
    qsort(sorted_jobs, active_count, sizeof(job), compare_jobs);
    
    for (int i = 0; i < active_count; i++) {
        char* state_str;
        switch (sorted_jobs[i].state) {
            case JOB_RUNNING:
                state_str = "Running";
                break;
            case JOB_STOPPED:
                state_str = "Stopped";
                break;
            default:
                continue;
        }
        printf("[%d] : %s - %s\n", sorted_jobs[i].pid, sorted_jobs[i].command, state_str);
    }
    free(sorted_jobs);
    return 1;
}

// ai generated code ends here