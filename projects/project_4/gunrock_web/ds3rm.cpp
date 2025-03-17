#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>

#include "Disk.h"
#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;

void printError()
{
    cerr << "Error removing entry" << endl;
}

int validateInode(char* argv[])
{
    int inodeNumber;
    try {
        inodeNumber = stoi(argv[2]);
        if (inodeNumber < 0) {
            return -1;
        }
    } catch (const exception& e) {
        return -1;
    }

    return inodeNumber;
}

int validateName(string fileName)
{
    if (fileName.empty() || fileName.length() >= DIR_ENT_NAME_SIZE || fileName.find('/') != string::npos) {
        // if (fileName.empty() || fileName.length() >= DIR_ENT_NAME_SIZE) {
        return -1;
    }

    // Check if string is only whitespace
    bool onlyWhitespace = true;
    for (int i = 0; i < static_cast<int>(fileName.size()); i++) {
        if (!isspace(fileName[i])) {
            onlyWhitespace = false;
            break;
        }
    }

    if (onlyWhitespace) {
        return -1;
    }

    return 0;
}

int startRemoval(LocalFileSystem* const fs, char* argv[])
{
    // validate inode number
    int parentInodeNumber = validateInode(argv);
    if (parentInodeNumber < 0) {
        return -1;
    }

    // validate entry name
    string entryName = string(argv[3]);
    if (validateName(entryName) != 0) {
        return -1;
    }

    int rc = fs->unlink(parentInodeNumber, entryName);
    if (rc == -ENOTFOUND) {
        return 0;
    }

    return rc;
}

int main(int argc, char* argv[])
{
    if (argc != 4) {
        cerr << argv[0] << ": diskImageFile parentInode entryName" << endl;
        return 1;
    }

    // Parse command line arguments
    Disk* disk = new Disk(argv[1], UFS_BLOCK_SIZE);
    LocalFileSystem* fileSystem = new LocalFileSystem(disk);

    // start removal process
    int rc = startRemoval(fileSystem, argv);
    if (rc < 0) {
        // printError();
        delete disk;
        delete fileSystem;
        return 0;
    }

    delete disk;
    delete fileSystem;
    return 1;
}

