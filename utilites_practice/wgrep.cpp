#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

/**
 * Reads from standard input.
 * Reads a single line from a file descriptor until newline or EOF.
 * Reads one character at a time.
 */
std::string readLineFromStdIn(int fd)
{
    char c;
    ssize_t bytesRead;
    std::string fullLine;

    // read one byte at a time until newline
    while ((bytesRead = read(fd, &c, 1)) > 0) {
        fullLine += c;
        if (c == '\n') {
            return fullLine;
        }
    }

    if (bytesRead == -1) {
        std::cerr << "Error reading file descriptor" << std::endl;
        return "";
    }

    return fullLine;
}

/**
 * Reads from file.
 * Reads a single line from a file descriptor until newline or EOF.
 * Uses buffered I/O for efficiency and repositions file offset.
 */
std::string readLineFromFile(int fd)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    std::string fullLine;

    // request the OS to read from a file
    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
        for (ssize_t i = 0; i < bytesRead; i++) {
            // checks if buffer includes new line character
            if (buffer[i] != '\n') {
                continue;
            }
            fullLine.append(buffer, i + 1);

            // repositions the offset of the file descriptor (each read() advances the pointer by number of bytes read)
            lseek(fd, -(bytesRead - i - 1), SEEK_CUR);
            return fullLine;
        }

        // append the whole buffer into the return string (have not reached new line character)
        fullLine.append(buffer, bytesRead);
    }

    if (bytesRead == -1) {
        std::cerr << "Error reading file descriptor" << std::endl;
        return "";
    }

    return fullLine;
}

/* Decides which function to use depending on file descriptor */
std::string getNextLine(int fd)
{
    return (fd == STDIN_FILENO) ? readLineFromStdIn(fd) : readLineFromFile(fd);
}

/**
 * Search through each line and find searchKeyWord as a substring.
 * Then, print out each line that matches to standard output.
 */
void searchAndPrintLines(int fd, const std::string& searchKeyWord)
{
    std::stringstream stringStream;
    std::string fullLine;

    // Parses file (or input) into each line and checks if line includes searchKeyWord as substring
    while ((fullLine = getNextLine(fd)) != "") {
        if (fullLine.find(searchKeyWord) != std::string::npos) {
            stringStream << fullLine;
        }
    }

    // C++ -> C string
    std::string str = stringStream.str();
    size_t bytes_remaining = str.length();
    ssize_t bytesWritten;
    const char* current_position = str.c_str();

    // while loop incase write fails (OS might not write all the bytes)
    while (bytes_remaining > 0) {
        bytesWritten = write(STDOUT_FILENO, current_position, bytes_remaining);

        // re-write if failed on initial attempt
        if (bytesWritten == -1) {
            continue;
        }
        bytes_remaining -= bytesWritten;
        current_position += bytesWritten;
    }
}

int main(int argc, char* argv[])
{
    // Need at least search term
    if (argc < 2) {
        std::cout << "wgrep: searchterm [file ...]" << std::endl;
        return 1;
    }

    const std::string& searchterm = argv[1];

    // No file specified - use standard input
    if (argc == 2) {
        searchAndPrintLines(STDIN_FILENO, searchterm);
        return 0;
    }

    // Process each file specified in arguments
    for (int i = 2; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1) {
            std::cout << "wgrep: cannot open file" << std::endl;
            return 1;
        }

        searchAndPrintLines(fd, searchterm);
        close(fd);
    }

    return 0;
}