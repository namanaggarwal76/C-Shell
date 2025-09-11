#ifndef PARSER_H
#define PARSER_H

typedef struct arg_node{
    char *value;               // current argument
    struct arg_node *next;     // next argument
} arg_node;

typedef struct redirect_node{
    char *filename;
    int append;  // for output: 1 if '>>', 0 if '>'
    struct redirect_node *next;
} redirect_node;

// Represents a single atomic command and its redirections
typedef struct atomic_command {
    arg_node *args;            // linked list of arguments (argv[0] is command)
    char *input_file;          // final input file for '<' (for compatibility)
    char *output_file;         // final output file for '>' or '>>' (for compatibility) 
    int append;                // 1 if '>>', 0 otherwise (for compatibility)
    redirect_node *input_files;  // all input redirections
    redirect_node *output_files; // all output redirections
} atomic;

// Linked list node for commands in a pipeline
typedef struct command_node{
    atomic command;            // the command
    struct command_node *next; // next command in the pipeline
} command_node;

// Linked list node for command groups separated by ';' or '&'
typedef struct group_node {
    command_node *head;        // head of pipeline (linked list of commands)
    char *op_code;             // ";", "&"
    struct group_node *next;   // next group
} group_node;

void skip_whitespace();
int peek_char();
int consume_char(char expected);
int consume_string(const char* expected);
int is_name_char(char c);
int parse_name(char** name);
int parse_input_redirection(char** input_file);
int parse_output(char** output_file, int *append);
void add_arg(atomic* cmd, const char* arg);
int parse_atomic(atomic* cmd);
int parse_shell_cmd(group_node** head);
void free_atomic_command(atomic* cmd);
void free_group_list(group_node *head);
int parse_cmd_group(group_node** group);
group_node* parse_input();

#endif