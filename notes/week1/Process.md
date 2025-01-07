# Chapter 4 - The Abstraction: The Process

## Key Concepts

### Abstraction
- Hides complexity and lets you focus on what you need to do
- Makes code simpler, easier to read, and more maintainable

### Process
- Most fundamental abstraction the OS provides to users
- Definition: A running program
- Program is static (sits on disk) until OS transforms it into an active process

### Virtualization
- OS creates illusion of many processes running simultaneously
- Achieved through CPU virtualization
- Allows multiple processes to share CPU resources

### Time Sharing
- Basic technique used by OS to share resources
- Resources are used by one entity for a brief period before switching
- Enables multiple concurrent processes at cost of performance

### Modularity
- Breaking programs into smaller, self-contained parts (modules)
- Allows independent development, fixing, and reuse

### Policies
- Algorithms for making decisions within the OS

## Process Components

### Machine State
- Address space (memory the process can access)
- Registers (CPU storage locations used by process)
- Program counter and other special-purpose registers

### Process Creation
1. Load program code and static data into memory
2. Allocate memory for:
   - Program stack (local variables, parameters, return addresses)
   - Program heap (dynamic data structures)
3. Initialize I/O tasks (Input/Output)
4. ![Loading: From Program To Process](https://github.com/user-attachments/assets/13b97174-0c14-48c4-9ab8-4d2b7bf2119b).

### Process States
1. **Running**
   - Currently executing on processor
   - Actively using CPU resources

2. **Ready**
   - Able to run but waiting for CPU time
   - Scheduled/rescheduled between ready and running

3. **Blocked**
   - Waiting for external event (e.g., I/O completion)
   - Cannot proceed until event occurs

## OS Scheduler
- Manages process execution
- Determines:
  - Which process runs
  - When it runs
  - Duration of execution
- Ensures efficient CPU utilization
- Implements fair time-sharing between processes

## APIs in modern OS
- Create
- Destroy
- Wait
- Miscellaneous Control
- Status