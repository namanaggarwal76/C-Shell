#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <linux/limits.h>
#include <fcntl.h>
#include <unistd.h>
#include "parser.h"

extern char input_buffer[ARG_MAX];
int pos;
int len;
int error;

void skip_whitespace(){
    while (pos<len && isspace(input_buffer[pos])){
        pos++;
    }
}

int peek_char(){
    if (pos >= len) return -1;
    return input_buffer[pos];
}

int consume_char(char expected){
    skip_whitespace();
    if (pos>=len || input_buffer[pos]!=expected) {
        return 0;
    }
    pos++;
    return 1;
}

int consume_string(const char* expected){
    skip_whitespace();
    int expected_len = strlen(expected);
    if (pos + expected_len > len) return 0;
    if (strncmp(&input_buffer[pos], expected, expected_len) != 0) return 0;
    pos += expected_len;
    return 1;
}

int is_name_char(char c){
    return c != '|' && c != '&' && c != '>' && c != '<' && c != ';' && c != '\0' && c != '"' && !isspace(c);
}

int parse_name(char** name){
    skip_whitespace();
    int start=pos;
    if (pos<len && input_buffer[pos]=='"'){
        pos++;
        start=pos;
        while (pos<len && input_buffer[pos]!='"') {
            pos++;
        }
        int name_len=pos-start;
        *name=malloc(name_len + 1);
        strncpy(*name,&input_buffer[start],name_len);
        (*name)[name_len] ='\0';
        if (pos<len && input_buffer[pos]=='"') {
            pos++;
        }
        return 1;
    }
    while (pos<len && is_name_char(input_buffer[pos])) {
        pos++;
    }
    if (start==pos){
        error=1;
        return 0;
    }
    int name_len=pos-start;
    *name=malloc(name_len + 1);
    strncpy(*name, &input_buffer[start], name_len);
    (*name)[name_len]='\0';
    return 1;
}

int parse_input_redirection(char** input_file){
    skip_whitespace();
    if (!consume_char('<')){
        return 0;
    }
    skip_whitespace();
    if (!parse_name(input_file)) {
        error=1;
        return 0;
    }
    return 1;
}

int parse_output(char** output_file, int *append){
    skip_whitespace();
    if (consume_string(">>")){
        *append = 1;
    }
    else if (consume_char('>')) {
        *append = 0;
    }
    else{
        return 0;
    }
    skip_whitespace();
    if (!parse_name(output_file)) {
        error=1;
        return 0;
    }
    return 1;
}

void add_input_redirect(atomic* cmd, const char* filename){
    redirect_node* node = malloc(sizeof(redirect_node));
    node->filename = malloc(strlen(filename) + 1);
    strcpy(node->filename, filename);
    node->append = 0; // not used for input
    node->next = NULL;
    
    if (cmd->input_files == NULL){
        cmd->input_files = node;
    } else {
        redirect_node* t = cmd->input_files;
        while(t->next){
            t = t->next;
        }
        t->next = node;
    }
}

void add_output_redirect(atomic* cmd, const char* filename, int append){
    redirect_node* node = malloc(sizeof(redirect_node));
    node->filename = malloc(strlen(filename) + 1);
    strcpy(node->filename, filename);
    node->append = append;
    node->next = NULL;
    
    if (cmd->output_files == NULL){
        cmd->output_files = node;
    } else {
        redirect_node* t = cmd->output_files;
        while(t->next){
            t = t->next;
        }
        t->next = node;
    }
}

void add_arg(atomic* cmd, const char* arg){
    arg_node* node=malloc(sizeof(arg_node));
    node->value=malloc(strlen(arg) + 1);
    strcpy(node->value,arg);
    node->next=NULL;
    if (cmd->args==NULL){
        cmd->args=node;
    }
    else{
        arg_node* t=cmd->args;
        while(t->next){
            t = t->next;
        }
        t->next=node;
    }
}

