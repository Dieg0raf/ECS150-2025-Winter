#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

struct Command {
    std::vector<std::string> args;
    std::string outputFile;
    bool hasRedirection;
    bool isParallel;
};

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

bool isBlank(const std::string& str)
{
    return str.find_first_not_of(" \t\n\r") == std::string::npos;
}

// Handles the child process
void handleChildProcess(Command& currentCommand, std::string binaryFilePath)
{
    // convert to C strings and allocate memory for each string into argv array (holds arguments to put into execv() sys call)
    char** argv = new char*[currentCommand.args.size() + 1];
    for (size_t i = 0; i < currentCommand.args.size(); i++) {
        argv[i] = strdup(currentCommand.args[i].c_str());
    }
    argv[currentCommand.args.size()] = NULL;

    // check if redirection has to be done
    if (currentCommand.hasRedirection) {
        ssize_t fd = open(currentCommand.outputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            printError();
            exit(1);
        }

        // standard output points to the same file as fd
        if (dup2(fd, STDOUT_FILENO) == -1) {
            printError();
            exit(1);
        }
        close(fd);
    }

    // swap program in child process and run new program (check if execv fails)
    // std::cout << "Right before swapping programs!" << std::endl;
    if (execv(binaryFilePath.c_str(), argv) == -1) {
        // perror(argv[0]);
        for (size_t i = 0; i < currentCommand.args.size(); i++) {
            free(argv[i]);
        }
        // std::cout << "In here!" << std::endl;
        delete[] argv;
        printError();
        exit(1);
    }
}

// Parses into their own command
std::vector<Command> parseCommands(std::istringstream& stream)
{
    std::vector<Command> commands;
    Command currentCommand;
    currentCommand.hasRedirection = false;
    currentCommand.isParallel = false;
    std::string arg;

    while (stream >> arg) {

        // handle single ampersand
        if (arg == "&") {
            if (!currentCommand.args.empty()) {
                currentCommand.isParallel = true;
                commands.push_back(currentCommand);
                currentCommand = Command(); // Reset for next command
            } else {
                return std::vector<Command>();
            }
            continue;
        }

        // handle cnonected string with ampersands
        if (arg.find("&") != std::string::npos) {
            // Split the argument by ampersands
            std::istringstream argStream(arg);
            std::string subArg;
            bool firstSubArg = true;
            currentCommand.isParallel = true;

            while (std::getline(argStream, subArg, '&')) {
                if (!isBlank(subArg)) {
                    if (!firstSubArg) {
                        // Add previous command and reset
                        if (!currentCommand.args.empty()) {
                            commands.push_back(currentCommand);
                            currentCommand = Command();
                        }
                    }

                    // handle redirection within the sub-argument
                    size_t redirectPos = subArg.find('>');
                    if (redirectPos != std::string::npos) {
                        std::string beforeRedirect = subArg.substr(0, redirectPos);
                        std::string afterRedirect = subArg.substr(redirectPos + 1);

                        if (!isBlank(beforeRedirect)) {
                            currentCommand.args.push_back(beforeRedirect);
                        }

                        if (!isBlank(afterRedirect)) {
                            if (currentCommand.hasRedirection) {
                                // std::cout << "Too many redirections" << std::endl;
                                return std::vector<Command>();
                            }
                            currentCommand.outputFile = afterRedirect;
                            currentCommand.hasRedirection = true;
                        }
                    } else {
                        currentCommand.args.push_back(subArg);
                    }

                    firstSubArg = false;
                }
            }
            continue;
        }

        // check for redirection(s)
        if (arg == ">") {
            if (currentCommand.args.size() < 1) {
                printError();
                return std::vector<Command>();
            }

            // std::cout << "Caught by this!" << std::endl;
            if (currentCommand.hasRedirection) {
                // std::cout << "To many re-directions" << std::endl;
                printError();
                return std::vector<Command>();
                // exit(1);
            }

            currentCommand.hasRedirection = true;
            if (!(stream >> currentCommand.outputFile)) {
                // std::cout << "No file specified" << std::endl;
                printError();
                return std::vector<Command>();
                // exit(1);
            }

            // std::cout << "Output file set: " << currentCommand.outputFile << std::endl;
            continue;
        }

        size_t firstPos = arg.find(">");
        size_t secondPos = arg.find(">", firstPos + 1);

        // Check if theres more than one redirection
        if (secondPos != std::string::npos || currentCommand.hasRedirection) {
            // std::cout << "To many re-directions" << std::endl;
            // std::cout << "this is the args: " << arg << std::endl;
            printError();
            return std::vector<Command>();
        }

        // Only one redirection is present
        if (firstPos != std::string::npos) {
            // std::cout << "Character found!" << std::endl;
            currentCommand.hasRedirection = true;

            std::string firstHalf = arg.substr(0, firstPos);
            std::string secondHalf = arg.substr(firstPos + 1);

            if (!isBlank(firstHalf)) {
                currentCommand.args.push_back(firstHalf);
                // std::cout << "First half: " << firstHalf << std::endl;
            }

            if (!isBlank(secondHalf)) {
                currentCommand.outputFile = secondHalf;
                // std::cout << "Second half: " << currentCommand.outputFile << std::endl;
            }

            continue;
        }

        currentCommand.args.push_back(arg);
    }

    if (!currentCommand.args.empty()) {
        commands.push_back(currentCommand);
    }

    return commands;
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
        std::istringstream secondStream(line);

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

        // Handle non-built in commands
        else {
            std::vector<Command> commands = parseCommands(secondStream);
            if (commands.empty()) {
                continue;
            }

            // Iterate through all commands (children vector initialized for parallel commands)
            std::vector<pid_t> children;
            for (size_t k = 0; k < commands.size(); k++) {

                std::string binaryFilePath;
                ssize_t isAccessible;

                // check if a file exists in a directory and is executable
                for (size_t i = 0; i < searchPaths.size(); i++) {
                    std::string searchPath = getExecutablePath(searchPaths[i], commands[k].args[0]);
                    isAccessible = access(searchPath.c_str(), X_OK);
                    if (isAccessible == 0) {
                        binaryFilePath = searchPath;
                        break;
                    }
                }

                if (isAccessible == -1) {
                    printError();
                    continue;
                }

                // set executable file path as the first argument in args
                // commands[k].args[0] = binaryFilePath;
                // std::cout << "Binary file path: " << commands[k].args[0] << std::endl;
                pid_t childPID = fork();
                if (childPID == -1) {
                    printError();
                    continue;
                }

                if (childPID == 0) {
                    handleChildProcess(commands[k], binaryFilePath);
                } else {
                    if (commands[0].isParallel) {
                        children.push_back(childPID);
                        continue;
                    }

                    if (wait(NULL) == -1) {
                        printError();
                    }
                }
            }

            // Wait for commands in parallel if needed
            if (commands[0].isParallel) {
                for (size_t i = 0; i < children.size(); i++) {
                    // keep trying to wait if wait fails
                    while (waitpid(children[i], nullptr, 0) == -1) {
                        if (errno == EINTR)
                            continue;
                        printError();
                        break;
                    }
                }
            }
        }
    }

    return true;
}

int main(int argc, char* argv[])
{
    // Check if more than one file are passed (for batch mode)
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
