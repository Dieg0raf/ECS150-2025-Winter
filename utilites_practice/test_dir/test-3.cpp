#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    int rc = fork();
    if (rc < 0) {
        // fork failed
        fprintf(stderr, "fork failed\n");
        exit(1);
    } else if (rc == 0) {
        // child: redirect standard output to a file
        close(STDOUT_FILENO);
        open("./p4.output", O_CREAT | O_WRONLY | O_TRUNC,
            S_IRWXU);
        // now exec "wc"...
        char* myargs[3];
        myargs[0] = strdup("wc"); // program: wc
        myargs[1] = strdup("test-2.cpp"); // arg: file to count
        myargs[2] = NULL; // mark end of array
        execvp(myargs[0], myargs); // runs word count
    } else {
        // parent goes down this path (main)
        int rc_wait = wait(NULL);

        if (rc_wait < 0) {
            std::cerr << "wait failed" << std::endl;
            exit(1);
        }
    }
    return 0;
}