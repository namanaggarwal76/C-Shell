// ai generated code starts here

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <linux/limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include "reveal.h"

extern char home_dir[PATH_MAX];
extern char prev_dir[PATH_MAX];

int compare_names(const void* a, const void* b){
    const char* str1 = *(const char**)a;
    const char* str2 = *(const char**)b;
    return strcmp(str1, str2); 
}

int reveal_command(arg_node* args){
    int show_all = 0;
    int long_format = 0;
    char target_dir[PATH_MAX];
    int has_target = 0;
    int arg_count = 0;
    
    // Count arguments first
    arg_node* temp = args;
    while (temp){
        arg_count++;
        temp = temp->next;
    }
    
    arg_node* current = args;
    while (current){
        char* arg = current->value;
        
        // Handle standalone '-' (previous directory) as a special case
        if (strcmp(arg, "-") == 0){
            if (has_target){
                printf("reveal: Invalid Syntax!\n");
                return 1;
            }
            has_target = 1;
            
            if (strlen(prev_dir) == 0){
                printf("No such directory!\n");
                return 1;
            }
            strcpy(target_dir, prev_dir);
        }
        else if (arg[0] == '-'){
            // Parse flags
            for (int i = 1; arg[i] != '\0'; i++){
                if (arg[i] == 'a'){
                    show_all = 1;
                } else if (arg[i] == 'l'){
                    long_format = 1;
                }
            }
        } else {
            if (has_target){
                printf("reveal: Invalid Syntax!\n");
                return 1;
            }
            has_target = 1;
            
            if (strcmp(arg, "~") == 0){
                strcpy(target_dir, home_dir);
            }
            else if (strcmp(arg, ".") == 0){
                if (getcwd(target_dir, sizeof(target_dir)) == NULL){
                    perror("getcwd");
                    return 1;
                }
            }
            else if (strcmp(arg, "..") == 0){
                if (getcwd(target_dir, sizeof(target_dir)) == NULL){
                    perror("getcwd");
                    return 1;
                }
                char* last_slash = strrchr(target_dir, '/');
                if (last_slash && last_slash != target_dir){
                    *last_slash = '\0';
                } else {
                    strcpy(target_dir, "/");
                }
            }
            else {
                if (arg[0] == '/'){
                    strcpy(target_dir, arg);
                } else {
                    char curr_dir[PATH_MAX];
                    if (getcwd(curr_dir, sizeof(curr_dir)) == NULL){
                        perror("getcwd");
                        return 1;
                    }
                    if (strlen(curr_dir) + strlen(arg) + 2 >= PATH_MAX){
                        printf("No such directory!\n");
                        return 1;
                    }
                    strcpy(target_dir, curr_dir);
                    strcat(target_dir, "/");
                    strcat(target_dir, arg);
                }
            }
        }
        current = current->next;
    }
    
    if (!has_target){
        if (getcwd(target_dir, sizeof(target_dir)) == NULL){
            perror("getcwd");
            return 1;
        }
    }
    
    struct stat st;
    if (stat(target_dir, &st) != 0 || !S_ISDIR(st.st_mode)){
        printf("No such directory!\n");
        return 1;
    }
    
    DIR* dir = opendir(target_dir);
    if (!dir){
        printf("No such directory!\n");
        return 1;
    }
    
    // Read directory entries
    char** entries = NULL;
    int entry_count = 0;
    int entry_capacity = 10;
    
    entries = malloc(entry_capacity * sizeof(char*));
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL){
        if (!show_all && entry->d_name[0] == '.'){
            continue;
        }
        
        if (entry_count >= entry_capacity){
            entry_capacity *= 2;
            entries = realloc(entries, entry_capacity * sizeof(char*));
        }
        
        entries[entry_count] = malloc(strlen(entry->d_name) + 1);
        strcpy(entries[entry_count], entry->d_name);
        entry_count++;
    }
    
    closedir(dir);
    
    // Sort entries lexicographically
    qsort(entries, entry_count, sizeof(char*), compare_names);
    
    // Print entries
    if (long_format){
        for (int i = 0; i < entry_count; i++){
            printf("%s\n", entries[i]);
        }
    } else {
        for (int i = 0; i < entry_count; i++){
            printf("%s", entries[i]);
            if (i < entry_count - 1){
                printf(" ");
            }
        }
        if (entry_count > 0){
            printf("\n");
        }
    }
    
    // Free memory
    for (int i = 0; i < entry_count; i++){
        free(entries[i]);
    }
    free(entries);
    
    return 1;
}

// ai generated code ends here