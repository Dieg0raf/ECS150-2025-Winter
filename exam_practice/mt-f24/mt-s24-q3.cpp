
#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
 
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <sstream>
using namespace std;

int main (int argc, char *argv[]) {
  vector<pid_t> pids;
  for (int idx = 1; idx < argc; idx++) {
    string filename = argv[idx];
    pid_t pid = fork();
    if (pid == 0) { // child process
      // args to pass to execv
      char *args[3];
      args[0] = strdup("-f");
      args[1] = strdup(filename.c_str());
      args[2] = NULL;

      // will replace program with "/bin/rm" executable and execute "rm -f filename'
      execv("/bin/rm", args);
    } 

    // parent process (execv creates a whole different program, if it is successful it returns nothing)
    // meaning it doens't get to this point
    pids.push_back(pid);
  }

  // wait for all the children processes to finish (order doesn't matter - meaning they are running in parallel)
  for (size_t idx = 0; idx < pids.size(); idx++) {
    waitpid(pids[idx], nullptr, 0);
  }

  cout << "all files deleted" << endl;

}
