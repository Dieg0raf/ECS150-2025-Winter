#include <iostream>
#include <string>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "Disk.h"
#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;

void printError(string srcFile)
{
    cout << "Could not write to " << srcFile << endl;
}

int openFile(string srcFile)
{
    int fd = open(srcFile.c_str(), O_RDONLY);
    if (fd == -1) {
        cerr << "Error opening file " << srcFile << endl;
        return -1;
    }

    return fd;
}

int validateInode(char* argv[])
{
    int inodeNumber;
    try {
        inodeNumber = stoi(argv[3]);
        if (inodeNumber < 0) {
            return -1;
        }
    } catch (const exception& e) {
        return -1;
    }

    return inodeNumber;
}

int startCopy(LocalFileSystem* const fileSystem, const int dstInode, string srcFile, const int fd)
{
    // get size of file
    struct stat st;
    if (stat(srcFile.c_str(), &st) != 0) {
        cerr << "Error getting file size" << endl;
        return -1;
    }

    // copy file into buffer
    ssize_t fileSize = st.st_size;
    char* buffer = new char[fileSize];
    if (read(fd, buffer, fileSize) != fileSize) {
        cerr << "Error reading file" << endl;
        delete[] buffer;
        return -1;
    }

    // write buffer to disk
    int bytesWritten = fileSystem->write(dstInode, buffer, fileSize);
    if (bytesWritten < 0) {
        delete[] buffer;
        return -1;
    }

    delete[] buffer;
    return bytesWritten;
}

int main(int argc, char* argv[])
{
    if (argc != 4) {
        cerr << argv[0] << ": diskImageFile src_file dst_inode" << endl;
        cerr << "For example:" << endl;
        cerr << "    $ " << argv[0] << " tests/disk_images/a.img dthread.cpp 3" << endl;
        return 1;
    }

    // Parse command line arguments
    Disk* disk = new Disk(argv[1], UFS_BLOCK_SIZE);
    LocalFileSystem* fileSystem = new LocalFileSystem(disk);
    string srcFile = string(argv[2]);

    // validate inode number
    int dstInode = validateInode(argv);
    if (dstInode < 0) {
        printError(srcFile);
        delete fileSystem;
        delete disk;
    }

    // open file
    int fd = openFile(srcFile);
    if (fd == -1) {
        return 1;
    }

    // Run copy of file
    int returnCode = 0;
    int bytesWritten = startCopy(fileSystem, dstInode, srcFile, fd);
    if (bytesWritten < 0) {
        printError(srcFile);
        returnCode = 1;
    }

    close(fd);
    delete fileSystem;
    delete disk;

    return returnCode;
}

