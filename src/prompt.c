#include <stdio.h>  
#include <stdlib.h> 
#include <unistd.h>  
#include <string.h>  
#include <linux/limits.h>
#include <bits/local_lim.h>
#include <pwd.h>
#include "prompt.h"

extern char home_dir[5000];

void file_path(char *currdir, char *prompt){
    if (strstr(currdir,home_dir)==currdir){
        strcat(prompt,"~");
        int ptr=0;
        while (home_dir[ptr]!='\0'){
            ptr++;
        }
        strcat(prompt, &currdir[ptr]);
    }
    else{
        strcat(prompt, currdir);
    }
}

void showprompt(){
    char user[LOGIN_NAME_MAX];
    char prompt[5000];
    prompt[0] = '\0';

    char host[HOST_NAME_MAX];       
    if (gethostname(host, sizeof(host))!=0){
        strcpy(host, "SystemName"); // fallback if gethostname() fails
    }

    struct passwd *pw = getpwuid(getuid());
    if (pw==NULL){
        strcpy(user, "Username");
    }
    else{
        strcpy(user, pw->pw_name);
    }

    char curr_dir[PATH_MAX];
    if (getcwd(curr_dir, sizeof(curr_dir))==NULL) {
        perror("Failed to get current directory using getcwd()");
        exit(1);
    }

    // building the prompt
    strcat(prompt, "<");
    strcat(prompt, user);
    strcat(prompt, "@");
    strcat(prompt, host);
    strcat(prompt, ":");
    file_path(curr_dir, prompt);
    strcat(prompt, "> ");

    printf("%s", prompt);
}