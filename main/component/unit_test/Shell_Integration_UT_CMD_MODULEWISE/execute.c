#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include "execute.h"


// This function is now a "private" helper for execute_command.
static void run_exec(char** argv) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], argv);
        perror("execvp failed");
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("fork failed");
    }
}

// This is the main API function.
int execute_command(Command *cmd) {
    if (cmd->name == NULL) {
        return 1; // Continue
    }

    // Handle built-in: 'cd'
    if (strcmp(cmd->name, "cd") == 0) {
        char *dir = (cmd->arg_count > 1) ? cmd->args[1] : getenv("HOME");
        if (dir && chdir(dir) != 0) {
            perror(cmd->args[1]);
        }
        return 1;
    }

    // Handle built-in: 'exit'
    if (strcmp(cmd->name, "exit") == 0) {
        return 0; // Terminate
    }

    // For everything else, use run_exec
    run_exec(cmd->args);
    return 1;
}