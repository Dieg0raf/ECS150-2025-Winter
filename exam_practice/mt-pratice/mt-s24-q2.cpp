#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
 
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <sstream>
using namespace std;
int main(void){
    int fd;
    cout << "Hello 1" << endl;
    fd=open("myfile.txt", O_WRONLY | O_CREAT, 0644); // open file

    if (fork()) { // parent process
      cout << "Hello 2" << endl; // print to terminal (1)
      wait(NULL);
      dup2(fd, STDOUT_FILENO);
      close(fd);

      cout << "Hello 3" << endl;
    } else { // child process
      cout << "Hello 4" << endl; // print to terminal (2)
      dup2(fd, STDOUT_FILENO); // redirect to file
      close(fd);

      cout << "Hello 5" << endl; // print to file
    }
    return 0;
}

//1> Hello 5
//2> Hello 3
