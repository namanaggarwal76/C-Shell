#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <linux/limits.h>
#include "execute.h"
#include "hop.h"
#include "reveal.h"
#include "log.h"
#include "activities.h"
#include "ping.h"
#include "signals.h"
#include "fg_bg.h"

#define MAX_JOBS 1000

static job background_jobs[MAX_JOBS];
static int job_count = 0;
static int next_job_id = 1;
pid_t foreground_pid = 0;
int foreground_job_id = 0;
char current_command_str[ARG_MAX] = "";

// Forward declaration for helper used before its definition
static void build_pipeline_command_into_buffer(command_node* head, char* buf, size_t buflen);

extern char home_dir[PATH_MAX];
extern char prev_dir[PATH_MAX];
extern char input_buffer[ARG_MAX];

job* get_jobs() {
    return background_jobs;
}

int get_job_count() {
    return job_count;
}

int get_next_job_id() {
    return next_job_id;
}

void update_job_state(int job_index, job_state state) {
    if (job_index >= 0 && job_index < job_count) {
        background_jobs[job_index].state = state;
    }
}

job* find_job_by_pid(pid_t pid) {
    for (int i = 0; i < job_count; i++) {
        if (background_jobs[i].active && background_jobs[i].pid == pid) {
            return &background_jobs[i];
        }
    }
    return NULL;
}

job* find_job_by_id(int job_id) {
    for (int i = 0; i < job_count; i++) {
        if (background_jobs[i].active && background_jobs[i].job_id == job_id) {
            return &background_jobs[i];
        }
    }
    return NULL;
}

int find_most_recent_job() {
    int max_id = -1;
    int index = -1;
    for (int i = 0; i < job_count; i++) {
        if (background_jobs[i].active && background_jobs[i].job_id > max_id) {
            max_id = background_jobs[i].job_id;
            index = i;
        }
    }
    return index;
}

void check_background_jobs(){
    for (int i = 0; i < job_count; i++){
        if (!background_jobs[i].active) continue;
        
        int status;
        pid_t result = waitpid(background_jobs[i].pid, &status, WNOHANG);
        
        if (result > 0){
            // Clean up command name by removing trailing & or ;
            const char* cmd = background_jobs[i].command ? background_jobs[i].command : "unknown";
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
            
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0){
                printf("%s with pid %d exited normally\n", 
                       to_print, background_jobs[i].pid);
            } else {
                printf("%s with pid %d exited abnormally\n", 
                       to_print, background_jobs[i].pid);
            }
            
            free(background_jobs[i].command);
            background_jobs[i].active = 0;
            background_jobs[i].state = JOB_TERMINATED;
        } else if (result == 0) {
            // Process is still running, check if it's stopped
            if (kill(background_jobs[i].pid, 0) == 0) {
                // Process exists, check if stopped
                char proc_path[256];
                snprintf(proc_path, sizeof(proc_path), "/proc/%d/stat", background_jobs[i].pid);
                FILE* f = fopen(proc_path, "r");
                if (f) {
                    char state;
                    fscanf(f, "%*d %*s %c", &state);
                    fclose(f);
                    if (state == 'T') {
                        background_jobs[i].state = JOB_STOPPED;
                    } else {
                        background_jobs[i].state = JOB_RUNNING;
                    }
                }
            }
        }
    }
}

void add_background_job(pid_t pid, const char* command){
    add_background_job_with_notification(pid, command, 1);
}

void add_background_job_with_notification(pid_t pid, const char* command, int show_notification){
    for (int i = 0; i < MAX_JOBS; i++){
        if (!background_jobs[i].active){
            background_jobs[i].pid = pid;
            background_jobs[i].command = malloc(strlen(command) + 1);
            strcpy(background_jobs[i].command, command);
            background_jobs[i].job_id = next_job_id++;
            background_jobs[i].state = JOB_RUNNING;
            background_jobs[i].active = 1;
            
            if (i >= job_count) job_count = i + 1;
            
            if (show_notification) {
                printf("[%d] %d\n", background_jobs[i].job_id, pid);
                fflush(stdout);  // Ensure output is flushed
            }
            return;
        }
    }
}

char** convert_args_to_argv(arg_node* args){
    int count = 0;
    arg_node* temp = args;
    while (temp){
        count++;
        temp = temp->next;
    }
    
    char** argv = malloc((count + 1) * sizeof(char*));
    temp = args;
    for (int i = 0; i < count; i++){
        argv[i] = temp->value;
        temp = temp->next;
    }
    argv[count] = NULL;
    return argv;
}

