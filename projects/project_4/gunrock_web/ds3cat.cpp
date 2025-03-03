#include "Disk.h"
#include "LocalFileSystem.h"
#include "ufs.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>

using namespace std;

void printError()
{
    cerr << "Error reading file" << endl;
}

void printFileBlocks(inode_t* const inode)
{
    // writes File Blocks to STDOUT
    cout << "File blocks" << endl;
    size_t blocksNeeded = std::ceil(static_cast<double>(inode->size) / UFS_BLOCK_SIZE);
    for (size_t i = 0; i < blocksNeeded; i++) {
        cout << inode->direct[i] << endl;
    }
    cout << endl;
}

int printFileData(char* buffer, size_t fileSize)
{
    // writes File Data to STDOUT
    int rc;
    cout << "File data" << endl;
    rc = write(STDOUT_FILENO, buffer, fileSize);
    if (rc == -1) {
        return 1;
    }

    return 0;
}

int printFileContents(LocalFileSystem* const fileSystem, const int inodeNumber)
{
    // fetch inode object
    inode_t inode;
    if (fileSystem->stat(inodeNumber, &inode) == -EINVALIDINODE) {
        return 1;
    }

    if (inode.type == UFS_DIRECTORY) {
        return 1;
    }

    // reads file contents from disk
    size_t fileSize = inode.size;
    char buffer[fileSize];
    if (fileSystem->read(inodeNumber, buffer, fileSize) == 0) {
        return 1;
    }

    // print to STDOUT
    printFileBlocks(&inode);
    if (printFileData(buffer, fileSize) == 1) {
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        cerr << argv[0] << ": diskImageFile inodeNumber" << endl;
        return 1;
    }

    // Parse command line arguments
    Disk* disk = new Disk(argv[1], UFS_BLOCK_SIZE);
    LocalFileSystem* fileSystem = new LocalFileSystem(disk);
    int inodeNumber = stoi(argv[2]);

    // Print file contents
    int returnCode = printFileContents(fileSystem, inodeNumber);
    if (returnCode != 0) {
        printError();
    }

    delete fileSystem;
    delete disk;
    return returnCode;
}

