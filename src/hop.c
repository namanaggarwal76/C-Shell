// ai generated code starts here

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/limits.h>
#include <sys/stat.h>
#include "hop.h"

extern char home_dir[PATH_MAX];
extern char prev_dir[PATH_MAX];

int hop_command(arg_node* args){
    char curr_dir[PATH_MAX];
    if (getcwd(curr_dir, sizeof(curr_dir)) == NULL){
        perror("getcwd");
        return 1;
    }
    
    if (!args){
        // No arguments - go to home
        if (chdir(home_dir) != 0){
            perror("hop");
            return 1;
        }
        strcpy(prev_dir, curr_dir);
        return 1;
    }
    
    arg_node* current = args;
    while (current){
        char* arg = current->value;
        char new_dir[PATH_MAX];
        
        if (strcmp(arg, "~") == 0){
            strcpy(new_dir, home_dir);
        }
        else if (strcmp(arg, ".") == 0){
            strcpy(new_dir, curr_dir);
        }
        else if (strcmp(arg, "..") == 0){
            char* last_slash = strrchr(curr_dir, '/');
            if (last_slash && last_slash != curr_dir){
                *last_slash = '\0';
                strcpy(new_dir, curr_dir);
                *last_slash = '/';
            } else {
                strcpy(new_dir, "/");
            }
        }
        else if (strcmp(arg, "-") == 0){
            if (strlen(prev_dir) == 0){
                current = current->next;
                continue;
            }
            strcpy(new_dir, prev_dir);
        }
        else {
            // Regular path
            if (arg[0] == '/'){
                if (strlen(arg) >= PATH_MAX){
                    printf("No such directory!\n");
                    return 1;
                }
                strcpy(new_dir, arg);
            } else {
                if (strlen(curr_dir) + strlen(arg) + 2 >= PATH_MAX){
                    printf("No such directory!\n");
                    return 1;
                }
                strcpy(new_dir, curr_dir);
                strcat(new_dir, "/");
                strcat(new_dir, arg);
            }
        }
        
        struct stat st;
        if (stat(new_dir, &st) != 0 || !S_ISDIR(st.st_mode)){
            printf("No such directory!\n");
            return 1;
        }
        
        if (chdir(new_dir) != 0){
            printf("No such directory!\n");
            return 1;
        }
        
        strcpy(prev_dir, curr_dir);
        if (getcwd(curr_dir, sizeof(curr_dir)) == NULL){
            perror("getcwd");
            return 1;
        }
        
        current = current->next;
    }
    
    return 1;
}


// ai generated code ends here