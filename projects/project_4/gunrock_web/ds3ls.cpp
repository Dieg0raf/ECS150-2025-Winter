#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "Disk.h"
#include "LocalFileSystem.h"
#include "StringUtils.h"
#include "ufs.h"

using namespace std;

// Split the path without validation
vector<string> splitPath(const string& path)
{
    vector<string> components;
    size_t start = (path.front() == '/') ? 1 : 0;
    size_t end;

    while (start < path.length()) {
        end = path.find('/', start); // Find the next slash
        if (end == string::npos) {
            end = path.length();
        }

        if (end > start) { // Extract path component (if not empty)
            string component = path.substr(start, end - start);
            components.push_back(component);
        }
        start = end + 1;
    }

    return components;
}

bool isValidDirectoryPath(const string& path, vector<string>& pathComponents)
{
    // Check for empty path
    if (path.empty()) {
        return false;
    }

    // Check for leading slash
    if (path[0] != '/') {
        return false;
    }

    // Check for consecutive slashes
    for (size_t i = 1; i < path.length(); i++) {
        if (path[i] == '/' && path[i - 1] == '/') {
            return false;
        }
    }

    // split paths & check for root directory
    pathComponents = splitPath(path);
    if (pathComponents.empty()) {
        return true;
    }

    return true;
}

void printError()
{
    cerr << "Directory not found" << endl;
}

// Use this function with std::sort for directory entries
bool compareByName(const dir_ent_t& a, const dir_ent_t& b)
{
    return std::strcmp(a.name, b.name) < 0;
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        cerr << argv[0] << ": diskImageFile directory" << endl;
        cerr << "For example:" << endl;
        cerr << "    $ " << argv[0] << " tests/disk_images/a.img /a/b" << endl;
        return 1;
    }

    // parse command line arguments
    Disk* disk = new Disk(argv[1], UFS_BLOCK_SIZE);
    LocalFileSystem* fileSystem = new LocalFileSystem(disk);
    string directory = string(argv[2]);

    vector<string> pathComponents;
    if (!isValidDirectoryPath(directory, pathComponents)) {
        printError();
        delete fileSystem;
        delete disk;
        return 1;
    }

    if (pathComponents.empty()) {
        inode_t inode;
        if (fileSystem->stat(0, &inode) != 0) {
            return -EINVALIDINODE;
        }

        if (inode.type != UFS_DIRECTORY) {
            return -EINVALIDINODE;
        }

        // Read in raw data
        size_t dirSize = inode.size;
        char buffer[dirSize];
        if (fileSystem->read(0, buffer, dirSize) == 0) {
            return -EINVALIDINODE;
        }

        // Iterate through directory entires
        vector<dir_ent_t> dirEntries;
        size_t amountOfEntiresToRead = dirSize / sizeof(dir_ent_t);
        for (int j = 0; j < amountOfEntiresToRead; j++) {
            dir_ent_t dirEntry;
            memcpy(&dirEntry, buffer + (j * sizeof(dir_ent_t)), sizeof(dir_ent_t));
            dirEntries.push_back(dirEntry);
            // cout << "name: " << dirEntry.name << endl;
            // cout << "inum: " << dirEntry.inum << endl;
        }

        // After you've collected all directory entries in the vector:
        sort(dirEntries.begin(), dirEntries.end(), compareByName);

        // Then display the sorted entries
        for (int k = 0; k < dirEntries.size(); k++) {
            cout << dirEntries[k].inum << "\t" << dirEntries[k].name << endl;
        }

        return 0;
    }

    size_t currentParentNode = 0;
    size_t firstInodeNum = fileSystem->lookup(0, pathComponents[0]);
    if (firstInodeNum == -EINVALIDINODE || firstInodeNum == -ENOTFOUND) {
        printError();
        return 1;
    }
    currentParentNode = firstInodeNum;

    for (size_t i = 1; i < pathComponents.size(); i++) {
        size_t inode_num = fileSystem->lookup(currentParentNode, pathComponents[i]);
        if (inode_num == -EINVALIDINODE || inode_num == -ENOTFOUND) {
            printError();
            return 1;
        }
        currentParentNode = inode_num;
    }

    inode_t inode;
    if (fileSystem->stat(currentParentNode, &inode) != 0) {
        return -EINVALIDINODE;
    }

    // Check if it's a file or directory
    if (inode.type == UFS_REGULAR_FILE) {
        // For files, just print the file information
        // Get the file name from the last path component
        string fileName = pathComponents.back();
        cout << currentParentNode << "\t" << fileName << endl;

        delete fileSystem;
        delete disk;
        return 0;
    }

    // Read in raw data
    size_t dirSize = inode.size;
    char buffer[dirSize];
    if (fileSystem->read(currentParentNode, buffer, dirSize) == 0) {
        return -EINVALIDINODE;
    }

    // Iterate through directory entires
    vector<dir_ent_t> dirEntries;
    size_t amountOfEntiresToRead = dirSize / sizeof(dir_ent_t);
    for (int j = 0; j < amountOfEntiresToRead; j++) {
        dir_ent_t dirEntry;
        memcpy(&dirEntry, buffer + (j * sizeof(dir_ent_t)), sizeof(dir_ent_t));
        dirEntries.push_back(dirEntry);
        // cout << "name: " << dirEntry.name << endl;
        // cout << "inum: " << dirEntry.inum << endl;
    }

    // After you've collected all directory entries in the vector:
    sort(dirEntries.begin(), dirEntries.end(), compareByName);

    // Then display the sorted entries
    for (int k = 0; k < dirEntries.size(); k++) {
        cout << dirEntries[k].inum << "\t" << dirEntries[k].name << endl;
    }

    delete fileSystem;
    delete disk;

    return 0;
}

