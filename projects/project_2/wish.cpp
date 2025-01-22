#include <fcntl.h>
#include <fstream>
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
        exit(1);
    }
}

// Handles the child process
void handleChildProcess(const std::string& command, std::istringstream& stream, const std::vector<std::string>& searchPaths)
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
        // if (arg == ">") {
        // }
        args.push_back(arg);
    }

    // convert to C strings and allocate memory for each string into argv array (holds arguments to put into execv() sys call)
    char** argv = new char*[args.size() + 1];
    for (size_t i = 0; i < args.size(); i++) {
        argv[i] = strdup(args[i].c_str());
    }
    argv[args.size()] = NULL;

    // swap program in child process and run new program (check if execv fails)
    if (execv(argv[0], argv) == -1) {
        for (size_t i = 0; i < args.size(); i++) {
            free(argv[i]);
        }
        delete[] argv;
        printError();
        exit(1);
    }
}

bool runShell(std::istream& input, bool isInteractive)
{

    // Initialize search paths
    std::vector<std::string> searchPaths;
    searchPaths.push_back("/bin");

    // Run shell
    while (true) {
        if (isInteractive) {
            std::cout << "wish> ";
        }

        // get full line from input (could either be one or more lines)
        std::string line;
        if (!getline(input, line)) {
            break;
        }

        // get first word (command)
        std::istringstream stream(line);
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
            size_t argCount = 0;
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
            // fork process
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

    return true;
}

int main(int argc, char* argv[])
{
    if (argc > 2) {
        printError();
        return 1;
    }

    // Check if batch mode (default is interactive)
    if (argc > 1) {
        std::ifstream inFile;
        inFile.open(argv[1]);

        if (!inFile) {
            printError();
            return 1;
        }

        // Run shell in batch mode
        return runShell(inFile, false) ? 0 : 1;
    }

    // Run shell in interactive mode
    return runShell(std::cin, true) ? 0 : 1;
}