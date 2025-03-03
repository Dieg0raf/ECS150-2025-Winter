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

/**
 * Lookup an inode.
 *
 * Takes the parent inode number (which should be the inode number
 * of a directory) and looks up the entry name in it. The inode
 * number of name is returned.
 *
 * Success: return inode number of name
 * Failure: return -ENOTFOUND, -EINVALIDINODE.
 * Failure modes: invalid parentInodeNumber, name does not exist.
 */
int LocalFileSystem::lookup(int parentInodeNumber, string name)
{
    inode_t parentInode;
    if (this->stat(parentInodeNumber, &parentInode) != 0) {
        return -EINVALIDINODE;
    }

    if (parentInode.type != UFS_DIRECTORY) {
        return -EINVALIDINODE;
    }

    // Read in raw data
    size_t dirSize = parentInode.size;
    char buffer[dirSize];
    if (this->read(parentInodeNumber, buffer, dirSize) == 0) {
        return -EINVALIDINODE;
    }

    // Iterate through directory entires
    size_t amountOfEntiresToRead = dirSize / sizeof(dir_ent_t);
    for (int j = 0; j < amountOfEntiresToRead; j++) {
        dir_ent_t dirEntry;
        memcpy(&dirEntry, buffer + (j * sizeof(dir_ent_t)), sizeof(dir_ent_t));
        if (dirEntry.name == name) {
            return dirEntry.inum;
        }
    }

    return -ENOTFOUND;
}

/**
 * Read an inode.
 *
 * Given an inodeNumber this function will fill in the `inode` struct with
 * the type of the entry and the size of the data, in bytes, and direct blocks.
 *
 * Success: return 0
 * Failure: return -EINVALIDINODE
 * Failure modes: invalid inodeNumber
 */
int LocalFileSystem::stat(int inodeNumber, inode_t* inode)
{
    super_t super;
    this->readSuperBlock(&super);

    // Validate inode number
    size_t amountOfInodes = super.num_inodes;
    if (inodeNumber < 0 || inodeNumber >= amountOfInodes) {
        return -EINVALIDINODE;
    }

    // Calculation inode offset & block offset
    size_t inodes_per_block = UFS_BLOCK_SIZE / sizeof(inode_t);
    size_t block_offset = inodeNumber / inodes_per_block;
    size_t inode_block = super.inode_region_addr + block_offset;
    size_t inode_offset_in_block = inodeNumber % inodes_per_block;

    // Copy bytes from disk block to buffer & then into inode stuct
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(inode_block, buffer);
    memcpy(inode, buffer + (inode_offset_in_block * sizeof(inode_t)), sizeof(inode_t));

    return 0;
}

/**
 * Read the contents of a file or directory.
 *
 * Reads up to `size` bytes of data into the buffer from file specified by
 * inodeNumber. The routine should work for either a file or directory;
 * directories should return data in the format specified by dir_ent_t.
 *
 * Success: number of bytes read
 * Failure: -EINVALIDINODE, -EINVALIDSIZE.
 * Failure modes: invalid inodeNumber, invalid size.
 */
int LocalFileSystem::read(int inodeNumber, void* buffer, int size)
{
    // always read data starting from the beginning of the file.
    // if size IS LESS than the size of object to read, then return only those bytes.
    // if size IS GREATER than the size of object to read, then return the bytes in the object.

    // (1) Find the inode
    // (2) Read the raw bytes from the direct blocks
    // (3) Copy those bytes to the provided buffer
    // (4) Return how many bytes were read

    // Find the inode
    inode_t inode;
    if (this->stat(inodeNumber, &inode) != 0) {
        return -EINVALIDINODE;
    }

    size_t blocksNeeded = ceil(static_cast<double>(inode.size) / UFS_BLOCK_SIZE);
    size_t bytesToRead = min(static_cast<size_t>(size), static_cast<size_t>(inode.size));
    size_t bytesRead = 0;

    // Read the raw bytes from the direct blocks
    for (size_t i = 0; i < blocksNeeded; i++) {
        char blockBuffer[UFS_BLOCK_SIZE];
        this->disk->readBlock(inode.direct[i], blockBuffer);

        // Calculate where in the destination buffer this block goes
        size_t destOffset = i * UFS_BLOCK_SIZE; // Offset in destination buffer to start writing from
        size_t bytesToCopy = min(static_cast<size_t>(UFS_BLOCK_SIZE), static_cast<size_t>(bytesToRead - bytesRead));
        memcpy((char*)buffer + (destOffset), blockBuffer, bytesToCopy);
        bytesRead += bytesToCopy;
    }

    return bytesRead;
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

