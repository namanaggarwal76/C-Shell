#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <linux/limits.h>
#include <signal.h>
#include "prompt.h"
#include "parser.h"
#include "execute.h"
#include "signals.h"
#include "log.h"

char home_dir[PATH_MAX];
char input_buffer[ARG_MAX];
char prev_dir[PATH_MAX] = "";

int main() {
    if (getcwd(home_dir, sizeof(home_dir)) == NULL) {
        perror("error getting home directory using getcwd()");
        exit(1);
    }

    setup_signal_handlers();

    while (1){
        check_background_jobs();
        showprompt();

        char* result = fgets(input_buffer, sizeof(input_buffer), stdin);
        if (result == NULL){
            if (feof(stdin)){
                handle_eof();
            }
            else{
                clearerr(stdin);
                continue;
            }
        }

        int len=strlen(input_buffer);
        if (len>0 && input_buffer[len-1]=='\n') {
            input_buffer[len-1]='\0';
            len--;
        }

        if (len==0) continue;

        group_node* parsed = parse_input();
        if (parsed){
            execute(parsed);
            free_group_list(parsed);
        }
        else{
            add_to_log(input_buffer);   // added invalid command to log
        }
    }
}