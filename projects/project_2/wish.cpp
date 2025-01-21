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

// Trim string
std::string trim(const std::string& str)
{
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// Remove trailing slashes from path (keeps paths the same - no trailing slash)
std::string standardizePath(std::string path)
{
    while (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
    return path;
}

// Creates executable path
std::string getExecutablePath(const std::string& directory, const std::string& command)
{
    return directory + "/" + command;
}

// Prints general error
void printError()
{
    char error_message[30] = "An error has occurred\n";
    if (write(STDERR_FILENO, error_message, strlen(error_message)) == -1) {
        // If write fails, exit immediately
        exit(1);
    }
}

// Handles the child process
void handleChildProcess(std::string& command, std::istringstream& stream, std::vector<std::string>& searchPaths)
{
    std::string binaryFilePath;
    ssize_t isAccessible;

    // check if a file exists in a directory and is executable
    for (unsigned i = 0; i < searchPaths.size(); i++) {
        std::string searchPath = getExecutablePath(searchPaths[i], command);
        isAccessible = access(searchPath.c_str(), X_OK);
        if (isAccessible == 0) {
            binaryFilePath = searchPath;
            break;
        }
    }

    if (isAccessible == -1) {
        printError();
        exit(1);
    }

    // add search path to args & store stream of args into new args vector
    std::vector<std::string> args;
    std::string arg;
    args.push_back(binaryFilePath);
    while (stream >> arg) {
        args.push_back(arg);
    }

    // allocate array of pointers
    char** argv = new char*[args.size() + 1];

    // convert to C strings and allocate memory for each string into argv array (holds arguments to put into execv() sys call)
    for (size_t i = 0; i < args.size(); i++) {
        argv[i] = strdup(args[i].c_str());
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

    // Initialize search paths
    std::vector<std::string> searchPaths;
    searchPaths.push_back("/bin");

    // Keep prompting user to give command
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

        // built-in 'path' command
        else if (command == "path") {
            // clear previous paths
            searchPaths.clear();

            // set new paths
            std::string binPath;
            while (stream >> binPath) {
                searchPaths.push_back(standardizePath(binPath));
            }
        }

        // built-in 'cd' command
        else if (command == "cd") {

            std::string dirPath;
            int16_t argCount = 0;
            while (stream >> dirPath) {
                argCount++;
            }

            if (argCount > 1) {
                printError();
                continue;
            }

            if (chdir(dirPath.c_str()) == -1) {
                printError();
                continue;
            }
        }

        else {
            // fork process (error check)
            pid_t childPID = fork();
            if (childPID == -1) {
                printError();
                continue;
            }

            // handle parent or child process
            if (childPID == 0) {
                handleChildProcess(command, stream, searchPaths);
            }

            if (wait(NULL) == -1) {
                printError();
            }
        }
    }

    return 0;
}