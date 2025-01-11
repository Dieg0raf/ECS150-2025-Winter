#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
// #include <string.h>
// #include <string>
#include <sys/types.h>
#include <sys/uio.h>
#include <typeinfo>
#include <unistd.h>

#define BUFFER_SIZE 1024

void reach_new_line(int fd, char* searchWord)
{
    // create buffer and start reading bytes from file (fill buffer)
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead = read(fd, buffer, BUFFER_SIZE);

    // points to the start of the buffer
    const char* start = buffer;
    const char* newline = strchr(start, '\n');

    // meaning no more new lines
    while (newline != NULL) {
        // write(fd, newline - start, bytesRead);
        // // fwrite(start, 1, newline - start, stdout);
        // printf("\n");
        for (int i = 0; i < bytesRead; i++) {
            write(STDOUT_FILENO, start, 1);
        }

        // Move to the next line
        start = newline + 1;
        newline = strchr(start, '\n');
    }

    // for (int i = 0; i < bytesRead; i++) {
    //     if (buffer[i] == '\n') {

    //         // // Check if line has keyword
    //         // if (strstr(buffer, searchWord) != NULL) {
    //         //     write(STDOUT_FILENO, buffer, bytesRead);
    //         // }

    //         // bytesRead = read(fd, buffer, BUFFER_SIZE);
    //     }
    // }
}

int main(int argc, char* argv[])
{
    // no files are specified on the command line
    if (argc < 2) {
        std::cout << "wgrep: searchterm [file ...]" << std::endl;
        return 1;
    }

    // checks if search string is empty string
    if (std::string(argv[1]).empty()) {
        return 0;
    }

    // iterate through command line arguments starting at 1 (after the program name)
    for (int i = 2; i < argc; i++) {

        std::cout << "Opening: " << argv[i] << std::endl;
        // open file (get file descriptor)
        int fd = open(argv[i], O_RDONLY);

        if (fd < 0) {
            std::cout << "wgrep: cannot open file" << std::endl;
            return 1;
        }

        reach_new_line(fd, argv[1]);

        // close file
        close(fd);
    }

    return 0;
}