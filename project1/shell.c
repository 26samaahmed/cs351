#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define MAX_COMMAND_LENGTH 256
#define MAX_LENGTH 100
#define MAX_STRINGS 5
#define ARGS_SIZE 64

const char *LOCAL_CMDS[] = {"cd", "mkdir", "exit", "help", "!!"};
char recent_command[BUFSIZ] = "";
char command_array[MAX_STRINGS][MAX_LENGTH];

int count_arguments(char *arguments[])
{
    int count = 0;
    while (arguments[count] != NULL)
    {
        ++count;
    }
    return count;
}

// Example command: cal feb 2025 > t.txt ECHO
void parse_echo(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[argc - 1], "ECHO") == 0)
    {
        int i = 0;
        // Loop until we reach any of the redirection or pipe symbols
        while (strcmp(argv[i], "<") != 0 && strcmp(argv[i], ">") != 0 && strcmp(argv[i], "|") != 0)
        {
            printf("%s ", argv[i]);
            ++i;

            // Ensure we don't go out of bounds
            if (i >= argc)
            {
                break;
            }
        }
        printf("\n");
    }
}

// Example command: cal feb 2025 > t.txt IO (if t.txt exists)
void parse_io(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[argc - 1], "IO") == 0)
    {
        int i = 0;

        // Loop until we reach any of the redirection symbols
        while (strcmp(argv[i], "<") != 0 && strcmp(argv[i], ">") != 0)
        {
            ++i;
            if (i >= argc)
            {
                break;
            }
        }

        if (strcmp(argv[i], ">") == 0)
        {
            printf("GT %s\n", argv[i + 1]);
        }
        else if (strcmp(argv[i], "<") == 0)
        {
            printf("LT %s\n", argv[i + 1]);
        }
    }
}

void strings_print(char *args[], int fin, int fout)
{
    char **q = args;
    int i = 0;
    while (*q != NULL)
    {
        printf("args[%d] is %s\n", i++, *q++);
    }
    printf("fin is %d, fout is %d\n", fin, fout);
    printf("============================\n");
}

int execute(char **args, int fin, int fout);

