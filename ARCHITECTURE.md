# Unix-Like Shell — Architecture

## 1. Overview

The shell is an interactive C program that continuously:

1. Displays a prompt.
2. Reads a command line.
3. Parses commands, arguments, pipes, and redirection operators.
4. Handles shell built-ins in the parent process.
5. Creates child processes for external commands.
6. Connects processes using pipes when required.
7. Redirects file descriptors when requested.
8. Waits for child processes to complete.

## 2. High-Level Architecture

```text
             +----------------+
             |   User Input   |
             +-------+--------+
                     |
                     v
             +---------------+
             | Command Parser|
             +-------+-------+
                     |
              +------+------+
              |             |
              v             v
        Built-in?       External?
              |             |
       +------+-----+       v
       |            |    fork()
      cd           pwd      |
       |            |       v
       |            |    Child
       |            |       |
       |            |    execvp()
       |            |       |
       +------+-----+       v
              |       External Program
              |
              +----------------------+
                                     |
                                     v
                                  waitpid()
```

## 3. Command Parsing

The parser converts an input line into one or more `Command` structures.

Each command stores:

```c
typedef struct {
    char *argv[MAX_ARGS];
    int argc;

    char *input_file;
    char *output_file;
    int append_output;
} Command;
```

This separates:

- Program name
- Arguments
- Input file
- Output file
- Append mode

The parser recognizes:

```text
|
<
>
>>
```

## 4. Built-in Commands

Some commands must execute inside the shell process itself.

### `cd`

```c
chdir(path);
```

`cd` changes the current working directory of the shell. Therefore it cannot simply be executed in a child process, because the parent shell's directory would remain unchanged.

### `pwd`

Uses:

```c
getcwd()
```

to obtain and display the current working directory.

### `exit`

Terminates the shell's main loop.

## 5. External Command Execution

External commands use the standard Unix process model:

```text
Parent Shell
     |
     | fork()
     +------------------+
     |                  |
     v                  v
Parent               Child
                        |
                        | execvp()
                        v
                  External Command
```

The parent waits using:

```c
waitpid()
```

This prevents the shell from immediately accepting another command while the foreground process is still executing.

## 6. Pipe Implementation

For:

```bash
ls | grep ".c"
```

the shell creates a pipe:

```c
int pipefd[2];
pipe(pipefd);
```

The pipe provides:

```text
pipefd[0] = read end
pipefd[1] = write end
```

The first process redirects its standard output:

```c
dup2(pipefd[1], STDOUT_FILENO);
```

The second process redirects its standard input:

```c
dup2(pipefd[0], STDIN_FILENO);
```

Conceptually:

```text
+---------+       pipe        +---------+
|   ls    | -- stdout --->    |  grep   |
| process |                   | process |
+---------+                   +---------+
```

## 7. Multiple Pipelines

The implementation supports multiple stages:

```bash
cat file.txt | grep error | wc -l
```

The process chain becomes:

```text
cat
 |
 v
pipe
 |
 v
grep
 |
 v
pipe
 |
 v
wc
```

Each child process inherits the appropriate pipe descriptor and redirects its standard input/output with `dup2()`.

## 8. Input Redirection

For:

```bash
sort < input.txt
```

the shell opens the file:

```c
open("input.txt", O_RDONLY);
```

and connects it to standard input:

```c
dup2(fd, STDIN_FILENO);
```

Result:

```text
input.txt
    |
    v
stdin of sort
    |
    v
  sort
```

## 9. Output Redirection

For:

```bash
ls > listing.txt
```

the shell opens the output file with create/truncate behavior:

```c
open("listing.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
```

Then:

```c
dup2(fd, STDOUT_FILENO);
```

Result:

```text
ls
 |
 v
stdout
 |
 v
listing.txt
```

## 10. Append Redirection

For:

```bash
echo "hello" >> output.txt
```

the shell opens the destination using:

```c
O_WRONLY | O_CREAT | O_APPEND
```

so existing contents are preserved.

## 11. File Descriptors

The shell relies heavily on Unix file descriptors:

```text
0 → stdin
1 → stdout
2 → stderr
```

`dup2()` allows a file or pipe to replace one of these standard streams for a child process.

This is the core mechanism behind:

- `<`
- `>`
- `>>`
- `|`

## 12. Execution Flow

A typical command follows:

```text
Read line
   |
   v
Parse tokens
   |
   v
Identify built-in / external
   |
   +---- built-in ----> Execute in parent
   |
   +---- external ---> fork()
                          |
                          v
                       Child
                          |
                    Configure pipes
                          |
                    Configure redirection
                          |
                       execvp()
                          |
                          v
                     Application
                          |
                          v
                       exit()
                          |
                          v
Parent <------------- waitpid()
```

## 13. Design Scope

The implementation focuses on the core mechanisms behind a Unix shell rather than attempting to reproduce a complete POSIX shell.

Implemented:

- Interactive command execution
- Built-ins
- `fork()`
- `execvp()`
- `waitpid()`
- `pipe()`
- `dup2()`
- Input redirection
- Output redirection
- Append redirection
- Multi-stage pipelines

Not implemented:

- Full shell quoting/escaping
- Environment-variable expansion
- Glob expansion
- Command substitution
- Job control
- Shell scripting
- `&&` / `||`
- Shell functions
