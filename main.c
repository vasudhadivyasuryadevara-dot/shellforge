#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "history.h"
#include "token.h"
#include "lexer.h"
#include "parser.h"
#include "expand.h"
#include "builtin.h"
#include "executor.h"

int main(void)
{
    /* Display welcome message */
    printf("=====================================\n");
    printf("      Shellforge\n");
    printf(" A Unix Style Shell written in C\n");
    printf("=====================================\n");

    using_history();

    token_list_t tokens;
    pipeline_t pipeline;

    char *line;

    while (1)
    {
        /* Read command from user */
        line = readline("shellforge$ ");

        if (line == NULL)
        {
            printf("\nGoodbye!\n");
            break;
        }

        /* Ignore empty input */
        if (strlen(line) == 0)
        {
            free(line);
            continue;
        }

        /* =========================
           Milestone 1 - History
           ========================= */

        if (strcmp(line, "history") == 0)
        {
            print_history();
            free(line);
            continue;
        }

        add_history(line);


        /* =========================
           Milestone 2.1
           Tokenization / Lexer
           ========================= */

        lexer(line, &tokens);


        /* =========================
           Milestone 2.2
           Parser + Variable Expansion
           ========================= */

        if (parser(&tokens, &pipeline))
        {
            expand_variables(&pipeline);
        }


        /* =========================
           Exit command
           ========================= */

        if (pipeline.command_count == 1 &&
            pipeline.commands[0].argc > 0 &&
            strcmp(pipeline.commands[0].argv[0],
                   "exit") == 0)
        {
            free(line);
            break;
        }


        /* =========================
           Milestone 4.1
           Pipeline Execution
           ========================= */

        execute_pipeline(&pipeline);


        free(line);
    }

    return 0;
}
