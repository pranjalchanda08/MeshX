#include "Parser.h"

#define MAX_ARGS_STEP 64
#define DELIM " \t\r\n"

int parse_line(char *line, Command *cmd) {
  int cap = MAX_ARGS_STEP;
  int pos = 0;
  char **tokens = malloc((size_t)cap * sizeof *tokens);
  if (!tokens) { perror("malloc"); exit(1); }

  char *save = NULL;
  char *tok = strtok_r(line, DELIM, &save);

  while (tok) {
    if (pos + 1 >= cap) { // +1 for final NULL
      cap += MAX_ARGS_STEP;
      char **tmp = realloc(tokens, (size_t)cap * sizeof *tokens);
      if (!tmp) { perror("realloc"); free(tokens); exit(1); }
      tokens = tmp;
    }
    tokens[pos++] = tok;
    tok = strtok_r(NULL, DELIM, &save);
  }

  if (pos == 0) { // Empty line
    free(tokens);
    return 0;
  }

  tokens[pos] = NULL;
  cmd->args = tokens;
  cmd->arg_count = pos;
  cmd->name = tokens[0];
  return 1;
}

void free_command(Command *cmd) {
  free(cmd->args);
  cmd->args = NULL;
  cmd->name = NULL;
  cmd->arg_count = 0;
}