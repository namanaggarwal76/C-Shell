#ifndef EXECUTE_H
#define EXECUTE_H
#include "parser.h"
#include <sys/types.h>

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_TERMINATED
} job_state;

typedef struct job {
    pid_t pid;
    char* command;
    int job_id;
    job_state state;
    int active;
} job;

void execute(group_node* head);
void check_background_jobs();

job* get_jobs();
int get_job_count();
int get_next_job_id();
void update_job_state(int job_index, job_state state);
job* find_job_by_pid(pid_t pid);
job* find_job_by_id(int job_id);
int find_most_recent_job();
void add_background_job(pid_t pid, const char* command);
void add_background_job_with_notification(pid_t pid, const char* command, int show_notification);

#endif
