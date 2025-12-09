#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/limits.h>
#include "log.h"
#include "parser.h"
#include "execute.h"

extern char home_dir[PATH_MAX];
extern char input_buffer[ARG_MAX];

static char* log_entries[MAX_LOG_ENTRIES];
static int log_count = 0;
static int log_start = 0;

char* get_log_file_path(){
    static char log_path[PATH_MAX];
    char* env_home = getenv("HOME");
    if (env_home) {
        if (strlen(env_home) + 12 >= PATH_MAX){
            return NULL;
        }
        strcpy(log_path, env_home);
        strcat(log_path, "/.shell_log");
    } else {
        if (strlen(home_dir) + 12 >= PATH_MAX){
            return NULL;
        }
        strcpy(log_path, home_dir);
        strcat(log_path, "/.shell_log");
    }
    return log_path;
}

void load_log(){
    FILE* file = fopen(get_log_file_path(), "r");
    if (!file) return;
    char line[ARG_MAX];
    log_count = 0;
    log_start = 0;
    
    while (fgets(line, sizeof(line), file) && log_count < MAX_LOG_ENTRIES){
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n'){
            line[len-1] = '\0';
        }
        
        log_entries[log_count] = malloc(strlen(line) + 1);
        strcpy(log_entries[log_count], line);
        log_count++;
    }
    
    fclose(file);
}

void save_log(){
    // Only create/save log file if we have entries
    if (log_count == 0) return;
    
    FILE* file = fopen(get_log_file_path(), "w");
    if (!file) return;
    
    for (int i = 0; i < log_count; i++){
        int idx = (log_start + i) % MAX_LOG_ENTRIES;
        if (log_entries[idx]){
            fprintf(file, "%s\n", log_entries[idx]);
        }
    }
    
    fclose(file);
}

void add_to_log(const char* command){
    if (log_count == 0){
        load_log();
    }
    
    // Check if same as last command
    if (log_count > 0){
        int last_idx = (log_start + log_count - 1) % MAX_LOG_ENTRIES;
        if (log_entries[last_idx] && strcmp(log_entries[last_idx], command) == 0){
            return;
        }
    }
    
    if (log_count < MAX_LOG_ENTRIES){
        log_entries[log_count] = malloc(strlen(command) + 1);
        strcpy(log_entries[log_count], command);
        log_count++;
    } else {
        // Overwrite oldest
        free(log_entries[log_start]);
        log_entries[log_start] = malloc(strlen(command) + 1);
        strcpy(log_entries[log_start], command);
        log_start = (log_start + 1) % MAX_LOG_ENTRIES;
    }
    
    // Only save if we actually have entries
    save_log();
}

int log_command(arg_node* args){
    if (log_count == 0){
        load_log();
    }
    
    if (!args){
        // Print all commands in order from oldest to newest
        for (int i = 0; i < log_count; i++){
            int idx = (log_start + i) % MAX_LOG_ENTRIES;
            if (log_entries[idx]){
                printf("%s\n", log_entries[idx]);
            }
        }
        return 1;
    }
    
    char* command = args->value;
    
    if (strcmp(command, "purge") == 0){
        // Clear history
        for (int i = 0; i < log_count; i++){
            int idx = (log_start + i) % MAX_LOG_ENTRIES;
            if (log_entries[idx]){
                free(log_entries[idx]);
                log_entries[idx] = NULL;
            }
        }
        log_count = 0;
        log_start = 0;
        
        // Delete the log file
        char* log_path = get_log_file_path();
        if (log_path) {
            unlink(log_path);
        }
        return 1;
    }
    
    if (strcmp(command, "execute") == 0){
        if (!args->next){
            printf("log: Invalid Syntax!\n");
            return 1;
        }
        
        // Check if the argument is a valid positive integer
        char* arg = args->next->value;
        char* endptr;
        long index = strtol(arg, &endptr, 10);
        
        // Check if conversion was successful and the entire string was consumed
        if (*endptr != '\0' || index < 1 || index > log_count){
            printf("log: Invalid index!\n");
            return 1;
        }
        
        // Index is 1-based, newest to oldest
        int actual_idx = (log_start + log_count - (int)index) % MAX_LOG_ENTRIES;
        if (log_entries[actual_idx]){
            // Execute the command rather than printing it
            size_t cmd_len = strlen(log_entries[actual_idx]);
            if (cmd_len >= sizeof(input_buffer)) {
                // Truncate safely if command is too long for input buffer
                cmd_len = sizeof(input_buffer) - 1;
            }
            memcpy(input_buffer, log_entries[actual_idx], cmd_len);
            input_buffer[cmd_len] = '\0';

            group_node* parsed = parse_input();
            if (parsed) {
                execute(parsed);
                free_group_list(parsed);
            }
        }
        return 1;
    }
    
    return 1;
}