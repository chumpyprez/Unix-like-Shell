# Unix-Like Shell in C

A lightweight Unix-like command shell implemented in **C** to explore Linux/Unix process management, system calls, command parsing, inter-process communication, and file-descriptor manipulation.

The shell supports common commands such as `cd`, `pwd`, and external programs such as `ls`, along with **piping and input/output redirection**.

## Features

- Interactive command prompt
- Built-in `cd`
- Built-in `pwd`
- Built-in `exit`
- External command execution using `fork()` and `execvp()`
- Process synchronization using `waitpid()`
- Single and multi-stage pipelines using `pipe()`
- Input redirection using `<`
- Output redirection using `>`
- Append redirection using `>>`
- File-descriptor duplication using `dup2()`
- Error handling for invalid commands and malformed redirection

## Architecture

The shell follows this general execution flow:

```text
User Input
    |
    v
+----------------+
| Command Parser |
+-------+--------+
        |
        v
+----------------------+
| Built-in Command?    |
+----------+-----------+
           |
       Yes |       No
           |        |
           v        v
     +---------+  +------------------+
     | cd/pwd  |  | fork()           |
     | /exit   |  | execvp()         |
     +---------+  +--------+---------+
                           |
              +------------+------------+
              |                         |
              v                         v
        Redirection                 Pipeline
         dup2()                     pipe()
              |                         |
              +------------+------------+
                           |
                           v
                     Child Processes
                           |
                           v
                        waitpid()
```

A more detailed architecture diagram is available in [`docs/unix-shell-project-overview.png`](docs/unix-shell-project-overview.png).

## Supported Commands

### Built-ins

```bash
cd <directory>
pwd
exit
```

### External commands

The shell uses `execvp()` so commands available through the system `PATH` can be executed.

Examples:

```bash
ls
ls -l
cat file.txt
echo Hello
grep main shell.c
```

## Piping

Commands can be connected using `|`.

Example:

```bash
ls | grep ".c"
```

The shell creates a pipe and connects:

```text
stdout of command 1
        |
        v
      pipe
        |
        v
stdin of command 2
```

Multiple pipeline stages are also supported:

```bash
cat file.txt | grep "main" | wc -l
```

## Input Redirection

```bash
sort < input.txt
```

The shell opens the file and redirects it to standard input using `dup2()`.

## Output Redirection

```bash
ls -l > listing.txt
```

The shell redirects standard output to the specified file.

## Append Redirection

```bash
echo "new line" >> output.txt
```

The shell opens the destination using append mode.

## Example Session

```text
unix-shell:/home/gagan$ pwd
/home/gagan

unix-shell:/home/gagan$ ls
README.md  shell.c  Makefile

unix-shell:/home/gagan$ ls | grep README
README.md

unix-shell:/home/gagan$ cat input.txt | grep error | wc -l
3

unix-shell:/home/gagan$ exit
```

## System Calls Used

| System call / API | Purpose |
|---|---|
| `fork()` | Creates child processes |
| `execvp()` | Executes external programs |
| `waitpid()` | Waits for child processes |
| `pipe()` | Creates IPC channels between processes |
| `dup2()` | Redirects standard input/output |
| `open()` | Opens files for redirection |
| `close()` | Releases file descriptors |
| `chdir()` | Implements `cd` |
| `getcwd()` | Implements `pwd` |

## Process Model

For a simple command:

```text
Shell Process
     |
     | fork()
     v
Child Process
     |
     | execvp()
     v
External Program
     |
     | exit
     v
Shell waits using waitpid()
```

For a pipeline:

```text
Shell
 |
 +-- fork --> Process 1 -- stdout --> pipe -- stdin --> Process 2
 |
 +-- fork --> Process 2
 |
 +-- waitpid()
```

## File Structure

```text
Unix-Like-Shell/
│
├── README.md
├── ARCHITECTURE.md
├── Makefile
├── LICENSE
│
├── src/
│   └── shell.c
│
├── docs/
│   └── unix-shell-project-overview.png
│
└── tests/
    └── test_commands.txt
```

## Build

Linux/macOS:

```bash
make
```

Or directly:

```bash
gcc -Wall -Wextra -std=c11 -pedantic -o unix_shell src/shell.c
```

## Run

```bash
./unix_shell
```

## Test Examples

After starting the shell:

```bash
pwd
ls
cd ..
pwd
ls | grep ".c"
cat input.txt | grep error
sort < input.txt
ls -l > listing.txt
echo "hello" >> listing.txt
```

## Limitations

This is an educational Unix-like shell rather than a complete POSIX shell.

It intentionally does not attempt to implement the complete shell language, including:

- Full quote/escape semantics
- Environment-variable expansion
- Command substitution
- Job-control commands
- Shell scripting
- Wildcard/glob expansion
- Logical operators such as `&&` and `||`
- Shell functions

The focus is on **process creation, command execution, pipes, file descriptors, and I/O redirection**.

## Learning Outcomes

This project demonstrates practical understanding of:

- C programming
- Linux/Unix system calls
- Process creation and management
- Inter-process communication
- File descriptors
- Standard input/output
- Command parsing
- `fork()` / `exec()` process model
- Unix pipes
- I/O redirection
- Process synchronization

## Author

**BM Gagan**

Electronics and Communication Engineering — PES University
