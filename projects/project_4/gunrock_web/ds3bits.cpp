#include <algorithm>
#include <cstring>
#include <iostream>
#include <string>

#include "Disk.h"
#include "LocalFileSystem.h"
#include "ufs.h"

using namespace std;

void printSuperBlockInfo(super_t* super)
{
    cout << "Super" << endl;
    cout << "inode_region_addr " << super->inode_region_addr << endl;
    cout << "inode_region_len " << super->inode_region_len << endl;
    cout << "num_inodes " << super->num_inodes << endl;

    cout << "data_region_addr " << super->data_region_addr << endl;
    cout << "data_region_len " << super->data_region_len << endl;
    cout << "num_data " << super->num_data << endl;
    cout << endl;
}

void printBitMap(unsigned char* bitmap, int numBytes)
{
    for (int i = 0; i < numBytes; i++) {
        cout << (unsigned int)bitmap[i] << " ";
    }
    cout << endl;
}

void processDataBitMap(super_t* super, LocalFileSystem* fileSystem)
{
    // Process Data bit map
    int dataBitmapBytes = static_cast<int>(ceil(super->num_data / 8));
    unsigned char* dataBitmap = new unsigned char[dataBitmapBytes];
    fileSystem->readDataBitmap(super, dataBitmap);

    cout << "Data bitmap" << endl;
    printBitMap(dataBitmap, dataBitmapBytes);
    delete[] dataBitmap;
}

void processInodeBitMap(super_t* super, LocalFileSystem* fileSystem)
{
    // Process Inode bit map
    int inodeBitmapBytes = static_cast<int>(ceil(super->num_inodes / 8));
    unsigned char* inodeBitmap = new unsigned char[inodeBitmapBytes];
    fileSystem->readInodeBitmap(super, inodeBitmap);

    cout << "Inode bitmap" << endl;
    printBitMap(inodeBitmap, inodeBitmapBytes);
    delete[] inodeBitmap;
}

int main(int argc, char* argv[])
{
    if (argc != 2) {
        cerr << argv[0] << ": diskImageFile" << endl;
        return 1;
    }

    // Parse command line arguments
    Disk* disk = new Disk(argv[1], UFS_BLOCK_SIZE);
    LocalFileSystem* fileSystem = new LocalFileSystem(disk);

    // Read in super block
    super_t superBlock;
    fileSystem->readSuperBlock(&superBlock);

    // Process superblock info and bitmaps
    printSuperBlockInfo(&superBlock);
    processInodeBitMap(&superBlock, fileSystem);
    processDataBitMap(&superBlock, fileSystem);

    delete fileSystem;
    delete disk;

    return 0;
}
