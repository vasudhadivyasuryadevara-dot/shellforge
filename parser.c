#include <stdio.h>
#include <string.h>
#include "parser.h"

int parse_command(char *input, command_t *cmd)
{
    char *token;

    cmd->argc = 0;

    token = strtok(input, " \t\n");

    while (token != NULL && cmd->argc < MAX_ARGS)
    {
        strcpy(cmd->argv[cmd->argc], token);
        cmd->argc++;

        token = strtok(NULL, " \t\n");
    }

    return cmd->argc;
}
