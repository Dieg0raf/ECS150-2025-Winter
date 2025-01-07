# Chapter 4 - The Abstraction: The Process

## Key Concepts

### Abstraction
- Hides complexity and lets you focus on what you need to do
- Makes code simpler, easier to read, and more maintainable

### Process
- Most fundamental abstraction the OS provides to users
- Definition: A running program
- Program is static (sits on disk) until OS transforms it into an active process

### Low-level Mechanisms
- Needed to implement processes

### Higher-level Policies
- Required to schedule processes in an intelligent way

### Policies
- Algorithms for making decisions within the OS
  
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

## Process Components

### Machine State
- Address space (memory the process can access)
- Registers (CPU storage locations used by process)
- Program counter and other special-purpose registers

### Address Space
- Memory that the process can address
- Contains several key segments:
  - **Code (Text)**: Program instructions
  - **Stack**: Local variables, function parameters, return addresses
  - **Heap**: Dynamically allocated memory
  - **Data**: Global variables
    - Static data: Initialized at start
    - BSS (Block Started by Symbol): Uninitialized data

### Process Creation
1. Load program code and static data into memory
2. Allocate memory for:
   - Program stack (local variables, parameters, return addresses)
   - Program heap (dynamic data structures)
3. Initialize I/O tasks (Input/Output)
4. <img src="https://github.com/user-attachments/assets/13b97174-0c14-48c4-9ab8-4d2b7bf2119b" width="50%"/>

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

4. **Other states a process can have**
   - process can have other states, beyond running, ready, and blocked
   - Initial: being created
   - Final (Zombie for UNIX-based systems): exited, but not yet cleaned up
      - Allows the parent process (created the process) to examine the return code of the process
      - Unix-based systems return zero if task was completed, else a non-zero

### OS Scheduler
- Manages process execution
- Determines:
  - Which process runs
  - When it runs
  - Duration of execution
- Ensures efficient CPU utilization
- Implements fair time-sharing between processes

### APIs in modern OS (System Calls)
- Create
- Destroy
- Wait
- Miscellaneous Control
- Status

### Data Structures within an OS
1. **Process list (task list)** 
   - for all processes that are ready 
   - additional information to track which process is currently running

2. **Process Control Block (PCB)**
   - Individual structure that stores information about a process
     - C structure that contains information about each

3. **Register context**
   - will hold, for a stopped process, the contents of it's register