#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 100
#define MAX_ARG_LENGTH 256

typedef struct
{
    int argc;
    char argv[MAX_ARGS][MAX_ARG_LENGTH];
} command_t;

int parse_command(char *input, command_t *cmd);

#endif
