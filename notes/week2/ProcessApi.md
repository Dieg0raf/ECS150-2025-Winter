# Chapter 5 - Process API

## Key Concepts

### fork ()

- sys call used to create a new process
- creates copy of the calling process
- new process starts after the fork() call
- The child process starts running as an almost identical copy of the parent, inheriting its memory, variables, and state at the moment fork() is called.
- After fork(), the parent and child processes have separate memory spaces.
- The 0 return value from fork() in the child process serves as a way to distinguish between the parent and child processes after the fork() call.
- outputs of programs in each process are **non-deterministic** because child can run before parent and vise versa (depend on the CPU Scheduler)

### wait ()

- sys call used for a parent process to wait for a child process to finish what it has been doing
- makes output deterministic (returns after child exits - few cases where it returns before the child exits. **Read man pages**)

### exec()

- sys call to run a program that is different from the calling program
  - **Program** in this context means: any executable file on the system:
    - A compiled binary (e.g., ls, vim , gcc)
    - A script (e.g., Python, shell, or Perl scripts)
    - GUI applications (e.g., Chrome, a text editor, etc.).
- allows a child to break free from its similarity to its parent and execute an entirely new program
- does **NOT** create a new process; rather, it transforms the currently running program into a different running program
- the heap and stack and other parts of the memory space of the program are re-initialized

### Why is this useful for a shell?

A shell (like bash or zsh) is the program you use to run commands. When you type a command like ls, the shell:

1. Uses fork() to create a copy of itself.

2. In that moment after fork() but before exec(), the shell can make changes. For example:
   - It might set up redirection (e.g., making the command’s output go to a file instead of the screen).
   - It can adjust the environment (e.g., setting specific variables for the program to use).
   - Finally, it calls exec() to replace the copy with the program you want to run (like ls).

In short: The fork() and exec() combo gives the shell a chance to prepare things (like redirection or environment changes) before actually running a new program.

### How a Shell Works

1. **What is the Shell?**

   - The shell is a user program that shows a prompt and waits for you to type a command.

2. **What Happens When You Enter a Command?**

   - You type a command (name of an executable program + arguments).
   - The shell figures out where the executable is located in the file system.

3. **Steps the Shell Takes:**

   1. **`fork()`**: Creates a new child process to run the command.
   2. **`exec()`**: Replaces the child process with the command to execute it.
   3. **`wait()`**: Waits for the child process to complete.

4. **After the Command Completes:**
   - The shell returns from `wait()`.
   - It prints a new prompt, ready for the next command.

### Process Identifier (PID)

- Name of each process; in most systems
- used to name the process if one wants to do something with that process

### CPU Scheduler

- determines which process runs at a given moment in time

### kill()

- system call is used to send signals to a process, including directives to pause, die, and other useful imperatives.

### Other notes

- process control is available in the form of **signals**, which can cause jobs to stop, continue, or even terminate
