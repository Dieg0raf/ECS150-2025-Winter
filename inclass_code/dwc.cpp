#include <iostream>

#include <fcntl.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <sstream>

using namespace std;

// system call is a way for the OS to invoke the process

int main(int argc, char* argv[])
{
    int fd;
    if (argc == 1) {
        // if no word, use standard input
        fd = STDIN_FILENO;
    }

    else {
        fd = open(argv[1], O_RDONLY);
        if (fd == -1) {
            cerr << "Error opening file" << argv[1] << endl;
            return 1;
        }

        int bytesRead = 0;
        char buffer[4096];
        int ret;

        // read() invokes the OS read system call to read data from the file descriptor.
        // Therefore, making buffer to low will make it read less data at a time (not efficient).
        while ((ret = read(fd, buffer, sizeof(buffer))) > 0) {
            bytesRead += ret;
        }

        if (ret == -1) {
            cerr << "Error reading file descriptor" << endl;
            return 1;
        }

        // Equivalent: cout << "Bytes read: " << bytesRead << endl;
        stringstream stringStream;
        stringStream << "Bytes read: " << bytesRead << endl;

        // convert to C++ string
        string str = stringStream.str();

        // Keep track of bytes that need to be written
        size_t bytes_remaining = str.length();

        // convert to C string (system calls only read in C)
        const char* current_position = str.c_str();

        // include while loop incase write fails (OS might not write all the bytes)
        while (bytes_remaining > 0) {
            ret = write(STDOUT_FILENO, current_position, bytes_remaining);
            if (ret == -1) {
                cerr << "Cout not write to stdout" << endl;
                return 1;
            }

            // decrease the amount of bytes remaining (write() returns the bytes which were written)
            bytes_remaining -= ret;

            // move point forward by 'ret' amount of bytes
            current_position += ret;
        }

        // Always check for errors if making system calls

        return 0;
    }
}
