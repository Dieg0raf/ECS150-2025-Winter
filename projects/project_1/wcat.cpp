#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

// argc: The number of command-line arguments (including the program name)
// argv: An array of C-style strings containing the arguments
int main(int argc, char* argv[])
{

    // no files are specified on the command line
    if (argc == 1) {
        std::cout << "wcat: no file was specifed" << std::endl;
        return 0;
    }

    // iterate through command line arguments starting at 1 (after the program name)
    for (int i = 1; i < argc; i++) {

        // open file (get file descriptor)
        int fd = open(argv[i], O_RDONLY);
        int w1;

        if (fd < 0) {
            std::cerr << "wcat: cannot open file" << std::endl;
            return 1;
        }

        // create buffer and start reading bytes from file (fill buffer)
        char buffer[BUFFER_SIZE];
        ssize_t bytesRead = read(fd, buffer, BUFFER_SIZE);

        // read all bytes of the file
        while (bytesRead > 0) {

            // terminal output (empty buffer)
            w1 = write(STDOUT_FILENO, buffer, bytesRead);
            if (w1 == -1) {
                std::cerr << "wcat: error writing to standard output" << std::endl;
            }
            bytesRead = read(fd, buffer, BUFFER_SIZE);
        }

        // close file
        close(fd);
    }

    return 0;
}
