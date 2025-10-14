#ifndef SHELL_H
#define SHELL_H

// Common includes needed by multiple files
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// The structure for a parsed command
typedef struct {
  char *name;
  char **args;
  int arg_count;
} Command;

// Function prototypes for functions in shell.c
void setup_signals(void);
char *read_line(void);
void shell_loop(void);

#endif // SHELL_H;