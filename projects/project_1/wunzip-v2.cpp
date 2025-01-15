#include <cctype>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cout << "wunzip: file1 [file2 ...]" << std::endl;
        return 1;
    }

    char current_int[sizeof(uint32_t)];
    char character;
    int bytes_read_into_int = 0;
    uint32_t integer = 0;
    bool num_from_prev = false;
    bool set_num_from_prev = false;

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
            // std::cout << "Bytes Read: " << bytesRead << std::endl;
            // std::cout << "New buffer" << std::endl;
            for (ssize_t k = 0; k < bytesRead; k++) {

                // Check if part of the integer is from previous buffer
                if (num_from_prev) {

                    // Create a new buffer to hold the integer
                    for (ssize_t p = 0; p < bytesRead; p++) {
                        if (bytes_read_into_int + p >= sizeof(uint32_t)) {
                            // update loop counter used for reading from file
                            k = p;
                            break;
                        }
                        // std::cout << "Bytes read into int: " << bytes_read_into_int + p << std::endl;
                        current_int[bytes_read_into_int + p] = buffer[p];
                    }

                    // create integer from buffer
                    memcpy(&integer, &current_int, sizeof(uint32_t));
                    // std::cout << "integer from prev: " << integer << std::endl;
                    num_from_prev = false;
                    bytes_read_into_int = 0;

                    // Check if character byte is available (k was updated to p)
                    if (k + 1 >= bytesRead) {
                        // std::cerr << "\nIncomplete record at buffer boundary in num_from_prev\n";
                        //   << std::endl;
                        break;
                    }

                    character = buffer[k + 1]; // Read next byte after k

                    // std::cout << integer << character;
                    // std::cout << "Prev k = " << k << ": Integer: " << integer << " Character: " << character << std::endl;
                    for (int32_t l = 0; l < integer; l++) {
                        // std::cout << character;
                        write(STDOUT_FILENO, &character, 1);
                    }
                    // std::cout << "Integer and character from prev: " << integer << character << std::endl;
                } else {

                    // Check if enough bytes remain for full integer
                    if (k + sizeof(uint32_t) > bytesRead) {
                        // save the integer for next iteration
                        // std::cout << "Bytes remaining: " << bytesRead - k << std::endl;
                        for (ssize_t j = k; j < bytesRead; j++) {
                            current_int[j - k] = buffer[j];
                            bytes_read_into_int++;
                        }
                        num_from_prev = true;
                        // std::cerr << "\nIncomplete integer at buffer boundary. Was able to read: " << bytes_read_into_int
                        // << std::endl;
                        break;

                    } else {

                        if (!set_num_from_prev) {
                            memcpy(&integer, &buffer[k], sizeof(integer));
                            // Check if character byte is available
                            if (k + 4 >= bytesRead) {
                                std::cerr << "k + 4 (" << k + 4 << ") >= bytesRead(" << bytesRead << ")" << std::endl;
                                std::cerr << "Incomplete record at buffer boundary with integer set: " << integer << std::endl;
                                set_num_from_prev = true;
                                break;
                            }

                            character = buffer[k += 4];
                        } else {
                            character = buffer[k];
                        }

                        // // Check if character byte is available (k was updated to p)
                        // if (k + 1 >= bytesRead) {
                        //     std::cerr << "\nIncomplete record at buffer boundary in num_from_prev\n";
                        //     //   << std::endl;
                        //     break;
                        // }

                        // std::cout << integer << character;
                        // std::cout << "New k = " << k << ": Integer: " << integer << " Character: " << character << std::endl;
                        set_num_from_prev = false;

                        for (int32_t l = 0; l < integer; l++) {
                            // std::cout << character;
                            write(STDOUT_FILENO, &character, 1);
                        }
                        // printf("Byte %zu: 0x%02x\n", k, buffer[k]);
                    }
                }
            }
        }

        if (bytesRead == -1) {
            std::cout << "wzip: Failed to read" << std::endl;
        }

        close(fd);
    }

    return 0;
}
