#include <iostream>
#include <string>
#include <vector>

#include "Disk.h"
#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;

void printError()
{
    cerr << "Error creating directory" << endl;
}

int validateParentInode(char* argv[])
{
    int parentInode;
    try {
        parentInode = stoi(argv[2]);
        if (parentInode < 0) {
            return -1;
        }
    } catch (const exception& e) {
        return -1;
    }

    return parentInode;
}

int validateName(string fileName)
{
    if (fileName.empty() || fileName.length() >= DIR_ENT_NAME_SIZE || fileName.find('/') != string::npos) {
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    if (argc != 4) {
        cerr << argv[0] << ": diskImageFile parentInode directory" << endl;
        cerr << "For example:" << endl;
        cerr << "    $ " << argv[0] << " a.img 0 a" << endl;
        return 1;
    }

    // Parse command line arguments
    Disk* disk = new Disk(argv[1], UFS_BLOCK_SIZE);
    LocalFileSystem* fileSystem = new LocalFileSystem(disk);

    // validate parent inode
    int parentInode = validateParentInode(argv);
    if (parentInode < 0) {
        printError();
        delete fileSystem;
        delete disk;
        return 1;
    }

    // validate directory name
    string directory = string(argv[3]);
    if (validateName(directory) == 1) {
        printError();
        delete fileSystem;
        delete disk;
        return 1;
    }

    int returnCode = 0;
    if (fileSystem->create(parentInode, UFS_DIRECTORY, directory) < 0) {
        printError();
        returnCode = 1;
    }

    delete fileSystem;
    delete disk;

    return returnCode;
}

