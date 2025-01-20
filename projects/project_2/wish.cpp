#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>

std::string trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

void printError()
{
    char error_message[30] = "An error has occurred\n";
    if (write(STDERR_FILENO, error_message, strlen(error_message)) == -1) {
        // If write fails, exit immediately
        exit(1);
    }
}

void handleChildProcess(std::string& command, std::istringstream& stream)
{
    // Store args in vector & add binary path to args
    std::vector<std::string> args;
    std::string binaryFilePath = "/bin/" + command;
    args.push_back(binaryFilePath);

    std::string arg;
    while (stream >> arg) {
        args.push_back(arg);
        std::cout << "argument: " << arg << std::endl;
    }

    // allocate array of pointers
    char** argv = new char*[args.size() + 1];

    // Convert to C strings and allocate memory for each string into argv array (holds arguments to put into execv() sys call)
    for (size_t i = 0; i < args.size(); i++) {
        argv[i] = strdup(args[i].c_str());
        if (i == 0) {
            // std::cout << "command: " << args[i].c_str() << std::endl;
        }
        // std::cout << args[i].c_str() << std::endl;
    }

    argv[args.size()] = NULL;

    // swap program in child process and run new program (check if execv fails)
    if (execv(argv[0], argv) == -1) {
        // Cleanup if exec fails
        for (size_t i = 0; i < args.size(); i++) {
            free(argv[i]);
        }

        delete[] argv;

        printError();
        exit(1);
    }
}

int main(int argc, char* argv[])
{
    if (argc > 2) {
        printError();
        exit(1);
    }

    while (true) {
        std::string input;
        std::cout << "wish> ";
        std::getline(std::cin, input);
        std::istringstream stream(input);

        // get first word (command)
        std::string command;
        if (!(stream >> command)) {
            // Empty input, show prompt again
            continue;
        }

        // built-in 'exit' command
        if (command == "exit") {
            if (trim(stream.str()).length() > 4) {
                printError();
                continue;
            }
            exit(0);
        }

        // built-in 'cd' command
        if (command == "cd") {

            std::string dirPath;
            int16_t count = 0;
            while (stream >> dirPath) {
                count++;
            }

            if (count > 1) {
                // std::cout << "to many arguments passed" << std::endl;
                printError();
                continue;
            }

            if (chdir(dirPath.c_str()) == -1) {
                // std::cout << "changing directories failed: " << errno << std::endl;
                printError();
                continue;
            }
            continue;
        }

        // fork process (error check)
        pid_t childPID = fork();
        if (childPID == -1) {
            std::cerr << "Fork failed" << std::endl;
            continue;
        }

        // handle parent or child process
        if (childPID == 0) {
            // std::cout << "In the child process: " << getpid() << std::endl;
            handleChildProcess(command, stream);
        } else {

            if (wait(NULL) == -1) {
                std::cerr << errno << std::endl;
            }

            // std::cout << "In the parent process: " << getpid() << std::endl;
            // std::cout << "Waited for child process: " << childPID << std::endl;
        }

        // std::cout << "I'm done" << std::endl;
    }

    return 0;
}