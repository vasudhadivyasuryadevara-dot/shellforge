#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "executor.h"
#include "builtin.h"


/* =========================================================
   EXECUTE SINGLE COMMAND
   ========================================================= */

int execute_command(command_t *cmd)
{
    pid_t pid;
    int status;

    if (cmd == NULL || cmd->argc == 0)
    {
        return -1;
    }

    /* Check whether it is a built-in command */
    if (is_builtin(cmd))
    {
        return execute_builtin(cmd);
    }

    /* Create child process */
    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return -1;
    }

    /* Child process */
    if (pid == 0)
    {
        char *args[MAX_ARGS + 1];

        for (int i = 0; i < cmd->argc; i++)
        {
            args[i] = cmd->argv[i];
        }

        args[cmd->argc] = NULL;

        /* Execute command */
        execvp(args[0], args);

        /* Runs only if execvp fails */
        perror(args[0]);

        _exit(127);
    }

    /* Parent waits for child */
    if (waitpid(pid, &status, 0) < 0)
    {
        perror("waitpid");
        return -1;
    }

    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status))
    {
        fprintf(stderr,
                "Process terminated by signal %d\n",
                WTERMSIG(status));

        return -1;
    }

    return -1;
}


/* =========================================================
   EXECUTE PIPELINE
   ========================================================= */

int execute_pipeline(pipeline_t *pipeline)
{
    int previous_read = -1;

    pid_t pids[MAX_COMMANDS];

    int command_count;

    /* Check pipeline */
    if (pipeline == NULL)
    {
        return -1;
    }

    command_count = pipeline->command_count;

    if (command_count == 0)
    {
        return -1;
    }


    /*
     * If there is only one command,
     * execute it normally.
     */

    if (command_count == 1)
    {
        return execute_command(&pipeline->commands[0]);
    }


    /*
     * Multiple command pipeline
     */

    for (int i = 0; i < command_count; i++)
    {
        int pipefd[2];


        /*
         * Create pipe unless this
         * is the last command.
         */

        if (i < command_count - 1)
        {
            if (pipe(pipefd) == -1)
            {
                perror("pipe");
                return -1;
            }
        }


        /*
         * Create child process
         */

        pids[i] = fork();

        if (pids[i] < 0)
        {
            perror("fork");
            return -1;
        }


        /* =================================================
           CHILD PROCESS
           ================================================= */

        if (pids[i] == 0)
        {
            command_t *cmd =
                &pipeline->commands[i];


            /*
             * INPUT REDIRECTION
             *
             * If this is not the first command,
             * receive input from previous pipe.
             */

            if (previous_read != -1)
            {
                if (dup2(previous_read,
                         STDIN_FILENO) == -1)
                {
                    perror("dup2 input");
                    _exit(EXIT_FAILURE);
                }
            }


            /*
             * OUTPUT REDIRECTION
             *
             * If this is not the last command,
             * send output into current pipe.
             */

            if (i < command_count - 1)
            {
                if (dup2(pipefd[1],
                         STDOUT_FILENO) == -1)
                {
                    perror("dup2 output");
                    _exit(EXIT_FAILURE);
                }
            }


            /*
             * Close unnecessary file descriptors
             */

            if (previous_read != -1)
            {
                close(previous_read);
            }

            if (i < command_count - 1)
            {
                close(pipefd[0]);
                close(pipefd[1]);
            }


            /*
             * Prepare arguments for execvp
             */

            char *args[MAX_ARGS + 1];

            for (int j = 0;
                 j < cmd->argc;
                 j++)
            {
                args[j] = cmd->argv[j];
            }

            args[cmd->argc] = NULL;


            /*
             * Built-in command inside pipeline
             */

            if (is_builtin(cmd))
            {
                int result =
                    execute_builtin(cmd);

                _exit(result == 0 ? 0 : 1);
            }


            /*
             * Execute external command
             */

            execvp(args[0], args);


            /*
             * execvp only returns when
             * execution fails.
             */

            perror(args[0]);

            _exit(127);
        }


        /* =================================================
           PARENT PROCESS
           ================================================= */


        /*
         * Parent no longer needs the
         * previous pipe read end.
         */

        if (previous_read != -1)
        {
            close(previous_read);
        }


        /*
         * Save current pipe read end
         * for next command.
         */

        if (i < command_count - 1)
        {
            close(pipefd[1]);

            previous_read = pipefd[0];
        }
        else
        {
            previous_read = -1;
        }
    }


    /*
     * Wait for all child processes
     */

    int final_status = 0;

    for (int i = 0; i < command_count; i++)
    {
        int status;

        if (waitpid(pids[i],
                    &status,
                    0) < 0)
        {
            perror("waitpid");
            continue;
        }


        /*
         * Save status of final command
         */

        if (i == command_count - 1)
        {
            if (WIFEXITED(status))
            {
                final_status =
                    WEXITSTATUS(status);
            }
        }
    }


    return final_status;
}