int parse_atomic(atomic* cmd){
    cmd->args=NULL;
    cmd->input_file=NULL;
    cmd->output_file=NULL;
    cmd->append=0;
    cmd->input_files=NULL;
    cmd->output_files=NULL;
    char* name;
    if (!parse_name(&name)) {
        error=1;
        return 0;
    }
    add_arg(cmd,name);
    free(name);
    while (1){
        skip_whitespace();
        int ch=peek_char();
        if (ch==-1 || ch=='|' || ch =='&' || ch ==';'){
            break;
        }
        char* input_file;
        if (parse_input_redirection(&input_file)){
            add_input_redirect(cmd, input_file);
            // Maintain compatibility - last input wins
            if (cmd->input_file){
                free(cmd->input_file);
            }
            cmd->input_file = input_file;
            continue;
        }
        char* output_file;
        int append;
        if (parse_output(&output_file, &append)){
            add_output_redirect(cmd, output_file, append);
            // Maintain compatibility - last output wins
            if (cmd->output_file){
                free(cmd->output_file);
            }
            cmd->output_file=output_file; 
            cmd->append=append;
            continue;
        }
        if (parse_name(&name)){
            add_arg(cmd,name);
            free(name);
            continue;
        }
        error=1;
        return 0;
    }
    return 1;
}

int parse_shell_cmd(group_node** head){
    *head=NULL;
    group_node** current=head;
    group_node* group;
    if (parse_cmd_group(&group)==0){
        return 0;
    }
    *current=group;
    current=&group->next;
    while (1){
        skip_whitespace();
        char* op=NULL;
        if (consume_char(';')){
            op=(char*)malloc(sizeof(char)*2);
            strcpy(op,";");
        }
        else if (consume_char('&')){
            op=(char*)malloc(sizeof(char)*2);
            strcpy(op,"&");
        }
        else{
            break;
        }
        group->op_code=op;
        skip_whitespace();
        if (pos>=len){
            break;
        }
        if (!parse_cmd_group(&group)){
            error=1;
            return 0;
        }
        *current=group;
        current=&group->next;
    }
    skip_whitespace();
    if (pos<len){
        error=1;
        return 0;
    }
    return 1;
}

void free_atomic_command(atomic* cmd){
    if (!cmd) return;
    arg_node *a=cmd->args;
    while (a){
        arg_node *n=a->next;
        free(a->value);
        free(a);
        a=n;
    }
    free(cmd->input_file);
    free(cmd->output_file);
    
    // Free input redirect list
    redirect_node *ir = cmd->input_files;
    while (ir){
        redirect_node *n = ir->next;
        free(ir->filename);
        free(ir);
        ir = n;
    }
    
    // Free output redirect list  
    redirect_node *or = cmd->output_files;
    while (or){
        redirect_node *n = or->next;
        free(or->filename);
        free(or);
        or = n;
    }
    
    cmd->args=NULL;
    cmd->input_file=NULL;
    cmd->output_file=NULL;
    cmd->input_files=NULL;
    cmd->output_files=NULL;
}

void free_group_list(group_node *head){
    group_node* g=head;
    while (g){
        group_node* n=g->next;
        command_node* c=g->head;
        while (c){
            command_node* n2=c->next;
            free_atomic_command(&c->command);
            free(c);
            c=n2;
        }
        free(g->op_code);
        free(g);
        g=n;
    }
}

group_node* parse_input(){
    pos=0;
    len=strlen(input_buffer);
    error=0;
    group_node* head=NULL;
    if (parse_shell_cmd(&head)==0){
        printf("Invalid Syntax!\n");
        free_group_list(head);
        return NULL;
    }
    return head;
}

int parse_cmd_group(group_node** group){
    *group=malloc(sizeof(group_node));
    (*group)->head=NULL;
    (*group)->op_code=NULL;
    (*group)->next=NULL;
    command_node** current=&((*group)->head);
    command_node* node=malloc(sizeof(command_node));
    node->next=NULL;
    if (!parse_atomic(&node->command)) {
        free(node);
        free(*group);
        return 0;
    }
    *current=node;
    current=&node->next;
    while (1){
        skip_whitespace();
        if (!consume_char('|')) break;
        
        // After consuming |, we should have another atomic command
        skip_whitespace();
        
        // Check if we immediately hit a special character (invalid syntax)
        int ch = peek_char();
        if (ch == -1 || ch == ';' || ch == '&' || ch == '|' || ch == '>' || ch == '<') {
            error = 1;
            return 0;
        }
        
        node=malloc(sizeof(command_node));
        node->next=NULL;
        if (!parse_atomic(&node->command)) {
            error=1;
            free(node);
            return 0;
        }
        *current=node;
        current=&node->next;
    }
    return 1;
}