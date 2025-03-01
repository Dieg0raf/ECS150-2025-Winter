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

    pathComponents = splitPath(path);
    if (pathComponents.empty()) { // check for root directory
        return true;
    }

    return true;
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
        cerr << "Directory not found" << endl;
        delete fileSystem;
        delete disk;
        return 1;
    }

    for (size_t i = 0; i < pathComponents.size(); i++) {
        cout << "  [" << i << "]: " << pathComponents[i] << endl;
    }

    delete fileSystem;
    delete disk;

    return 0;
}
