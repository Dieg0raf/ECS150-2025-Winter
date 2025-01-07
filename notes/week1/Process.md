## Chapter 4 - The Abstraction: The Process

**Abstraction**
    - Abstraction is all about hiding complexity and letting you focus on what you need to do, rather than how it’s done. It makes your code simpler, easier to read, and more maintainable!

**Process**: The most fundamental abstraction that the OS provides to users.
    - Def: quite simple, it's a running "program".
    - program is lifeless, sits on the disk, waiting to spring into action.
    - OS takes these bytes and gets them running, transforming the "program" into something useful.

**Virtualization**
    - The OS creates the illusion that many processes are running at the same time by **virtualizing** the CPU.
    - Def: Virtualization is the creation of virtual versions of physical resources, like computers or servers, allowing multiple independent systems to run on a single physical machine efficiently.
    - Ex:  Virtualization is like pretending you have multiple computers inside one real computer. Imagine you have a powerful computer, and you use special software to divide it into smaller "virtual" computers. Each virtual computer can run its own programs and even a different operating system (like Windows, Linux, or macOS).
    - To implement virtualization of the CPU, and to implement it well, the OS will need both some low-level machinery and some high-level intelligence. We call the low-level machinery mechanisms; mechanisms are low-level methods or protocols that implement a needed piece of functionality.

**Time Sharing**
    - Def: Time sharing is a basic technique used by an OS to share a resource. By allowing the resource to be used for a little while by one entity, and then a little while by another, and so forth, the resource in question (e.g., the CPU, or a network link) can be shared by many.
    - of the CPU, allows users to run as many concurrent processes as they would like (cost performance, each process will run slowly if the CPU is shared).

**Modularity**
    - modularity means breaking a program into smaller, self-contained parts (called modules), so each part can be developed, fixed, or reused independently.

**Polices**
    - Def: algorithms for making some kind of decision within the OS.

**What constitutes a process?**
    - we have to understand it's machine state: what a program can read or update when it's running. At any given time, what parts of the machine are important to the execution of this program?
    - Memory that the process can address (called its address space) is part of the process.
    - registers; many instructions read or update registers and thus they are important to the execution of the process.

**How does the OS get a program up and running**
    - ![Loading: From Program To Process](https://github.com/user-attachments/assets/13b97174-0c14-48c4-9ab8-4d2b7bf2119b).
    - load its code and any static data into memory into the address space of the process.
    - the process of loading a program and static data into memory requires the OS to read those bytes from disk and place them into memory somewhere.
    - OS must do some work to get the important program bits from disk into memory.
    - OS needs to allocate some memory for the programs stack (used for local variables, function parameters, and return addresses).
    - OS also needs to allocate memory for the programs heap (used for dynamically-allocated data -- e.g. data structure: linked list, trees, etc...).
    - OS initializes tasks, particularly as related to input/output(I/O) set.

**States a Process can be in at any given time**
    - **Running**: running on a processor. Means it is executing instructions.
    - **Ready**: Ready to run, but OS has chosen not to run at this given moment.
    - **Blocked**: performed some kind of operation that makes it not read to run until some other even takes place.
        - A process can be blocked during I/O (Input/Output) operations because of how operating systems manage resource access and scheduling. When a process performs I/O, such as reading from a file, requesting data over a network, or writing to a disk, it has to wait for the I/O operation to complete before it can continue its execution. This waiting period is what causes the process to be blocked.
    - Being moved from ready to running means the process has been **scheduled**.
    - Being moved from running to ready means the process has been **rescheduled**.
    - Process moves from Blocked to running.

**OS Scheduler**
    - The OS scheduler is a part of the operating system that manages the execution of processes.
    - It decides which process (task) should run, when, and for how long. 
    - The goal is to ensure that the CPU (central processing unit) is used efficiently by switching between processes, giving each one a fair amount of time to execute.
    - In short, OS scheduler keeps things running smoothly by quickly switching between tasks so that everything works as efficiently as possible!

- APIs available in a modern OS:
  - Create, Destroy, Wait, Miscellaneous Control, Status
- Input/Output -> I/O