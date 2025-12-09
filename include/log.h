#ifndef LOG_H
#define LOG_H

#include "parser.h"

#define MAX_LOG_ENTRIES 15

int log_command(arg_node* args);
void add_to_log(const char* command);
void save_log();
void load_log();

#endif
