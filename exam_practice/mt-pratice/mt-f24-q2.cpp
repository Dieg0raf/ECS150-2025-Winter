#include <iostream>
 
#include <fcntl.h>
#include <stdlib.h>
 
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <sstream>

using namespace std;

int main () {
    cout << "Parent starts" << endl; // (1)

    pid_t pid = fork(); // creates first child (2nd process)
    if (pid == 0) {
        cout << "Child starts" << endl; // (2)
        pid_t pid2 = fork(); // creates second child (3rd process)
        if (pid2 != 0) {
            wait(NULL);
            cout << "Child print" << endl; // (4)
        }
        cout << "Child prints again" << endl; // (3) // (5)
        return 0;
    }

    wait(NULL);
    cout << "Parent ends" << endl; // (6)
    return 0;
}

// Output:
// Parent starts
// Child starts
// Child prints again
// Child print
// Child prints again
// Parent ends
