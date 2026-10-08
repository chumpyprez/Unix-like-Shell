/*
 * Unix-Like Shell
 * ----------------
 * A compact Unix-style command shell implemented in C.
 *
 * Features:
 *   - Built-in commands: cd, pwd, exit
 *   - External command execution using fork()/execvp()
 *   - Input redirection:  <
 *   - Output redirection: >
 *   - Append redirection:  >>
 *   - Pipelines using: |
 *   - Multiple commands in a pipeline
 *
 * Build:
 *   gcc -Wall -Wextra -std=c11 -o unix_shell shell.c
 *
 * Run:
 *   ./unix_shell
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_LINE 4096
#define MAX_ARGS 128
#define MAX_CMDS 32

typedef struct {
    char *argv[MAX_ARGS];
    int argc;

    char *input_file;
    char *output_file;
    int append_output;
} Command;

/* ---------- Utility ---------- */

static void print_prompt(void)
{
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) != NULL)
        printf("unix-shell:%s$ ", cwd);
    else
        printf("unix-shell$ ");

    fflush(stdout);
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;

    if (*s == '\0')
        return s;

    char *end = s + strlen(s) - 1;

    while (end > s && isspace((unsigned char)*end))
        end--;

    end[1] = '\0';
    return s;
}

/*
 * Tokenize a command while keeping |, <, > and >> as separate tokens.
 * This intentionally provides a compact parser rather than attempting to
 * implement the complete POSIX shell grammar and quoting rules.
 */
static int tokenize(char *line, char *tokens[], int max_tokens)
{
    int count = 0;
    char *p = line;

    while (*p != '\0') {
        while (isspace((unsigned char)*p))
            p++;

        if (*p == '\0')
            break;

        if (*p == '|') {
            if (count >= max_tokens)
                return -1;

            tokens[count++] = p;
            *p++ = '\0';
            continue;
        }

        if (*p == '<') {
            if (count >= max_tokens)
                return -1;

            tokens[count++] = p;
            *p++ = '\0';
            continue;
        }

        if (*p == '>') {
            if (count >= max_tokens)
                return -1;

            tokens[count++] = p;

            if (*(p + 1) == '>') {
                *p++ = '\0';
                *p++ = '\0';
            } else {
                *p++ = '\0';
            }

            continue;
        }

        char *start = p;

        while (*p != '\0' &&
               !isspace((unsigned char)*p) &&
               *p != '|' &&
               *p != '<' &&
               *p != '>') {
            p++;
        }

        if (*p != '\0') {
            *p++ = '\0';
        }

        if (count >= max_tokens)
            return -1;

        tokens[count++] = start;
    }

    return count;
}

/* ---------- Parsing ---------- */

static int parse_line(char *line, Command commands[], int *num_commands)
{
    char *tokens[MAX_ARGS * 2];
    int token_count = tokenize(line, tokens, MAX_ARGS * 2);

    if (token_count < 0) {
        fprintf(stderr, "shell: command too long\n");
        return -1;
    }

    if (token_count == 0)
        return 0;

    int cmd_index = 0;
    commands[0].argc = 0;
    commands[0].input_file = NULL;
    commands[0].output_file = NULL;
    commands[0].append_output = 0;

    for (int i = 0; i < token_count; i++) {
        char *tok = tokens[i];

        if (strcmp(tok, "|") == 0) {
            if (commands[cmd_index].argc == 0) {
                fprintf(stderr, "shell: invalid pipeline\n");
                return -1;
            }

            commands[cmd_index].argv[commands[cmd_index].argc] = NULL;

            cmd_index++;

            if (cmd_index >= MAX_CMDS) {
                fprintf(stderr, "shell: too many pipeline stages\n");
                return -1;
            }

            commands[cmd_index].argc = 0;
            commands[cmd_index].input_file = NULL;
            commands[cmd_index].output_file = NULL;
            commands[cmd_index].append_output = 0;
        }
        else if (strcmp(tok, "<") == 0) {
            if (i + 1 >= token_count ||
                strcmp(tokens[i + 1], "|") == 0 ||
                strcmp(tokens[i + 1], "<") == 0 ||
                strcmp(tokens[i + 1], ">") == 0) {
                fprintf(stderr, "shell: missing input file\n");
                return -1;
            }

            commands[cmd_index].input_file = tokens[++i];
        }
        else if (strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0) {
            if (i + 1 >= token_count ||
                strcmp(tokens[i + 1], "|") == 0 ||
                strcmp(tokens[i + 1], "<") == 0 ||
                strcmp(tokens[i + 1], ">") == 0 ||
                strcmp(tokens[i + 1], ">>") == 0) {
                fprintf(stderr, "shell: missing output file\n");
                return -1;
            }

            commands[cmd_index].output_file = tokens[++i];
            commands[cmd_index].append_output = (strcmp(tok, ">>") == 0);
        }
        else {
            if (commands[cmd_index].argc >= MAX_ARGS - 1) {
                fprintf(stderr, "shell: too many arguments\n");
                return -1;
            }

            commands[cmd_index].argv[commands[cmd_index].argc++] = tok;
        }
    }

    if (commands[cmd_index].argc == 0) {
        fprintf(stderr, "shell: incomplete command\n");
        return -1;
    }

    commands[cmd_index].argv[commands[cmd_index].argc] = NULL;
    *num_commands = cmd_index + 1;

    return 1;
}

