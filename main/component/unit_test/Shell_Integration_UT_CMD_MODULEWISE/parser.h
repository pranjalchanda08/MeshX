#ifndef PARSER_H
#define PARSER_H

#include "Shell.h"

// Function prototypes for functions in parser.c
int parse_line(char *line, Command *cmd);
void free_command(Command *cmd);

#endif // PARSER_H