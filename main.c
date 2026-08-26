#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "executor.h"

#define INPUT_SIZE 1024

int main()
{
    char input[INPUT_SIZE];
    command_t cmd;

    printf("Welcome to Shellforge!\n");

    while (1)
    {
        printf("shellforge> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL)
        {
            printf("\n");
            break;
        }

        if (strcmp(input, "\n") == 0)
        {
            continue;
        }

        parse_command(input, &cmd);

        execute_command(&cmd);
    }

    return 0;
}
