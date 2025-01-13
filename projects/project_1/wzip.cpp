#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

bool hasWriteError(ssize_t write1, ssize_t write2)
{
    return (write1 == -1 || write2 == -1) ? true : false;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "wzip: file1 [file2 ...]" << std::endl;
        return 1;
    }

    int current_count = 1;
    char current_char;
    bool first_char = true;
    ssize_t w1;
    ssize_t w2;

    // Read through the passed in arguments
    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);

        if (fd == -1) {
            std::cout << "wzip: cannot open file" << std::endl;
            return 1;
        }

        char buffer[BUFFER_SIZE];
        ssize_t bytesRead;

        while ((bytesRead = read(fd, buffer, BUFFER_SIZE)) > 0) {
            // Checking each character that was read into the buffer
            for (ssize_t k = 0; k < bytesRead; k++) {

                if (first_char) {
                    current_char = buffer[k];
                    first_char = false;
                    continue;
                }

                // Count occurrences of current character (if they don't match then write to standard output)
                if (buffer[k] == current_char) {
                    current_count++;
                } else {
                    w1 = write(STDOUT_FILENO, &current_count, sizeof(int));
                    w2 = write(STDOUT_FILENO, &current_char, 1);

                    if (hasWriteError(w1, w2)) {
                        std::cerr << "wzip: error writing to standard output" << std::endl;
                        return 1;
                    }

                    // update current variables
                    current_count = 1;
                    current_char = buffer[k];
                }
            }
        }

        if (bytesRead == -1) {
            std::cout << "wzip: Failed to read" << std::endl;
        }

        close(fd);
    }

    // Print the last count and character
    if (!first_char) {
        w1 = write(STDOUT_FILENO, &current_count, sizeof(int));
        w2 = write(STDOUT_FILENO, &current_char, 1);

        if (hasWriteError(w1, w2)) {
            std::cerr << "wzip: error writing to standard output" << std::endl;
            return 1;
        }
    }

    return 0;
}