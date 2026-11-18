# LSH — Lightweight Unix Shell in C

![Language](https://img.shields.io/badge/Language-C99%2FPOSIX-00599C?style=flat-square&logo=c&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20macOS%20%7C%20WSL-FCC624?style=flat-square&logo=linux&logoColor=black)
![Build](https://img.shields.io/badge/Compiler-GCC%20%7C%20Clang-success?style=flat-square)

**LSH (Lightweight Shell)** is a minimalist, POSIX-compliant command-line interpreter written in C. Built to demonstrate core operating system fundamentals, it implements a complete **Read–Eval–Print Loop (REPL)**, dynamic memory management, lexical tokenization, built-in command routing via function pointers, and Unix process lifecycle management using `fork()`, `execvp()`, and `waitpid()`.

## Table of Contents

1. [Architecture & Shell Lifecycle](#architecture--shell-lifecycle)
2. [Core OS Concepts Under the Hood](#core-os-concepts-under-the-hood)
3. [Codebase & Function Reference](#codebase--function-reference)
4. [Getting Started](#getting-started)
5. [Usage & Interactive Session](#usage--interactive-session)
6. [Current Limitations & Future Roadmap](#current-limitations--future-roadmap)
7. [Acknowledgments](#acknowledgments)

## Architecture & Shell Lifecycle

At its highest level, LSH operates across three distinct lifecycle phases:

1. **Initialize**: Sets up the execution environment and enters the interactive command loop.
2. **Interpret (The REPL Cycle)**: Continuously reads user input from `stdin`, parses the raw string into a null-terminated vector of tokens, and executes either an internal built-in command or an external system binary.
3. **Terminate**: Cleans up dynamically allocated heap memory on each iteration and exits cleanly upon receiving an `exit` command or an `EOF` (`Ctrl+D`) signal.

```text
+-----------------------------------------------------------------------+
|                              main()                                   |
+-----------------------------------------------------------------------+
                                  |
                                  v
+-----------------------------------------------------------------------+
|                            lsh_loop()                                 |
|   +------------------+   +-------------------+   +----------------+   |
|   |  lsh_readLine()  |-->| lsh_split_line()  |-->| lsh_execute()  |   |
|   | (getline / heap) |   | (strtok / realloc)|   |  (Dispatcher)  |   |
|   +------------------+   +-------------------+   +----------------+   |
+-----------------------------------------------------------------------+
                                                           |
                              +----------------------------+----------------------------+
                              | (Matches Built-in)                                      | (External Program)
                              v                                                         v
              +-------------------------------+                         +-------------------------------+
              |      Built-in Dispatch        |                         |         lsh_launch()          |
              |  (*builtin_func[i])(args)     |                         |                               |
              |  - lsh_cd()   -> chdir()      |                         |  1. fork()                    |
              |  - lsh_help() -> printf()     |                         |  2. Child:  execvp(args[0])   |
              |  - lsh_exit() -> returns 0    |                         |  3. Parent: waitpid(pid)      |
              +-------------------------------+                         +-------------------------------+
```

## Core OS Concepts Under the Hood

### 1. Process Creation (`fork`, `execvp`, and `waitpid`)
On Unix-like operating systems, new processes are not created from scratch—they are cloned from existing ones. Whenever you run an external command (like `ls`, `grep`, or `gcc`), LSH orchestrates three system calls inside `lsh_launch()`:

* **`fork()`**: Duplicates the running shell process. After `fork()` returns, two nearly identical processes run concurrently. `fork()` returns `0` to the newly created **child process** and returns the child's Process ID (`PID`) to the **parent process** (the shell).
* **`execvp()`**: Executed inside the child process (`pid == 0`). It replaces the child's entire memory image with the new program specified in `args[0]`. The **`v`** indicates it takes a *vector* (null-terminated array of `char *` strings), and the **`p`** instructs the OS kernel to automatically search the system `PATH` environment variable for the executable (so you can type `ls` instead of `/bin/ls`).
* **`waitpid()`**: Executed inside the parent shell process (`pid > 0`). While the child runs the external command, the parent blocks and waits for the child to finish. Because a process can change state without terminating (e.g., being stopped by a signal), LSH loops over `waitpid(pid, &status, WUNTRACED)` until `WIFEXITED(status)` (normal exit) or `WIFSIGNALED(status)` (killed by signal) evaluates to true.

### 2. Why Shell Built-ins Must Exist
Not every command can be executed in a forked child process. Commands that modify the state of the shell itself must be compiled directly into the shell executable:

* **Directory Navigation (`cd`)**: The current working directory is an attribute stored in a process's kernel control block. If `cd` were an external binary launched via `fork()`, the child process would call `chdir()`, change its *own* directory, and immediately exit—leaving the parent shell inside the original directory. By running `lsh_cd()` directly in the parent process, the shell updates its own working directory, which all future child processes subsequently inherit.
* **Shell Termination (`exit`)**: An external `exit` binary could only terminate its own child process. `lsh_exit()` runs in the parent process and returns `0` to `lsh_loop()`, breaking the `do-while` loop and terminating the shell gracefully.

### 3. Function Pointer Dispatch Table
Instead of hardcoding a sprawling `if-else` or `switch` block to check for built-in commands, LSH pairs an array of command strings with an array of function pointers:

```c
char *builtin_str[] = { "cd", "help", "exit" };

int (*builtin_func[])(char **) = {
    &lsh_cd, &lsh_help, &lsh_exit
};
```

Each function pointer in `builtin_func` accepts a `char **` argument vector and returns an `int` status code (`1` to continue the shell loop, `0` to terminate). Adding a new built-in command requires only defining the function and appending its name and pointer to these two arrays.

### 4. Dynamic Buffer Allocation
In C, reading arbitrary-length user input safely requires dynamic heap allocation to avoid buffer overflows:
* **Line Reading (`lsh_readLine`)**: Uses the POSIX `getline()` function, which automatically allocates and resizes the character buffer on the heap as characters are read from `stdin`. (The source code also preserves a manual character-by-character `getchar()` + `realloc()` implementation in comments to illustrate the underlying mechanics).
* **Tokenization (`lsh_split_line`)**: Starts with an initial pointer array of size `LSH_TOK_BUFF` (`70` tokens). If a command contains more than 70 arguments, the function dynamically doubles the capacity (`bufferSize += bufferSize`) via `realloc()`, achieving amortized $O(1)$ insertion cost.

## Codebase & Function Reference

| Function / Macro | Return Type | Parameters | Responsibility |
| :--- | :--- | :--- | :--- |
| `main` | `int` | `int argc, char **argv` | Entry point; initiates `lsh_loop()` and returns `EXIT_SUCCESS`. |
| `lsh_loop` | `void` | `void` | Core REPL loop; coordinates reading, parsing, execution, and `free()`. |
| `lsh_readLine` | `char *` | `void` | Reads a line from `stdin` via `getline()` and handles `EOF`. |
| `lsh_split_line` | `char **` | `char *line` | Tokenizes input on whitespace (` \t\r\n\a`) into an `argv` vector. |
| `lsh_execute` | `int` | `char **args` | Routes parsed commands to built-in functions or `lsh_launch()`. |
| `lsh_launch` | `int` | `char **args` | Forks a child process, calls `execvp()`, and awaits completion. |
| `lsh_cd` | `int` | `char **args` | Built-in command; changes working directory via `chdir(args[1])`. |
| `lsh_help` | `int` | `char **args` | Built-in command; displays shell info and registered built-ins. |
| `lsh_exit` | `int` | `char **args` | Built-in command; returns `0` to break the REPL loop. |
| `lsh_num_builtins` | `int` | `void` | Computes the number of built-in commands dynamically via `sizeof`. |

### Detailed Function Breakdown

* **`char *lsh_readLine(void)`**
  Initializes `char *line = NULL` and `size_t bufferSize = 0`, delegating heap allocation to `getline(&line, &bufferSize, stdin)`. If `getline()` returns `-1`, it checks `feof(stdin)`: on `EOF` (`Ctrl+D`), the shell exits cleanly with `EXIT_SUCCESS`; on an I/O error, it logs the system error via `perror()` and exits with `EXIT_FAILURE`.

* **`char **lsh_split_line(char *line)`**
  Allocates an array of `char *` pointers of size `LSH_TOK_BUFF` (`70`). It invokes `strtok(line, LSH_TOK_DELIM)` to split the string in-place by replacing whitespace delimiters (` `, `\t`, `\r`, `\n`, `\a`) with null bytes (`\0`). Each token pointer is stored in `tokens[position]`. If the token count reaches `bufferSize`, the capacity is doubled (`bufferSize += bufferSize`) and reallocated via `realloc()`. The final element is set to `NULL` as required by `execvp()`.

* **`int lsh_execute(char **args)`**
  Guards against empty input (`args[0] == NULL`, such as when a user hits `Enter` on a blank line) by immediately returning `1`. Otherwise, it iterates through `builtin_str[]` using `strcmp()`. If a match is found, it dereferences the corresponding function pointer `(*builtin_func[i])(args)`. If no built-in matches, it delegates to `lsh_launch(args)`.

* **`int lsh_launch(char **args)`**
  Calls `fork()` and branches into three execution paths:
  1. `pid == 0` (Child): Calls `execvp(args[0], args)`. If `execvp` returns `-1` (e.g., command not found), it prints an error with `perror("lsh")` and terminates the child process with `EXIT_FAILURE` so the failed child does not continue running the parent shell's loop.
  2. `pid < 0` (Fork Error): Logs the failure via `perror("lsh: Couldnt Fork")` and returns `1` so the shell remains usable.
  3. `pid > 0` (Parent): Calls `waitpid(pid, &status, WUNTRACED)` in a `do-while` loop until the child either exits normally (`WIFEXITED`) or is terminated by a signal (`WIFSIGNALED`), then returns `1`.

## Getting Started

### Prerequisites
* A POSIX-compliant operating system (Linux, macOS, FreeBSD, or Windows Subsystem for Linux).
* A C compiler such as `gcc` or `clang`.

### Compilation & Execution

1. **Clone the repository** (or save the source file as `main.c`):
   ```bash
   git clone [https://github.com/your-username/lsh-shell.git](https://github.com/your-username/lsh-shell.git)
   cd lsh-shell
   ```

2. **Compile the source code**:
   ```bash
   gcc -Wall -Wextra -pedantic -std=c99 -D_POSIX_C_SOURCE=200809L main.c -o lsh
   ```
   *(Note: `-D_POSIX_C_SOURCE=200809L` exposes the POSIX `getline()` declaration when compiling with strict `-std=c99` flags).*

3. **Run the shell**:
   ```bash
   ./lsh
   ```

## Usage & Interactive Session

Once inside the `> ` prompt, you can run both built-in commands and standard Unix utilities available in your `PATH`:

```text
$ ./lsh
> help
Rahul Singhs LSH
Type program names and arguments and hit enter.
The following are builtin commands:
 cd
 help
 exit
Use the main command for informantin of other programs.
> pwd
/home/rahul/projects/lsh-shell
> cd ..
> pwd
/home/rahul/projects
> ls -la
total 16
drwxr-xr-x 4 rahul rahul 4096 Nov 18 19:20 .
drwxr-xr-x 8 rahul rahul 4096 Nov 18 18:00 ..
drwxr-xr-x 2 rahul rahul 4096 Nov 18 19:25 lsh-shell
> exit
$
```

## Current Limitations & Future Roadmap

Because LSH is designed for clarity and educational simplicity, several advanced shell features are intentionally omitted and serve as natural next steps for expansion:

* **Quoting & Backslash Escaping**: Arguments are split strictly on whitespace via `strtok()`. Running `echo "hello world"` passes `"hello` and `world"` as two separate tokens rather than a single string.
* **Pipes & I/O Redirection**: Operators such as `|`, `>`, `>>`, and `<` (implemented via `pipe()` and `dup2()`) are not currently parsed.
* **Background Execution (`&`)**: The parent process always blocks on `waitpid()`. Supporting `&` at the end of a command would allow asynchronous background execution.
* **Signal Handling**: Pressing `Ctrl+C` (`SIGINT`) currently terminates the `lsh` process itself rather than only interrupting the running foreground child process (can be resolved using `sigaction()` or `signal(SIGINT, SIG_IGN)` in the parent).
* **Command History & Line Editing**: Integrating `GNU Readline` or raw terminal mode (`termios.h`) to support arrow-key navigation and tab auto-completion.

## Acknowledgments

* **Author**: Rahul Singh
* **Reference & Inspiration**: Based on Stephen Brennan's classic systems programming guide, [*Tutorial - Write a Shell in C*](https://brennan.io/2015/01/16/write-a-shell-in-c/).