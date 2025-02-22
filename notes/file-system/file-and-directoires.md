process -> a virtualization of the CPU
address space -> a virtualization of memory

- these two abstractions allow a program to run as if it has its own CPU and memory

persistance storage (hard disk drive, SSD) -> stores infomation permanently 
- keeps data even when the computer is turned off, unlike memory (RAM), whose contents are lost when the power is turned off
-

Unix File System (a detailed blueprint for how all information is stored and accessed)
- everything is treated as a file (not just documents, but also directories, devices, programs, and even processes)
- responsible to store such data persistantly on disk and make sure that when you request the data again, you get what you put there in the first place
- There are many different type of file systems (e.g., ext4, NTFS, HFS+, APFS, etc.)
- file systems define how data is stored and accessed
- different file systems have different rules & features (speed, size limits, etc.)
- choosing the right one depends on your device & needs (linux, macOS, windows, etc.)

Two key abstractions in the virutalization of storage:
1. file -> a linear array of bytes, each of which you can read or write 
  - each file has an inode number(low-level name) assoicated with it)
2. directory -> it contains a list of pairs (user-readable name, low-level name)
  - ex: file with the low-level name 1234 and the user-readable name "myfile.txt" would be represented as (myfile.txt, 1234) inside the directory
  - the user-readable name is the name you see when you list the contents of a directory
  - the low-level name is the inode number of the file or directory
  - directories are used to organize files into a hierarchy, which makes it easier to find and access files
  - directories can also contain other directories, which allows for a tree-like structure of files and directories

Directroy Hierarchy:
- the root directory (/) is the top-level directory in the hierarchy

- file descriptor is a pointer to an object of type file

lseek() -> simply changes a variable in OS memory that keeps track of the current position in the file (the file offset)

**UNIX File System & `file` Struct Notes**

### **1. `file` Struct Overview**
- Represents an open file in UNIX, stored in a process’s **file descriptor table**.
- Tracks read/write access, position, and links to the file's inode.

### **2. `file` Struct Breakdown**
```c
struct file {
    int ref;           // Reference count
    char readable;     // Read permission
    char writable;     // Write permission
    struct inode *ip;  // File metadata
    uint off;          // File position
};
```
- `ref`: Number of processes using the file.
- `readable/writable`: File permissions.
- `ip`: Points to the file’s inode.
- `off`: Current position in the file.

### **3. Usage in Processes**
- Each process has an **file descriptor table**:
```c
struct process {
    int pid;                      // Process ID
    struct file *open_files[256];  // Open files
};
```
- Example:
    - Process opens `file.txt`, OS creates a `file` struct.
    - OS assigns it to `open_files[3]`, meaning **FD 3 represents this file**.

### **4. File System Interaction**
- `open()`: OS finds inode, creates `file` struct, and assigns FD.
- `read()/write()`: OS checks permissions and updates `off`.
- `close(fd)`: Decreases `ref`; if `ref == 0`, file is closed.

### **5. Summary**
- The `file` struct manages open files.
- It links to inodes for file metadata.
- OS maps FDs to `file` structs for tracking operations.

**Open File Table in UNIX**

### **What is the Open File Table?**
- A system-wide table that tracks all currently opened files.
- Contains `file` structs, each representing an open file.
- Multiple processes can reference the same entry if they open the same file.

### **Structure of the Open File Table**
Each entry in the table includes:
- A reference count (`ref`) tracking how many file descriptors point to it.
- Read (`readable`) and write (`writable`) permissions.
- A pointer to the file’s `inode`, which stores metadata.
- A file offset (`off`), tracking the current position in the file.

### **How It Works**
1. A process calls `open()`, creating a new `file` struct if the file is not already opened.
2. The `file` struct is added to the open file table.
3. The process’s **file descriptor table** gets a pointer to the `file` struct.
4. If another process opens the same file separately, a new `file` struct is created but shares the same `inode`.
5. If a process duplicates a file descriptor (via `dup()` or `fork()`), both FDs share the same `file` struct.

