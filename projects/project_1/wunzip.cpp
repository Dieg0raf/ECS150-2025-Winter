#include <cctype>
#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

struct Unzip_State {
    char partial_int[sizeof(uint32_t)];
    int partial_bytes_read;
    bool need_char;
    uint32_t current_integer;
};

/*
 * Process buffer of bytes.
 * Handles processing file from different buffer.
 * Returns 0 if successful, -1 if error.
 */
int process_buffer(char* buffer, ssize_t bytesRead, Unzip_State& state)
{
    ssize_t i = 0;

    // Iterate through buffer
    while (i < bytesRead) {
        // Checks if there is a partial integer (part of it was in the last buffer)
        if (state.partial_bytes_read > 0) {
            int neededBytes = sizeof(uint32_t) - state.partial_bytes_read;
            int availableBytes = std::min(neededBytes, (int)(bytesRead - i)); // the amount of bytes that are going to be copied into the integer

            // copy 'availableBytes' amount of bytes into partial_int (temp bucket)
            memcpy(state.partial_int + state.partial_bytes_read, buffer + i, availableBytes);
            state.partial_bytes_read += availableBytes;
            i += availableBytes;

            // copy integer into current_integer variable
            if (state.partial_bytes_read == sizeof(uint32_t)) {
                memcpy(&state.current_integer, state.partial_int, sizeof(uint32_t));
                state.partial_bytes_read = 0;
                state.need_char = true;
            } else {
                return 0;
            }
        }

        // Check if character is needed
        if (state.need_char) {

            // Wait for next buffer (can't read in this buffer)
            if (i >= bytesRead) {
                return 0;
            }
            char c = buffer[i++];
            for (ssize_t i = 0; i < state.current_integer; i++) {
                write(STDOUT_FILENO, &c, 1);
            }
            state.need_char = false;
        }

        // Check if there is enough space to grab an integer
        if (i + sizeof(uint32_t) <= bytesRead) {
            // Get new integer from buffer
            memcpy(&state.current_integer, buffer + i, sizeof(uint32_t));
            i += sizeof(uint32_t);
            state.need_char = true;

        } else if (i < bytesRead) {
            // Save partial integer for next buffer (not enough bytes for the whole integer)
            int availableBytes = bytesRead - i;
            memcpy(state.partial_int, buffer + i, availableBytes);
            state.partial_bytes_read = availableBytes;
            i = bytesRead;
        }
    }

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "wunzip: file1 [file2 ...]" << std::endl;
        return 1;
    }

    Unzip_State state = { { 0 } };

    for (int i = 1; i < argc; i++) {
        int fd = open(argv[i], O_RDONLY);
        if (fd == -1) {
            std::cout << "wzip: cannot open file" << std::endl;
            return 1;
        }

        char buffer[BUFFER_SIZE];
        ssize_t bytesRead;

        // Process file
        while ((bytesRead = read(fd, buffer, BUFFER_SIZE)) > 0) {
            // Process bytes inside buffer
            if (process_buffer(buffer, bytesRead, state) != 0) {
                std::cout << "wzip: error processing file" << std::endl;
                return 1;
            }
        }

        if (bytesRead == -1) {
            std::cout << "wzip: error reading file" << std::endl;
            return 1;
        }

        close(fd);
    }

    return 0;
}
