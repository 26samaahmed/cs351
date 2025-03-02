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

void parse_command(int argc, char *argv[])
{

    if (argc > 1 && strcmp(argv[argc - 1], "ECHO") == 0)
    {
        int i = 0;
        while (strcmp(argv[i], "<") != 0 && strcmp(argv[i], ">") != 0 && strcmp(argv[i], "|") != 0)
        {
            printf("%s ", argv[i]);
            ++i;
            if (i >= argc)
            {
                break;
            }
        }
        printf("\n");
    }
}

void parse_io(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[argc - 1], "IO") == 0)
    {
        int i = 0;
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

void parse_pipe(int argc, char *argv[])
{
    // if (argc < 3 || strcmp(argv[argc - 1], "PIPE") != 0)
    // {
    //  fprintf(stderr, "Error: Last argument must be 'PIPE'\n");
    // return;
    //}

    if (strcmp(argv[argc - 1], "PIPE") == 0)
    {
        argv[argc - 1] = NULL; // Remove "PIPE" from the argument list
        argc--;

        int num_cmds = 1;
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
                argv[i] = NULL; // Terminate the current command args list

                // Set input and output for the command
                if (cmd_idx == 0)
                {
                    fin = -1; // First command, so use default input (keyboard)
                }
                else
                {
                    fin = pipe_fds[2 * (cmd_idx - 1)]; // Read from previous pipe
                }

                if (cmd_idx < num_cmds - 1)
                {
                    fout = pipe_fds[2 * cmd_idx + 1]; // Write to the next pipe
                }
                else
                {
                    fout = -1; // Last command, so use default stdout
                }
                strcpy(command_array[cmd_idx], argv[cmd_start]);
                execute(&argv[cmd_start], fin, fout);

                // Close used pipe ends in parent process
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

        // loop through command_array in reverse order
        int i = 1;
        while (i < num_cmds)
        {
            printf("PIPE %s ", command_array[i]);
            i++;
        }
        printf("\n");

        // Close all pipe ends in the parent
        for (int i = 0; i < 2 * (num_cmds - 1); i++)
        {
            close(pipe_fds[i]);
        }

        // Wait for all child processes to finish
        for (int i = 0; i < num_cmds; i++)
        {
            wait(NULL);
        }
    }
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
    else if (strncmp(cmd, "!!", 2) == 0)
    {
        if (strlen(recent_command) == 0)
        {
            fprintf(stderr, "No recent command to execute!\n");
        }
        else
        {
            printf("Executing recent command: %s\n", recent_command);
            char *recent_args[ARGS_SIZE];
            int fin = -1, fout = -1;

            // **Reparse the command** to handle `>`, `<`, and `|` properly
            parse(recent_command, recent_args);
            int num_args = count_arguments(recent_args);
            parse_io(num_args, recent_args);
            parse_pipe(num_args, recent_args);
            parse_redirect(recent_args, &fin, &fout);

            execute(recent_args, fin, fout);
        }
    }

    if (strncmp(cmd, "!!", 2) != 0)
    {
        strncpy(recent_command, args[0], BUFSIZ - 1);
        for (int i = 1; args[i] != NULL; i++)
        {
            strncat(recent_command, " ", BUFSIZ - strlen(recent_command) - 1);
            strncat(recent_command, args[i], BUFSIZ - strlen(recent_command) - 1);
        }
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

        // Store command before modifying args
        if (strncmp(cmd, "!!", 2) != 0 && strlen(cmd) > 0)
        {
            strncpy(recent_command, cmd, BUFSIZ - 1);
        }

        parse(cmd, args);
        int num_args = count_arguments(args);
        parse_command(num_args, args);
        parse_io(num_args, args);
        parse_redirect(args, &fin, &fout);
        parse_pipe(num_args, args);

        strings_print(args, fin, fout);

        if (is_local_command(cmd, LOCAL_CMDS, sizeof(LOCAL_CMDS) / sizeof(char *)))
        {
            execute_local_command(args);
            continue; // go to top of while loop
        }

        execute(args, fin, fout);
    }
    return 0;
}

int main(int argc, const char *argv[])
{
    return run_shell(argc, argv);

    return 0;
}