int setup_redirection(atomic* cmd){
    // First validate all input files exist
    redirect_node* input_node = cmd->input_files;
    while (input_node){
        int fd = open(input_node->filename, O_RDONLY);
        if (fd < 0){
            printf("No such file or directory\n");
            return -1;
        }
        close(fd);
        input_node = input_node->next;
    }
    
    // Then validate all output files can be written
    redirect_node* output_node = cmd->output_files;
    while (output_node){
        int flags = O_WRONLY | O_CREAT;
        if (output_node->append){
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        
        int fd = open(output_node->filename, flags, 0644);
        if (fd < 0){
            printf("Unable to create file for writing\n");
            return -1;
        }
        close(fd);
        output_node = output_node->next;
    }
    
    // Now actually set up the final redirections (for compatibility)
    if (cmd->input_file){
        int fd = open(cmd->input_file, O_RDONLY);
        if (fd < 0){
            printf("No such file or directory\n");
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    
    if (cmd->output_file){
        int flags = O_WRONLY | O_CREAT;
        if (cmd->append){
            flags |= O_APPEND;
        } else {
            flags |= O_TRUNC;
        }
        
        int fd = open(cmd->output_file, flags, 0644);
        if (fd < 0){
            printf("Unable to create file for writing\n");
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    
    return 0;
}

int execute_builtin(atomic* cmd){
    if (!cmd->args || !cmd->args->value) return 0;
    
    char* command = cmd->args->value;
    
    if (strcmp(command, "exit") == 0){
        exit(0);
    }
    
    if (strcmp(command, "hop") == 0){
        return hop_command(cmd->args->next);
    }
    
    if (strcmp(command, "reveal") == 0){
        return reveal_command(cmd->args->next);
    }
    
    if (strcmp(command, "log") == 0){
        return log_command(cmd->args->next);
    }
    
    if (strcmp(command, "activities") == 0){
        return activities_command(cmd->args->next);
    }
    
    if (strcmp(command, "ping") == 0){
        return ping_command(cmd->args->next);
    }
    
    if (strcmp(command, "fg") == 0){
        return fg_command(cmd->args->next);
    }
    
    if (strcmp(command, "bg") == 0){
        return bg_command(cmd->args->next);
    }
    
    return 0;
}

int execute_single_command(atomic* cmd){
    if (!cmd->args || !cmd->args->value) return -1;
    
    // Check if redirection is needed
    int needs_redirection = (cmd->input_file != NULL || cmd->output_file != NULL);
    
    // For built-ins with redirection, we need to fork to avoid affecting the shell
    if (!needs_redirection && execute_builtin(cmd)){
        return 0;
    }
    
    pid_t pid = fork();
    if (pid == 0){
        // Child process - create new process group
        setpgid(0, 0);
        
        if (setup_redirection(cmd) < 0){
            exit(1);
        }
        
        // Try builtin first, then external
        if (execute_builtin(cmd)){
            exit(0);
        }
        
        char** argv = convert_args_to_argv(cmd->args);
        execvp(argv[0], argv);
        printf("Command not found!\n");
        free(argv);
        exit(127);
    } else if (pid > 0){
        // Parent process
        setpgid(pid, pid);  // Set child's process group
        foreground_pid = pid;
        
        int status;
        pid_t result = waitpid(pid, &status, WUNTRACED);
        
        if (result > 0) {
            if (WIFSTOPPED(status)) {
                // Process was suspended - already handled by signal handler
                foreground_pid = 0;
                return 0;
            } else if (WIFEXITED(status)) {
                foreground_pid = 0;
                return WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                foreground_pid = 0;
                return 1;
            }
        }
        
        foreground_pid = 0;
        return 0;
    } else {
        perror("fork");
        return -1;
    }
}

int execute_pipeline(command_node* head){
    if (!head) return -1;
    
    // Count commands in pipeline
    int cmd_count = 0;
    command_node* temp = head;
    while (temp){
        cmd_count++;
        temp = temp->next;
    }
    
    if (cmd_count == 1){
        // Build description for foreground tracking
        build_pipeline_command_into_buffer(head, current_command_str, sizeof(current_command_str));
        return execute_single_command(&head->command);
    }
    
    // Create pipes
    int pipes[cmd_count - 1][2];
    for (int i = 0; i < cmd_count - 1; i++){
        if (pipe(pipes[i]) < 0){
            perror("pipe");
            return -1;
        }
    }
    
    pid_t pids[cmd_count];
    pid_t pgid = 0;  // Process group ID
    command_node* current = head;
    
    for (int i = 0; i < cmd_count; i++){
        pids[i] = fork();
        
        if (pids[i] == 0){
            // Child process
            
            // Set process group
            if (i == 0) {
                setpgid(0, 0);
                pgid = getpid();
            } else {
                setpgid(0, pgid);
            }
            
            // Set up pipes
            if (i > 0){
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            if (i < cmd_count - 1){
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            
            // Close all pipe file descriptors
            for (int j = 0; j < cmd_count - 1; j++){
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Set up redirection for first and last commands with validation
            if (i == 0 && current->command.input_files){
                // Validate all input files first
                redirect_node* input_node = current->command.input_files;
                while (input_node){
                    int fd = open(input_node->filename, O_RDONLY);
                    if (fd < 0){
                        printf("No such file or directory\n");
                        exit(1);
                    }
                    close(fd);
                    input_node = input_node->next;
                }
                // Use final input file
                if (current->command.input_file) {
                    int fd = open(current->command.input_file, O_RDONLY);
                    if (fd < 0){
                        printf("No such file or directory\n");
                        exit(1);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
            }
            
            if (i == cmd_count - 1 && current->command.output_files){
                // Validate all output files first
                redirect_node* output_node = current->command.output_files;
                while (output_node){
                    int flags = O_WRONLY | O_CREAT;
                    if (output_node->append){
                        flags |= O_APPEND;
                    } else {
                        flags |= O_TRUNC;
                    }
                    
                    int fd = open(output_node->filename, flags, 0644);
                    if (fd < 0){
                        printf("Unable to create file for writing\n");
                        exit(1);
                    }
                    close(fd);
                    output_node = output_node->next;
                }
                // Use final output file
                if (current->command.output_file) {
                    int flags = O_WRONLY | O_CREAT;
                    if (current->command.append){
                        flags |= O_APPEND;
                    } else {
                        flags |= O_TRUNC;
                    }
                    
                    int fd = open(current->command.output_file, flags, 0644);
                    if (fd < 0){
                        printf("Unable to create file for writing\n");
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
            }
            
            // Execute builtin or external command
            if (execute_builtin(&current->command)){
                exit(0);
            }
            
            char** argv = convert_args_to_argv(current->command.args);
            execvp(argv[0], argv);
            printf("Command not found!\n");
            free(argv);
            exit(127);
        } else if (pids[i] > 0) {
            // Parent process - set up process group
            if (i == 0) {
                pgid = pids[i];
                setpgid(pids[i], pgid);
        foreground_pid = pgid;  // Track the process group leader
        // Track current foreground command description
        build_pipeline_command_into_buffer(head, current_command_str, sizeof(current_command_str));
            } else {
                setpgid(pids[i], pgid);
            }
        }
        
        current = current->next;
    }
    
    // Close all pipe file descriptors in parent
    for (int i = 0; i < cmd_count - 1; i++){
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Wait for all children
    int exit_status = 0;
    for (int i = 0; i < cmd_count; i++){
        int status;
        pid_t result = waitpid(pids[i], &status, WUNTRACED);
        if (result == -1) {
            perror("waitpid");
        }
        
        if (WIFSTOPPED(status)) {
            // One of the processes in the pipeline was suspended
            // The signal handler should have already handled job management
            foreground_pid = 0;
            return 0;
        }
        
        if (i == cmd_count - 1){
            if (WIFEXITED(status)) {
                exit_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                exit_status = 1;
            }
        }
    }
    
    foreground_pid = 0;
    current_command_str[0] = '\0';
    return exit_status;
}

// Build a descriptive command string for a pipeline, including arguments,
// separated by " | ", and with a trailing " &" for background jobs.
static char* build_pipeline_command_string(command_node* head) {
    // First pass: compute required length
    size_t len = 0;
    command_node* node = head;
    while (node) {
        arg_node* arg = node->command.args;
        int first = 1;
        while (arg) {
            if (!first) len += 1; // space between args
            len += strlen(arg->value);
            first = 0;
            arg = arg->next;
        }
        if (node->next) {
            len += 3; // ' | '
        }
        node = node->next;
    }
    len += 2; // space + '&'
    len += 1; // null terminator

    char* out = (char*)malloc(len);
    if (!out) return NULL;
    out[0] = '\0';

    // Second pass: build the string
    node = head;
    while (node) {
        arg_node* arg = node->command.args;
        int first = 1;
        while (arg) {
            if (!first) strcat(out, " ");
            strcat(out, arg->value);
            first = 0;
            arg = arg->next;
        }
        if (node->next) strcat(out, " | ");
        node = node->next;
    }
    strcat(out, " &");
    return out;
}

// Build command string without trailing '&' into provided buffer
static void build_pipeline_command_into_buffer(command_node* head, char* buf, size_t buflen) {
    if (!buf || buflen == 0) return;
    buf[0] = '\0';
    command_node* node = head;
    while (node) {
        arg_node* arg = node->command.args;
        int first = 1;
        while (arg) {
            if (!first) strncat(buf, " ", buflen - strlen(buf) - 1);
            strncat(buf, arg->value, buflen - strlen(buf) - 1);
            first = 0;
            arg = arg->next;
        }
        if (node->next) strncat(buf, " | ", buflen - strlen(buf) - 1);
        node = node->next;
    }
}

char* get_command_string(command_node* head){
    // Build full command with args and trailing ' &'
    char* s = build_pipeline_command_string(head);
    if (s) return s;
    // Fallbacks in low-memory or empty cases
    if (head && head->command.args && head->command.args->value){
        // Allocate minimal fallback with command name and ' &'
        const char* name = head->command.args->value;
        size_t n = strlen(name) + 3; // space + '&' + NUL
        char* out = (char*)malloc(n);
        if (!out) return "unknown";
        snprintf(out, n, "%s &", name);
        return out;
    }
    return "unknown";
}

int execute_background(command_node* head, int show_notification){
    if (!head) return -1;
    
    pid_t pid = fork();
    if (pid == 0){
        // Child process - create new process group and redirect stdin
        setpgid(0, 0);
        
        int null_fd = open("/dev/null", O_RDONLY);
        if (null_fd >= 0){
            dup2(null_fd, STDIN_FILENO);
            close(null_fd);
        }
        
        int status = execute_pipeline(head);
        exit(status);
    } else if (pid > 0){
        // Parent process
        setpgid(pid, pid);  // Set child's process group
        char* command_name = get_command_string(head);
        add_background_job_with_notification(pid, command_name, show_notification);
        // get_command_string allocates memory; free after copying into job list
        if (command_name && strcmp(command_name, "unknown") != 0) {
            free(command_name);
        }
        return 0;
    } else {
        perror("fork");
        return -1;
    }
}

// Helper function to check if any command in the command line contains "log"
int contains_log_command(group_node* head) {
    group_node* current_group = head;
    
    // Iterate through all command groups (separated by ; or &)
    while (current_group) {
        command_node* current_cmd = current_group->head;
        
        // Iterate through all commands in pipeline (separated by |)
        while (current_cmd) {
            // Check if this atomic command is "log"
            if (current_cmd->command.args && 
                current_cmd->command.args->value && 
                strcmp(current_cmd->command.args->value, "log") == 0) {
                return 1;  // Found a log command
            }
            current_cmd = current_cmd->next;
        }
        current_group = current_group->next;
    }
    
    return 0;  // No log command found
}

void execute(group_node* head){
    check_background_jobs();
    
    if (!head) return;
    
    // Store command in log only if it doesn't contain any "log" command
    if (!contains_log_command(head)) {
        add_to_log(input_buffer);
    }
    
    group_node* current = head;
    
    while (current){
        // Check if current group should run in background
        if (current->op_code && strcmp(current->op_code, "&") == 0){
            // Always show background job notification
            int show_notification = 1;
            execute_background(current->head, show_notification);
            // If there's a next command after &, execute it in foreground
            if (current->next){
                current = current->next;
                continue;
            } else {
                // No next command, just background job
                break;
            }
        } else {
            execute_pipeline(current->head);
        }
        
        // Sequential execution - continue to next command
        if (current->op_code && strcmp(current->op_code, ";") == 0){
            current = current->next;
            continue;
        }
        break;
    }
}