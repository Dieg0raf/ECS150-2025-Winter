#include <fcntl.h> // For file opening flags like O_RDONLY
#include <iostream> // For standard input/output
#include <pthread.h> // For using POSIX threads (pthreads)
#include <stdlib.h> // For general functions like memory allocation
#include <sys/select.h> // For monitoring multiple file descriptors
#include <sys/types.h> // Defines system data types
#include <sys/uio.h> // For low-level I/O operations
#include <unistd.h> // For system calls like read, write, and close
#include <vector> // For using vector data structure

using namespace std;

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER; // Initialize mutex for thread synchronization

// Function to read from a file descriptor (fd) and write the contents to standard output
void write_file_to_stdout(int fd)
{
    char buffer[4096]; // Buffer to hold file content
    int ret; // Variable to store read return value

    // Read from file descriptor in chunks and write to stdout
    while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
        p_thread_mutex_lock(&lock); // Lock mutex to ensure thread-safe output
        write(STDOUT_FILENO, buffer, ret);
        p_thread_mutex_unlock(&lock); // Unlock mutex after writing
    }

    std::cout << "Executing fd: " << fd << std::endl;
    close(fd); // Close the file descriptor when done
}

// Struct to hold arguments for the thread function
struct ThreadArgs {
    int fd; // File descriptor to be processed by the thread
};

// Function executed by each thread
void* fileThread(void* arg)
{
    struct ThreadArgs* threadArgs = (struct ThreadArgs*)arg; // Cast void* to ThreadArgs*
    int fd = threadArgs->fd; // Extract the file descriptor
    write_file_to_stdout(fd); // Read file contents and print to stdout
    delete threadArgs; // Free dynamically allocated memory for arguments
    return NULL; // Return NULL since pthread requires a void* return type
}

int main(int argc, char* argv[])
{
    vector<int> fdVec; // Vector to store file descriptors
    fdVec.push_back(STDIN_FILENO); // Add standard input as the first source

    // Open each file provided as command-line arguments and store file descriptors
    for (int idx = 1; idx < argc; idx++) {
        int fd = open(argv[idx], O_RDONLY); // Open file in read-only mode
        fdVec.push_back(fd); // Store the file descriptor in vector
    }

    vector<pthread_t> threads; // Vector to store thread IDs

    // Create a separate thread for each file descriptor
    for (int idx = 0; idx < fdVec.size(); idx++) {
        int fd = fdVec[idx];
        struct ThreadArgs* threadArgs = new struct ThreadArgs; // Allocate memory for arguments
        threadArgs->fd = fd; // Set file descriptor
        pthread_t threadId; // Variable to hold thread ID
        pthread_create(&threadId, NULL, fileThread, threadArgs); // Create new thread
        threads.push_back(threadId); // Store thread ID
    }

    // Wait for all threads to finish before proceeding
    for (int idx = 0; idx < threads.size(); idx++) {
        pthread_join(threads[idx], NULL); // Wait for each thread to complete
    }

    cout << "All done!" << endl; // Indicate completion
}