// Example command: cat shell.c | wc PIPE
void parse_pipe(int argc, char *argv[])
{
    // Checking if the last argument is "PIPE"
    if (argc > 1 && strcmp(argv[argc - 1], "PIPE") == 0)
    {
        // Removing "PIPE" from the argument list to avoid executing it
        argv[argc - 1] = NULL;
        argc--;

        int num_cmds = 1;

        // Count the number of commands in the argument list by counting the number of pipes since each pipe separates two commands
        for (int i = 0; i < argc; i++)
        {
            if (strcmp(argv[i], "|") == 0)
                num_cmds++;
        }

        int pipe_fds[2 * (num_cmds - 1)];
        for (int i = 0; i < num_cmds - 1; i++)
        {
            if (pipe(pipe_fds + 2 * i) < 0)
            {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        int cmd_start = 0;
        int fin = -1, fout = -1;
        for (int i = 0, cmd_idx = 0; i <= argc; i++)
        {
            if (i == argc || strcmp(argv[i], "|") == 0)
            {
                argv[i] = NULL; // Terminating the current command args list

                // Setting input and output for the command
                if (cmd_idx == 0)
                {
                    fin = -1; // First command, so use default input which is keyboard
                }
                else
                {
                    fin = pipe_fds[2 * (cmd_idx - 1)]; // Reading from previous pipe
                }

                if (cmd_idx < num_cmds - 1)
                {
                    fout = pipe_fds[2 * cmd_idx + 1]; // Writing to the next pipe
                }
                else
                {
                    fout = -1; // Last command, so use default stdout
                }
                strcpy(command_array[cmd_idx], argv[cmd_start]);
                execute(&argv[cmd_start], fin, fout);

                // Closinf used pipe ends in parent process
                if (fin != -1)
                {
                    close(fin);
                }
                if (fout != -1)
                {
                    close(fout);
                }

                cmd_start = i + 1; // Move to the next command
                cmd_idx++;
            }
        }

        // Looping through commands array to print the commands if it's not the first command
        int i = 1;
        while (i < num_cmds)
        {
            printf("PIPE %s ", command_array[i]);
            i++;
        }
        printf("\n");

        // Closinf all pipe ends in the parent
        for (int i = 0; i < 2 * (num_cmds - 1); i++)
        {
            close(pipe_fds[i]);
        }

        // Waitinf for all child processes to finish
        for (int i = 0; i < num_cmds; i++)
        {
            wait(NULL);
        }
    }
}

// Example command: cal mar 2025 | wc
int execute_with_pipe(char **args_one, char **args_two)
{
    int pipe_fd[2]; // Array to store pipe file descriptors

    // Creating the pipe
    if (pipe(pipe_fd) == -1)
    {
        perror("Pipe failed");
        exit(1);
    }

    // Forking the first child process for Program A which is the cal command
    pid_t pid1 = fork();
    if (pid1 < 0)
    {
        perror("Fork failed");
        exit(1);
    }

    if (pid1 == 0)
    { // Child process 1: Executing Program A
        // Redirecting stdout to the write end of the pipe
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[0]); // Close unused read end
        close(pipe_fd[1]); // Close pipe write end after dup2

        // Executing the first command
        if (execvp(args_one[0], args_one) == -1)
        {
            perror("execvp failed for Program A");
            exit(1);
        }
    }

    // Forking the second child process for Program B
    pid_t pid2 = fork();
    if (pid2 < 0)
    {
        perror("Fork failed");
        exit(1);
    }

    if (pid2 == 0)
    { // Child process 2: Execute Program B
        // Redirecting stdin to the read end of the pipe
        dup2(pipe_fd[0], STDIN_FILENO);
        close(pipe_fd[1]); // Close unused write end
        close(pipe_fd[0]); // Close pipe read end after dup2

        // Executing the second command
        if (execvp(args_two[0], args_two) == -1)
        {
            perror("execvp failed for Program B");
            exit(1);
        }
    }

    // Parent process: Closing both ends of the pipe
    close(pipe_fd[0]); // Close read end
    close(pipe_fd[1]); // Close write end

    // Waiting for both child processes to complete
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;
}

int prompt_get_command(char *buf)
{
    memset(buf, 0, BUFSIZ);

    printf("> ");
    char *p = fgets(buf, BUFSIZ, stdin);
    unsigned long len = strlen(p);
    buf[len - 1] = '\0';
    return len;
}

bool is_local_command(const char *cmd, const char *local_cmds[], int size)
{
    for (int i = 0; i < size; ++i)
    {
        if (strcmp(cmd, local_cmds[i]) == 0)
        {
            printf("%s is a local command\n", cmd);
            printf("============================\n");
            return true;
        }
    }
    return false;
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
    *argv = NULL;
}

void parse_redirect(char **argv, int *fin, int *fout)
{
    char **q = argv;
    while (*q != NULL)
    {
        if (strncmp(*q, "<", 1) == 0)
        {
            *q++ = NULL;
            *fin = open(*q, O_RDONLY, 0644);
            *q++ = NULL; // 0 out ">" and filename
        }
        else if (strncmp(*q, ">", 1) == 0)
        {
            *q++ = NULL;
            *fout = open(*q, O_CREAT | O_TRUNC | O_RDWR, 0644);
            *q++ = NULL;
        }
        else
        {
            ++q;
        }
    }
}

int execute_local_command(char **args)
{
    int rc = 0;
    const char *cmd = args[0];

    if (strncmp(cmd, "exit", 4) == 0)
    {
        fprintf(stdout, "exiting shell...\n");
        exit(0);
    }
    else if (strncmp(cmd, "help", 4) == 0)
    {
        fprintf(stdout, "Welcome to my shell program. "
                        "Type any system cmd or exit or cd or mkdir\n");
    }
    else if (strncmp(cmd, "cd", 2) == 0)
    {
        rc = chdir(args[1]);
        if (rc == 0)
        {
            printf("changing directory to %s\n", args[1]);
        }
        else
        {
            fprintf(stderr, "failed to change directory to \'%s\'\n", args[1]);
        }
    }
    else if (strncmp(cmd, "mkdir", 5) == 0)
    {
        rc = mkdir(args[1], 0755);
        if (rc == 0)
        {
            printf("making new directory %s\n", args[1]);
        }
        else
        {
            fprintf(stderr, "failed to make directory \'%s\'\n", args[1]);
        }
    }
    else if (strncmp(cmd, "!!", 2) == 0) // Executing last command if !! is entered
    {
        if (recent_command[0] == '\0')
        {
            fprintf(stderr, "No recent command to execute\n");
            return 1;
        }

        printf("Executing recent command -> %s\n", recent_command);

        char *temp_args[ARGS_SIZE];
        int fin = -1, fout = -1;

        parse(recent_command, temp_args);

        if (temp_args[0] == NULL)
        {
            fprintf(stderr, "Invalid recent command\n");
            return 1;
        }

        int num_args = count_arguments(temp_args);

        // Reapplying parsing for redirection and pipes
        parse_redirect(temp_args, &fin, &fout);
        parse_pipe(num_args, temp_args);

        // Debugging output
        printf("Redirection applied - fin: %d, fout: %d\n", fin, fout);
        strings_print(temp_args, fin, fout);

        if (is_local_command(temp_args[0], LOCAL_CMDS, sizeof(LOCAL_CMDS) / sizeof(char *)))
        {
            fprintf(stderr, "Cannot execute !! recursively\n");
            return 1;
        }

        // Updating recent_command before executing
        strncpy(recent_command, recent_command, BUFSIZ - 1);
        recent_command[BUFSIZ - 1] = '\0';

        return execute(temp_args, fin, fout);
    }

    return rc;
}

int execute(char **args, int fin, int fout)
{
    int ex_rc = 0;
    // don't forget to reset these or console input/output be ignored
    int stdin_orig = dup(STDIN_FILENO);
    int stdout_orig = dup(STDOUT_FILENO);

    if (fin != -1)
    {
        dup2(fin, STDIN_FILENO);
    }
    if (fout != -1)
    {
        dup2(fout, STDOUT_FILENO);
    }

    int rc = fork();
    if (rc < 0)
    {
        perror("Fork failed in execute()\n");
    }
    else if (rc == 0)
    { // child replaced by execvp and runs
        ex_rc = execvp(args[0], args);
        if (ex_rc != 0)
        {
            perror("execvp failure in execute()\n");
        }
    }
    else
    { // parent waiting for child to execute...
        wait(&ex_rc);
        if (ex_rc != 0)
        {
            fprintf(stderr, "execvp returned %d\n", ex_rc);
        }
        // reset fin, fout back to original values
        //    or you will not be able to type input or see output
    }

    dup2(stdin_orig, STDIN_FILENO);
    dup2(stdout_orig, STDOUT_FILENO);
    close(stdin_orig);
    close(stdout_orig);
    return ex_rc;
}

int run_shell(int argc, const char **argv)
{
    char cmd[BUFSIZ];
    bool finished = false;
    char *args[ARGS_SIZE];
    int fin = -1, fout = -1;

    while (!finished)
    {
        fin = -1;
        fout = -1;

        prompt_get_command(cmd);
        if (strncmp(cmd, "!!", 2) == 0)
        {
            if (recent_command[0] == '\0')
            {
                fprintf(stderr, "No recent command to execute\n");
                continue;
            }
            printf("Executing recent command -> %s\n", recent_command);
            strncpy(cmd, recent_command, BUFSIZ - 1);
            cmd[BUFSIZ - 1] = '\0';
        }
        else
        {
            strncpy(recent_command, cmd, BUFSIZ - 1);
            recent_command[BUFSIZ - 1] = '\0';
        }

        parse(cmd, args);
        int num_args = count_arguments(args);
        parse_echo(num_args, args);
        parse_io(num_args, args);
        parse_pipe(num_args, args);
        parse_redirect(args, &fin, &fout);
        strings_print(args, fin, fout);

        // Detecting pipes in the command
        char **pipe_position = args;
        int pipe_index = 0;
        while (*pipe_position != NULL && strcmp(*pipe_position, "|") != 0)
        {
            pipe_position++;
            pipe_index++;
        }

        // If a pipe is detected, call execute_with_pipe
        if (*pipe_position != NULL)
        {
            char *args_one[1024];
            for (int i = 0; i < pipe_index; i++)
            {
                args_one[i] = args[i];
            }
            args_one[pipe_index] = NULL;

            char **args_two = pipe_position + 1;
            execute_with_pipe(args_one, args_two);
            continue;
        }

        // Handle local commands
        if (is_local_command(args[0], LOCAL_CMDS, sizeof(LOCAL_CMDS) / sizeof(char *)))
        {
            if (execute_local_command(args) == 0)
            {
                continue;
            }
        }

        // Execute normal commands
        execute(args, fin, fout);
    }
    return 0;
}

int main(int argc, const char *argv[])
{
    return run_shell(argc, argv);
    return 0;
}