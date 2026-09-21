#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "parser.h"
#include "executor.h"
#include "builtin.h"


/* =========================================================
   MILESTONE 4.2
   BACKGROUND PROCESS HANDLING
   ========================================================= */

static void sigchld_handler(int sig)
{
    int saved_errno = errno;

    (void)sig;

    /*
     * Reap all completed child processes without blocking.
     */
    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
    }

    errno = saved_errno;
}


/* ---------------------------------------------------------
   Install SIGCHLD handler
   --------------------------------------------------------- */

void setup_background_handler(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = sigchld_handler;

    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    if (sigaction(SIGCHLD, &sa, NULL) < 0)
    {
        perror("sigaction SIGCHLD");
    }
}


/* ---------------------------------------------------------
   Manually reap completed background processes
   --------------------------------------------------------- */

void reap_background_processes(void)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
    {
    }
}


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


    /*
     * Built-in command
     */

    if (is_builtin(cmd))
    {
        return execute_builtin(cmd);
    }


    /*
     * Create child process
     */

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return -1;
    }


    /* =====================================================
       CHILD PROCESS
       ===================================================== */

    if (pid == 0)
    {
        char *args[MAX_ARGS + 1];

        for (int i = 0; i < cmd->argc; i++)
        {
            args[i] = cmd->argv[i];
        }

        args[cmd->argc] = NULL;


        /*
         * Execute external command
         */

        execvp(args[0], args);


        /*
         * execvp returns only when execution fails
         */

        perror(args[0]);

        _exit(127);
    }


    /* =====================================================
       PARENT PROCESS
       ===================================================== */


    /*
     * Milestone 4.2
     *
     * If command is a background command,
     * do NOT wait for the child.
     */

    if (cmd->background)
    {
        printf("[Background PID: %d]\n", pid);

        return 0;
    }


    /*
     * Foreground command:
     * parent waits until child finishes.
     */

    if (waitpid(pid, &status, 0) < 0)
    {
        /*
         * SIGCHLD handler may have already
         * handled a completed process.
         */

        if (errno == ECHILD)
        {
            return 0;
        }

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


    /*
     * Validate pipeline
     */

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
        return execute_command(
            &pipeline->commands[0]
        );
    }


    /* =====================================================
       MULTIPLE COMMAND PIPELINE
       ===================================================== */

    for (int i = 0; i < command_count; i++)
    {
        int pipefd[2];


        /*
         * Create a pipe unless this is
         * the final command.
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
             * get input from previous pipe.
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
             * If this is not the final command,
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
             * Close previous pipe
             */

            if (previous_read != -1)
            {
                close(previous_read);
            }


            /*
             * Close current pipe descriptors
             * after dup2().
             */

            if (i < command_count - 1)
            {
                close(pipefd[0]);
                close(pipefd[1]);
            }


            /*
             * Prepare command arguments
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
             * Execute built-in command
             * inside pipeline.
             */

            if (is_builtin(cmd))
            {
                int result =
                    execute_builtin(cmd);

                _exit(
                    result == 0 ? 0 : 1
                );
            }


            /*
             * Execute external command
             */

            execvp(args[0], args);


            /*
             * execvp returns only
             * if execution fails.
             */

            perror(args[0]);

            _exit(127);
        }


        /* =================================================
           PARENT PROCESS
           ================================================= */


        /*
         * Parent no longer needs
         * previous pipe read end.
         */

        if (previous_read != -1)
        {
            close(previous_read);
        }


        /*
         * Save read side of current
         * pipe for next command.
         */

        if (i < command_count - 1)
        {
            close(pipefd[1]);

            previous_read =
                pipefd[0];
        }
        else
        {
            previous_read = -1;
        }
    }


    /* =====================================================
       MILESTONE 4.2
       BACKGROUND PIPELINE
       ===================================================== */


    /*
     * Check whether pipeline should
     * execute in background.
     *
     * We use the background flag of
     * the last command.
     */

    if (pipeline->commands[
            command_count - 1
        ].background)
    {
        printf(
            "[Background pipeline PID: %d]\n",
            pids[command_count - 1]
        );


        /*
         * Do NOT wait.
         *
         * SIGCHLD handler will reap
         * completed child processes.
         */

        return 0;
    }


    /* =====================================================
       FOREGROUND PIPELINE
       ===================================================== */

    int final_status = 0;


    /*
     * Wait for all commands in pipeline.
     */

    for (int i = 0;
         i < command_count;
         i++)
    {
        int status;


        if (waitpid(
                pids[i],
                &status,
                0) < 0)
        {
            /*
             * Child may already have been
             * collected by SIGCHLD handler.
             */

            if (errno == ECHILD)
            {
                continue;
            }


            perror("waitpid");

            continue;
        }


        /*
         * Save status of last command.
         */

        if (i ==
            command_count - 1)
        {
            if (WIFEXITED(status))
            {
                final_status =
                    WEXITSTATUS(status);
            }
            else if (WIFSIGNALED(status))
            {
                final_status =
                    128 +
                    WTERMSIG(status);
            }
        }
    }


    return final_status;
}
