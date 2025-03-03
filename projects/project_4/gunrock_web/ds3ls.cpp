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

// Path handling functions
vector<string> splitPath(const string& path)
{
    vector<string> components;
    if (path.empty()) {
        return components;
    }

    size_t start = (path.front() == '/') ? 1 : 0;
    size_t end;

    while (start < path.length()) {
        end = path.find('/', start);
        if (end == string::npos) {
            end = path.length();
        }

        if (end > start) { // Extract path component (if not empty)
            components.push_back(path.substr(start, end - start));
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

    // Split path into components
    pathComponents = splitPath(path);

    return true;
}

// Directory entry comparison for sorting
bool compareByName(const dir_ent_t& a, const dir_ent_t& b)
{
    return std::strcmp(a.name, b.name) < 0;
}

void printError()
{
    cerr << "Directory not found" << endl;
}

// Get and sort directory entries
vector<dir_ent_t> getDirectoryEntries(LocalFileSystem* const fileSystem, const inode_t* inode, size_t inodeNum)
{
    // Read directory data
    vector<dir_ent_t> dirEntries;
    size_t dirSize = inode->size;
    char buffer[dirSize];
    if (fileSystem->read(inodeNum, buffer, dirSize) == 0) {
        return dirEntries;
    }

    // Extract directory entries
    size_t entryCount = dirSize / sizeof(dir_ent_t);
    for (size_t j = 0; j < entryCount; j++) {
        dir_ent_t dirEntry;
        memcpy(&dirEntry, buffer + (j * sizeof(dir_ent_t)), sizeof(dir_ent_t));
        dirEntries.push_back(dirEntry);
    }

    // Sort entries by name
    sort(dirEntries.begin(), dirEntries.end(), compareByName);

    return dirEntries;
}

// Print directory entries
void printDirectoryEntries(const vector<dir_ent_t>& entries)
{
    for (size_t i = 0; i < entries.size(); i++) {
        cout << entries[i].inum << "\t" << entries[i].name << endl;
    }
}

int navigateToDirectory(LocalFileSystem* const fileSystem, const vector<string>& pathComponents, size_t& targetInode)
{
    // Handle root directory
    if (pathComponents.empty()) {
        targetInode = 0;
        return 0;
    }

    // Navigate through path
    size_t currentInode = 0;
    for (size_t i = 0; i < pathComponents.size(); i++) {
        size_t nextInode = fileSystem->lookup(currentInode, pathComponents[i]);
        if (nextInode == -EINVALIDINODE || nextInode == -ENOTFOUND) {
            return 1;
        }
        currentInode = nextInode;
    }
    targetInode = currentInode;

    return 0;
}

int listDirectoryContents(LocalFileSystem* const fileSystem, const string& directory)
{
    // Validate directory path
    vector<string> pathComponents;
    if (!isValidDirectoryPath(directory, pathComponents)) {
        printError();
        return 1;
    }

    // Navigate to target directory
    size_t targetInode;
    if (navigateToDirectory(fileSystem, pathComponents, targetInode) != 0) {
        printError();
        return 1;
    }

    // Get inode information for target
    inode_t inode;
    if (fileSystem->stat(targetInode, &inode) != 0) {
        printError();
        return 1;
    }

    // Handle file and directory differently
    if (inode.type == UFS_REGULAR_FILE) {
        string fileName = pathComponents.back();
        cout << targetInode << "\t" << fileName << endl;
    } else {
        // For directories, get and print all entries
        vector<dir_ent_t> entries = getDirectoryEntries(fileSystem, &inode, targetInode);
        printDirectoryEntries(entries);
    }

    return 0;
}

int main(int argc, char* argv[])
{
    // Check command line arguments
    if (argc != 3) {
        cerr << argv[0] << ": diskImageFile directory" << endl;
        cerr << "For example:" << endl;
        cerr << "    $ " << argv[0] << " tests/disk_images/a.img /a/b" << endl;
        return 1;
    }

    // Parse command line arguments
    Disk* disk = new Disk(argv[1], UFS_BLOCK_SIZE);
    LocalFileSystem* fileSystem = new LocalFileSystem(disk);
    string directory = string(argv[2]);

    // Start listing process
    int returnCode = listDirectoryContents(fileSystem, directory);

    // Clean up
    delete fileSystem;
    delete disk;
    return returnCode;
}
