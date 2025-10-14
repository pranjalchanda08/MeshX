
#include "Shell.h"
#include "Parser.h"
#include "execute.h"

#include <errno.h>
#include <signal.h>

// The main REPL for the shell
void shell_loop(void) {
  int status = 1;
  Command cmd;
  char *line = NULL;

  while (status) {
    if (isatty(STDIN_FILENO)) {
      fputs("MeshX > ", stdout);
      fflush(stdout);
    }

    line = read_line();
    if (!line) break; // EOF (Ctrl-D) or fatal error

    if (parse_line(line, &cmd)) {
      status = execute_command(&cmd);
      free_command(&cmd);
    }

    free(line);
    line = NULL;
  }
}

// Read a line of input from stdin
char *read_line(void) {
  char *line = NULL;
  size_t cap = 0;

  for (;;) {
    errno = 0;
    ssize_t n = getline(&line, &cap, stdin);
    if (n >= 0) return line;

    if (feof(stdin)) { // Ctrl-D
      if (isatty(STDIN_FILENO)) putchar('\n');
      free(line);
      return NULL;
    }
    if (errno == EINTR) { // Interrupted by a signal (Ctrl-C)
      clearerr(stdin);
      continue;
    }
    perror("getline");
    free(line);
    return NULL;
  }
}

// Optional: keep shell alive on Ctrl-C
static void sigint_handler(int sig) {
  (void)sig;
  if (isatty(STDIN_FILENO)) write(STDOUT_FILENO, "\n", 1);
}

void setup_signals(void) {
  struct sigaction sa = {0};
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa, NULL);

  sa.sa_handler = SIG_IGN; // Ignore Ctrl-
  sigaction(SIGQUIT, &sa, NULL);
}