/* ---------- Built-ins ---------- */

static int builtin_pwd(void)
{
    char cwd[1024];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return 1;
    }

    printf("%s\n", cwd);
    return 0;
}

static int builtin_cd(Command *cmd)
{
    const char *path = NULL;

    if (cmd->argc > 2) {
        fprintf(stderr, "cd: too many arguments\n");
        return 1;
    }

    if (cmd->argc == 1) {
        path = getenv("HOME");

        if (path == NULL) {
            fprintf(stderr, "cd: HOME is not set\n");
            return 1;
        }
    } else {
        path = cmd->argv[1];
    }

    if (chdir(path) != 0) {
        perror("cd");
        return 1;
    }

    return 0;
}

static int handle_builtin(Command *cmd)
{
    if (cmd->argc == 0)
        return 1;

    if (strcmp(cmd->argv[0], "cd") == 0)
        return builtin_cd(cmd);

    if (strcmp(cmd->argv[0], "pwd") == 0)
        return builtin_pwd();

    if (strcmp(cmd->argv[0], "exit") == 0)
        return 2;

    return 0;
}

/* ---------- Redirection ---------- */

static int apply_redirection(Command *cmd)
{
    if (cmd->input_file != NULL) {
        int fd = open(cmd->input_file, O_RDONLY);

        if (fd < 0) {
            perror(cmd->input_file);
            return -1;
        }

        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2");
            close(fd);
            return -1;
        }

        close(fd);
    }

    if (cmd->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT;

        flags |= cmd->append_output ? O_APPEND : O_TRUNC;

        int fd = open(cmd->output_file, flags, 0644);

        if (fd < 0) {
            perror(cmd->output_file);
            return -1;
        }

        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            close(fd);
            return -1;
        }

        close(fd);
    }

    return 0;
}

/* ---------- Pipeline Execution ---------- */

static int execute_pipeline(Command commands[], int count)
{
    int prev_read = -1;
    pid_t pids[MAX_CMDS];

    for (int i = 0; i < count; i++) {
        int pipefd[2] = {-1, -1};

        if (i < count - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                return 1;
            }
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");

            if (pipefd[0] != -1) close(pipefd[0]);
            if (pipefd[1] != -1) close(pipefd[1]);

            return 1;
        }

        if (pid == 0) {
            /* Connect previous pipeline stage to stdin. */
            if (prev_read != -1) {
                if (dup2(prev_read, STDIN_FILENO) < 0) {
                    perror("dup2");
                    _exit(1);
                }
            }

            /* Connect this stage to the next pipeline stage. */
            if (i < count - 1) {
                if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    _exit(1);
                }
            }

            if (prev_read != -1)
                close(prev_read);

            if (pipefd[0] != -1)
                close(pipefd[0]);

            if (pipefd[1] != -1)
                close(pipefd[1]);

            if (apply_redirection(&commands[i]) != 0)
                _exit(1);

            execvp(commands[i].argv[0], commands[i].argv);

            fprintf(stderr, "shell: %s: %s\n",
                    commands[i].argv[0], strerror(errno));
            _exit(127);
        }

        pids[i] = pid;

        if (prev_read != -1)
            close(prev_read);

        if (i < count - 1) {
            close(pipefd[1]);
            prev_read = pipefd[0];
        } else {
            prev_read = -1;
        }
    }

    int status = 0;

    for (int i = 0; i < count; i++) {
        if (waitpid(pids[i], &status, 0) < 0)
            perror("waitpid");
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);

    return 1;
}

/* ---------- Main Loop ---------- */

int main(void)
{
    char line[MAX_LINE];

    while (1) {
        Command commands[MAX_CMDS];
        int command_count = 0;

        print_prompt();

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        char *input = trim(line);

        if (*input == '\0')
            continue;

        int parse_result = parse_line(input, commands, &command_count);

        if (parse_result <= 0)
            continue;

        /*
         * Built-ins are handled by the shell process so that state-changing
         * commands such as cd affect the shell itself.
         *
         * For simplicity, built-ins are handled directly when the command
         * line contains a single command. A built-in appearing inside a
         * pipeline is treated as an external command and will fail unless
         * the operating system provides it.
         */
        if (command_count == 1) {
            int builtin_result = handle_builtin(&commands[0]);

            if (builtin_result == 2)
                break;

            if (builtin_result == 1)
                continue;
        }

        execute_pipeline(commands, command_count);
    }

    return 0;
}
