// cerr, cout, and cin are objects provided by the standard input/output stream library (<iostream>).
#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sstream>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
  // Returns a integer file descriptor
  int fileDescriptor = open(argv[1], O_RDONLY);

  // If the file descriptor is less than 0, then the file could not be opened (reference man pages)
  if (fileDescriptor < 0) {
    std::cerr << strerror(errno) << std::endl;
    exit(1);

  } else {
    std::cout << "opened file: (" << fileDescriptor << ")" << std::endl;
    // Allocated memory where we can store the data read from the file (Read from file into buffer)
    char buffer[BUFFER_SIZE];
    // Read from file into buffer

    // ssize_t is a signed integer type used to represent the sizes of objects in bytes
    ssize_t bytesRead = read(fileDescriptor, buffer, BUFFER_SIZE);
    std::cout << "Amount of bytes read: " << bytesRead << std::endl;

    if (bytesRead < 0) {
      std::cerr << strerror(errno) << std::endl;
      exit(1);
    }

    write(1, buffer, bytesRead);

    close(fileDescriptor);
  }

}
