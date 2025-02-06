#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#define ARGUMENTS_SIZE 64

void execute(char **argv)
{
  pid_t pid;
  int status;
  if ((pid = fork()) < 0)
  { // fork a child process
    printf("*** ERROR: forking child process failed\n");
    exit(1);
  }
  else if (pid == 0)
  { // for the child process:
    if (execvp(*argv, argv) < 0)
    { // execute the command
      printf("*** ERROR: exec failed\n");
      exit(1);
    }
  }
  else
  {                              // for the parent:
    while (wait(&status) != pid) // wait for completion
      ;
  }
}

void parse(char *line, char **argv)
{
  while (*line != '\0')
  {
    while (isspace(*line))
    {
      *line++ = '\0';
    }
    *argv++ = line;
    while (*line != '\0' && !isspace(*line))
    {
      line++;
    }
  }
  printf("exiting...\n");
  *argv = NULL;
}

int main(int argc, const char *argv[])
{
  char buf[BUFSIZ];
  bool finished = false;
  while (!finished)
  {
    memset(buf, 0, sizeof(buf));
    printf("> ");
    char *p = fgets(buf, BUFSIZ, stdin);
    buf[strlen(p) - 1] = '\0';
    printf("You entered %lu characters: %s\n", strlen(p), buf);
    if (strncmp(buf, "exit", 4) == 0)
    {
      printf("exiting shell...\n");
      exit(0);
    }
    else if (strncmp(buf, "help", 4) == 0)
    {
      printf("Welcome to my shell program. "
             "Type any system command or exit or cd or mkdir\n");
      continue;
    }
    char *arguments[ARGUMENTS_SIZE];
    memset(arguments, 0, ARGUMENTS_SIZE * sizeof(char *));
    parse(buf, arguments);
    // printf("strlen(arguments[0] is %lu\n", strlen(arguments[0]));
    // printf("strlen(arguments[1] is %lu\n", strlen(arguments[1]));
    // printf("strlen(arguments[2] is %lu\n", strlen(arguments[2]));
    if (arguments[3] == NULL)
    {
      printf("successfully terminated\n");
    }
    char **q = arguments;
    while (*q != NULL)
    {
      printf("arg is %s\n", *q);
      ++q;
    }
    execute(arguments);
  }
  return 0;
}
