#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

void execute()
{
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
    while (*line != '\0' && isspace(*line))
    {
      line++;
    }
  }
  *argv = NULL;
}

int main(int argc, char *argv[])
{
  char buf[BUFSIZ];
  bool finished = false;
  while (!finished)
  {
    memset(buf, 0, sizeof(buf));
    printf("> ");
    if (fgets(buf, sizeof(buf), stdin) == NULL)
    {
      fprintf(stderr, "gets error\n");
      exit(1);
    }
    if (strstr(buf, "exit"))
    {
      exit(0);
    }

    printf("%s\n", buf);
    parse(buf, argv);
    printf("%s\n", argv[0]);
  }

  return 0;
}