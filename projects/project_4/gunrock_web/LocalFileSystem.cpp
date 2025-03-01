#include <assert.h>
#include <iostream>
#include <string>
#include <vector>

#include "LocalFileSystem.h"
#include "ufs.h"

// A disk image is essentially a file that contains a complete copy or replica of a storage device,
// like a hard drive or a floppy disk

using namespace std;

LocalFileSystem::LocalFileSystem(Disk* disk)
{
    this->disk = disk;
}

void LocalFileSystem::readSuperBlock(super_t* super)
{
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(0, buffer);
    memcpy(super, buffer, sizeof(super_t));
}

void LocalFileSystem::readInodeBitmap(super_t* super, unsigned char* inodeBitmap)
{
    int inode_bitmap_block = super->inode_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(inode_bitmap_block, buffer);
    memcpy(inodeBitmap, buffer, static_cast<int>(ceil(super->num_inodes / 8)));
}

// Inode bitmap bits are allocated when creating new file/directories
void LocalFileSystem::writeInodeBitmap(super_t* super, unsigned char* inodeBitmap)
{
}

void LocalFileSystem::readDataBitmap(super_t* super, unsigned char* dataBitmap)
{
    int data_bitmap_block = super->data_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(data_bitmap_block, buffer);
    memcpy(dataBitmap, buffer, static_cast<int>(ceil(super->num_data / 8)));
}

// data bitmap bits are allocated when writing file content
void LocalFileSystem::writeDataBitmap(super_t* super, unsigned char* dataBitmap)
{
}

void LocalFileSystem::readInodeRegion(super_t* super, inode_t* inodes)
{
    int inode_region_block = super->inode_region_addr;
    int num_inodes = super->num_inodes;
    char buffer[UFS_BLOCK_SIZE];
    // int inodes_per_block = UFS_BLOCK_SIZE / sizeof(inode_t);

    this->disk->readBlock(inode_region_block, buffer);
    memcpy(inodes, buffer, sizeof(inode_t) * num_inodes);

    // for (int i = 0; i < super->inode_bitmap_len; i++) {
    //     this->disk->readBlock(inode_region_block + i, buffer);

    //     int inodes_to_copy = min(inodes_per_block, num_inodes - (i * inodes_per_block));
    //     if (inodes_to_copy <= 0)
    //         break;

    //     // Copy inodes from the buffer to the inodes array
    //     memcpy(&inodes[i * inodes_per_block], buffer, inodes_to_copy * sizeof(inode_t));
    // }
}

void LocalFileSystem::writeInodeRegion(super_t* super, inode_t* inodes)
{
}

int LocalFileSystem::lookup(int parentInodeNumber, string name)
{
    return 0;
}

int LocalFileSystem::stat(int inodeNumber, inode_t* inode)
{
    return 0;
}

int LocalFileSystem::read(int inodeNumber, void* buffer, int size)
{
    // always read data starting from the beginning of the file.
    // if size IS LESS than the size of object to read, then return only those bytes.
    // if size IS GREATER than the size of object to read, then return the bytes in the object.
    return 0;
}

// **Create - Write - Unlink**
// resuse existing data blocks if possible
// if new file uses fewer data blocks than the old file, then free the extra data blocks.
// if new file uses more data blocks than the old file, then allocate new data blocks.

// Allocate both an inode and disk lock for directories.
// If can't allocate on or the other, free allocated inodes or disk blocks and return an error.
int LocalFileSystem::create(int parentInodeNumber, int type, string name)
{
    return 0;
}

// **Write**
// Not enough storage, then write as many bytes as you can.
// Return sucess to the caller with the number of bytes written.
int LocalFileSystem::write(int inodeNumber, const void* buffer, int size)
{
    return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name)
{
    return 0;
}
