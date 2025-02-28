#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, const char *argv[])
{
  printf("before fork (pid:%d)\n", (int)getpid());

  int pipefd[2];
  if (pipe(pipefd) == -1)
  {
    perror("pipe failed");
    exit(1);
  }

  int rc_1 = fork();
  if (rc_1 < 0)
  {
    fprintf(stderr, "Fork failed\n");
    exit(1);
  }
  else if (rc_1 == 0)
  {                   // child #1
    close(pipefd[0]); // close read end of pipe in child #1

    // redirecting to the standard output to the pipe's write end
    dup2(pipefd[1], STDOUT_FILENO); // stdout -> pipefd[1]
    printf("Child #1 (pid:%d) is writing to the pipe\n", (int)getpid());

    // now close the write end of the pipe
    close(pipefd[1]);
    exit(0);
  }

  // do same process but with child #2
  int rc_2 = fork();
  if (rc_2 < 0)
  {
    fprintf(stderr, "Fork failed\n");
    exit(1);
  }
  else if (rc_2 == 0)
  {                                // child #2
    close(pipefd[1]);              // close read end of pipe in child #2
    dup2(pipefd[0], STDIN_FILENO); // stdin -> pipefd[0]

    fprintf(stderr, "Child #2 (pip:%d) is reading from the pipe now: \n", (int)getpid());

    // child #2 must read from the pipe now
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), stdin))
    {
      printf("%s", buffer); // print to ensure we succesfully read from the pipe
    }

    close(pipefd[0]);
    exit(0);
  }

  close(pipefd[0]);
  close(pipefd[1]);

  wait(NULL); // wait for child #1 to be done
  wait(NULL); // wait for child #2 to be done
  return 0;
}