### **Key Takeaways**
- The open file table **manages all open files in the system**.
- `file` structs can be **shared** across processes but have separate offsets unless explicitly duplicated.
- The **inode remains the same** for the same file but is referenced by multiple `file` structs.

**System Calls**

- stat() or fstat() -> used to get information about a file or directory
- these calls take a pathname (or file descriptor) and fill in a `stat` structure containing information about the file or directory

### **`stat` Struct Overview**
```c
struct stat {
    uint dev;         // Device ID
    uint ino;         // Inode number
    short type;       // File type
    short nlink;      // Number of links
    uint size;        // Size in bytes
    uint atime;       // Last access time
    uint mtime;       // Last modification time
    uint ctime;       // Last status change time
};
```
- each file system usually keeps this type of information in a structure called `inode`

- `mv [command]` -> calls `rename()` system call under the hood and implemented as an atomic call.
- `rm [command]` -> calls `unlink()` system call under the hood and implemented as an atomic call.
- `mkdir [command]` -> calls `mkdir()` system call under the hood.
- an empty directory has two entries: one entry that referes to itself and one entry that refers to is parent

### **`dirent` Struct Overview**
- shows information about a directory entry
- used by the `readdir()` system call to read directory entries
- `readdir()` returns a pointer to a `dirent` structure, which contains information about the directory entry

```c
struct dirent {
    char d_name[DIRSIZ]; // Filename
    uint d_ino;         // Inode number
    off_t d_off;        // Offset to the next dirent
    unsigned short d_reclen;     // Length of this record
    unsigned char d_type;        // Type of file
};
```

Deleting Directories:
- `rmdir [command]` -> delete a directory
- if you try to delete a non-empty directory, it will fail

### **link() and unlink() System Calls**
- `link()` -> creates a new link (hard link) to an existing file
    - new way to make an entry in the file system tree
    - takes two arguments: the name of the existing file and the name of the new link
    - essentially create another way to refer to the same file (creates another name in the directory you are creating the link to, and refers to the same inode of the original file)
- `unlink()` -> removes a link to a file

- when removing a file using `unlink()` by calling `rm [command]`, the file is not actually deleted until all links to it are removed
- it first checks a reference count within the inode number, this allows the file system to track how many different file names have been linked to this particular inode.

### Permission Bits and Access Control Lists

```bash
ls -l foo.txt
-rw-r--r-- 1 user group 1234 Jan 01 12:00 foo.txt

```
- `-rw-r--r--` -> permission bits
    - first character: file type (d for directory, - for regular file)
    - next three characters: owner permissions (r for read, w for write, x for execute)
    - next three characters: group permissions
    - last three characters: other permissions

- `chmod [mode] [file]` -> change the permissions of a file
    - `u` -> user (owner)
    - `g` -> group
    - `o` -> other
    - `a` -> all (user, group, and other)
    - `r` -> read
    - `w` -> write
    - `x` -> execute

### Making and Mounting A File System

- `mkfs` -> make a file system
    - creates a new file system on a device (e.g., hard drive, USB drive)
    - formats the device with a specific file system type (e.g., ext4, NTFS)
    - initializes the file system structures (superblock, inode table, etc.)

- creating and mounting a file system involves two main steps:
1. **Creating the File System**:
    - Use `mkfs` to create a file system on a device.
    - Example: `mkfs.ext4 /dev/sdb1` creates an ext4 file system on the device `/dev/sdb1`.
2. **Mounting the File System**:
    - Use `mount` to attach the file system to a directory in the existing file system hierarchy.
    - Example: `mount /dev/sdb1 /mnt/mydrive` mounts the file system on `/dev/sdb1` to the directory `/mnt/mydrive`.

- Making a file system (mkfs) initializes storage for use.
- Mounting a file system (mount) makes it accessible from a directory.
- Persistent mounting (/etc/fstab) automates mounting at boot.
