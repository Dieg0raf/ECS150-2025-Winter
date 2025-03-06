#include "LocalFileSystem.h"
#include "ufs.h"
#include <assert.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// A disk image is essentially a file that contains a complete copy or replica
// of a storage device, like a hard drive or a floppy disk

using namespace std;

int findFirstFreeBit(unsigned char* bitmap, size_t bitmapBytes,
    size_t totalBits)
{
    for (size_t i = 0; i < bitmapBytes; i++) {
        bitset<8> bits(bitmap[i]);

        // Skip fully used bytes
        if (bits.all()) {
            continue;
        }

        // Find first 0 bit in this byte
        for (int j = 0; j < 8; j++) {

            // Don't exceed total bits
            size_t bitIndex = (i * 8) + j;
            if (bitIndex >= totalBits) {
                return -1;
            }

            if (!bits[j]) { // If bit is 0 (not set)
                return bitIndex;
            }
        }
    }

    return -1;
}

// Set a bit at position
void setBit(unsigned char* bitmap, const int position)
{
    int byteIndex = position / 8;
    int bitOffset = position % 8;
    bitmap[byteIndex] |= (1 << bitOffset);
}

// Private helper functions declared at file scope (outside the class)
static int allocateNewInode(LocalFileSystem* const fs, super_t* super, int type)
{
    // Process Inode bitmap
    size_t inodeBitmapBytes = static_cast<int>(ceil(super->num_inodes / 8));
    unsigned char* inodeBitmap = new unsigned char[inodeBitmapBytes];
    fs->readInodeBitmap(super, inodeBitmap);

    // Find first free bit (free inode)
    int freeInodeIndex = findFirstFreeBit(inodeBitmap, inodeBitmapBytes, super->num_inodes - 1);
    if (freeInodeIndex < 0) {
        delete[] inodeBitmap;
        return -ENOTENOUGHSPACE;
    }

    setBit(inodeBitmap, freeInodeIndex); // Set bit in bitmap
    fs->writeInodeBitmap(super, inodeBitmap); // Write bitmap to disk
    delete[] inodeBitmap;

    return freeInodeIndex;
}

static int allocateDataBlock(LocalFileSystem* const fs, super_t* const super)
{
    // Process Data bitmap
    size_t dataBitmapBytes = static_cast<int>(ceil(super->num_data / 8));
    unsigned char* dataBitmap = new unsigned char[dataBitmapBytes];
    fs->readDataBitmap(super, dataBitmap);

    // Find first free bit (free block)
    int freeDataBlockIndex = findFirstFreeBit(dataBitmap, dataBitmapBytes, super->num_data - 1);
    if (freeDataBlockIndex < 0) {
        delete[] dataBitmap;
        return -ENOTENOUGHSPACE;
    }

    setBit(dataBitmap, freeDataBlockIndex); // Set bit in bitmap
    fs->writeDataBitmap(super, dataBitmap); // Write bitmap to disk

    // Initialize the block with zeros
    char emptyBlock[UFS_BLOCK_SIZE] = { 0 };
    fs->disk->writeBlock(freeDataBlockIndex, emptyBlock);

    delete[] dataBitmap;
    return freeDataBlockIndex;
}

static int validateInodeNumber(int parentInodeNumber, int amountOfNodes)
{
    return (parentInodeNumber < 0 || parentInodeNumber >= amountOfNodes) ? -EINVALIDINODE : 0;
}

static int validateCreateParameters(LocalFileSystem* const fs, super_t* const super, int parentInodeNumber, int type, string name)
{
    // Check if parent inode number is valid
    int parentInodeValidation = validateInodeNumber(parentInodeNumber, super->num_inodes);
    if (parentInodeValidation != 0) {
        return parentInodeValidation;
    }

    // Validate type
    if (type != UFS_DIRECTORY && type != UFS_REGULAR_FILE) {
        cout << "Invalid file type" << endl;
        return -EINVALIDTYPE;
    }

    // Check file name size
    if (name.size() >= DIR_ENT_NAME_SIZE) {
        cout << "Invalid name size" << endl;
        return -EINVALIDNAME;
    }

    // Check if name already exists in parent directory
    int existingInodeNum = fs->lookup(parentInodeNumber, name);
    if (existingInodeNum > 0) {
        inode_t existingInode;
        fs->stat(existingInodeNum, &existingInode);
        if (existingInode.type == type) {
            cout << "Name already exists and is of the same type";
            return existingInodeNum; // Success, return existing inode
        }
        cout << "Name already exists and but different TYPE" << endl;
        return -EINVALIDTYPE;
    }

    return 0; // All checks passed
}

static int writeDirectoryData(LocalFileSystem* const fs, super_t* const super, inode_t& parentInode, char* data, size_t dataSize, size_t oldSize)
{

    size_t blocksNeeded = std::ceil(static_cast<double>(dataSize) / UFS_BLOCK_SIZE);
    size_t allocatedBlocks = std::ceil(static_cast<double>(oldSize) / UFS_BLOCK_SIZE);

    // Allocate additional blocks if needed
    if (blocksNeeded > allocatedBlocks) {
        for (size_t i = allocatedBlocks; i < blocksNeeded; i++) {
            int newBlockNum = allocateDataBlock(fs, super);
            if (newBlockNum < 0) {
                return newBlockNum; // Error code from allocateDataBlock
            }
            parentInode.direct[i] = newBlockNum;
        }
    }

    // Write data to blocks
    size_t bytesWritten = 0;
    char bufferToWrite[UFS_BLOCK_SIZE];

    for (size_t i = 0; i < blocksNeeded; i++) {
        size_t blockToWrite = parentInode.direct[i];
        size_t destOffset = i * UFS_BLOCK_SIZE;
        size_t bytesToCopy = min(static_cast<size_t>(UFS_BLOCK_SIZE), static_cast<size_t>(dataSize - bytesWritten));

        memset(bufferToWrite, 0, UFS_BLOCK_SIZE);
        memcpy(bufferToWrite, data + destOffset, bytesToCopy);

        fs->disk->writeBlock(blockToWrite, bufferToWrite);
        bytesWritten += bytesToCopy;
    }

    return 0;
}

static int updateParentDirectory(LocalFileSystem* const fs, super_t* const super, vector<inode_t>& inodes, int parentInodeNumber, int newInodeNum, string name)
{
    inode_t& parentInode = inodes[parentInodeNumber];

    // Read directory data
    vector<dir_ent_t> dirEntries;
    size_t dirSize = parentInode.size;
    char buffer[dirSize];
    if (fs->read(parentInodeNumber, buffer, dirSize) < 0) {
        return -EINVALIDINODE;
    }

    // Extract directory entries into vector
    size_t entryCount = dirSize / sizeof(dir_ent_t);
    for (size_t j = 0; j < entryCount; j++) {
        dir_ent_t dirEntry;
        memcpy(&dirEntry, buffer + (j * sizeof(dir_ent_t)), sizeof(dir_ent_t));
        dirEntries.push_back(dirEntry);
    }

    // Create new directory entry
    dir_ent_t newDirEntry;
    newDirEntry.inum = newInodeNum;
    strncpy(newDirEntry.name, name.c_str(), sizeof(newDirEntry.name) - 1);
    newDirEntry.name[sizeof(newDirEntry.name) - 1] = '\0'; // Ensure null-termination
    dirEntries.push_back(newDirEntry);

    // Update parent directory size
    size_t oldParentSize = parentInode.size;
    parentInode.size = oldParentSize + sizeof(dir_ent_t);

    // Prepare directory data for writing
    size_t newDirSize = parentInode.size;
    size_t newEntryCount = newDirSize / sizeof(dir_ent_t);
    char newBuf[newDirSize];
    for (size_t j = 0; j < newEntryCount; j++) {
        memcpy(newBuf + (j * sizeof(dir_ent_t)), &dirEntries[j], sizeof(dir_ent_t));
    }

    // Check if we need more blocks than available
    size_t blocksNeeded = std::ceil(static_cast<double>(newDirSize) / UFS_BLOCK_SIZE);
    if (blocksNeeded > 30) {
        return -ENOTENOUGHSPACE;
    }

    // Write the directory data to disk
    int result = writeDirectoryData(fs, super, parentInode, newBuf, newDirSize, oldParentSize);
    return result;
}

LocalFileSystem::LocalFileSystem(Disk* disk) { this->disk = disk; }

void LocalFileSystem::readSuperBlock(super_t* super)
{
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(0, buffer);
    memcpy(super, buffer, sizeof(super_t));
}

void LocalFileSystem::readInodeBitmap(super_t* super,
    unsigned char* inodeBitmap)
{
    int inode_bitmap_block = super->inode_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(inode_bitmap_block, buffer);
    memcpy(inodeBitmap, buffer,
        static_cast<int>(std::ceil(super->num_inodes / 8)));
}

// Inode bitmap bits are allocated when creating new file/directories
void LocalFileSystem::writeInodeBitmap(super_t* super,
    unsigned char* inodeBitmap)
{
    int inode_bitmap_block = super->inode_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    memcpy(buffer, inodeBitmap,
        static_cast<int>(std::ceil(super->num_inodes / 8)));
    this->disk->writeBlock(inode_bitmap_block, buffer);
    cout << "wrote inode bitmap" << endl;
}

void LocalFileSystem::readDataBitmap(super_t* super,
    unsigned char* dataBitmap)
{
    int data_bitmap_block = super->data_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(data_bitmap_block, buffer);
    memcpy(dataBitmap, buffer, static_cast<int>(std::ceil(super->num_data / 8)));
}

// data bitmap bits are allocated when writing file content
void LocalFileSystem::writeDataBitmap(super_t* super,
    unsigned char* dataBitmap)
{
    int data_bitmap_block = super->data_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    memcpy(buffer, dataBitmap, static_cast<int>(std::ceil(super->num_data / 8)));
    this->disk->writeBlock(data_bitmap_block, buffer);
    cout << "wrote data bitmap" << endl;
}

void LocalFileSystem::readInodeRegion(super_t* super, inode_t* inodes)
{
    size_t inode_region_block = super->inode_region_addr;
    char buffer[UFS_BLOCK_SIZE];
    size_t inodes_per_block = UFS_BLOCK_SIZE / sizeof(inode_t);
    size_t num_inodes = super->num_inodes;
    size_t amountOfBlocks = super->inode_region_len;

    for (size_t i = 0; i < amountOfBlocks; i++) {
        this->disk->readBlock(inode_region_block + i, buffer);
        size_t inodes_to_copy = min(inodes_per_block, num_inodes - (i * inodes_per_block));
        if (inodes_to_copy <= 0) {
            break;
        }
        // Copy inodes from the buffer to the inodes array
        memcpy(&inodes[i * inodes_per_block], buffer, inodes_to_copy * sizeof(inode_t));
    }
}

void LocalFileSystem::writeInodeRegion(super_t* super, inode_t* inodes)
{
    size_t inodes_per_block = UFS_BLOCK_SIZE / sizeof(inode_t);
    size_t num_inodes = super->num_inodes;
    size_t amountOfBlocks = super->inode_region_len;
    size_t inode_region_block = super->inode_region_addr;
    char buffer[UFS_BLOCK_SIZE];

    for (size_t i = 0; i < amountOfBlocks; i++) {
        size_t inodes_to_copy = min(inodes_per_block,
            num_inodes - (i * inodes_per_block)); // how many inodes to copy from array
        if (inodes_to_copy <= 0) {
            break;
        }
        memset(buffer, 0, UFS_BLOCK_SIZE);
        memcpy(buffer, &inodes[i * inodes_per_block],
            inodes_to_copy * sizeof(inode_t)); // copy inodes from inodes array to buffer
        this->disk->writeBlock(inode_region_block + i, buffer);
    }
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
    for (size_t j = 0; j < amountOfEntiresToRead; j++) {
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

    // Check if inode number is valid
    int inodeValidation = validateInodeNumber(inodeNumber, super.num_inodes);
    if (inodeValidation != 0) {
        return inodeValidation;
    }

    // Calculation inode offset & block offset
    size_t inodes_per_block = UFS_BLOCK_SIZE / sizeof(inode_t);
    size_t block_offset = inodeNumber / inodes_per_block;
    size_t inode_block = super.inode_region_addr + block_offset;
    size_t inode_offset_in_block = inodeNumber % inodes_per_block;

    // Copy bytes from disk block to buffer & then into inode stuct
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(inode_block, buffer);
    memcpy(inode, buffer + (inode_offset_in_block * sizeof(inode_t)),
        sizeof(inode_t));

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
    // if size IS LESS than the size of object to read, then return only those
    // bytes. if size IS GREATER than the size of object to read, then return the
    // bytes in the object.

    // (1) Find the inode
    // (2) Read the raw bytes from the direct blocks
    // (3) Copy those bytes to the provided buffer
    // (4) Return how many bytes were read

    // Find the inode
    inode_t inode;
    if (this->stat(inodeNumber, &inode) != 0) {
        return -EINVALIDINODE;
    }

    size_t blocksNeeded = std::ceil(static_cast<double>(inode.size) / UFS_BLOCK_SIZE);
    size_t bytesToRead = min(static_cast<size_t>(size), static_cast<size_t>(inode.size));
    size_t bytesRead = 0;

    // Read the raw bytes from the direct blocks
    for (size_t i = 0; i < blocksNeeded; i++) {
        char blockBuffer[UFS_BLOCK_SIZE];
        this->disk->readBlock(inode.direct[i], blockBuffer);

        // Calculate where in the destination buffer this block goes
        size_t destOffset = i * UFS_BLOCK_SIZE; // Offset in destination buffer to start writing from
        size_t bytesToCopy = min(static_cast<size_t>(UFS_BLOCK_SIZE),
            static_cast<size_t>(bytesToRead - bytesRead));
        memcpy((char*)buffer + (destOffset), blockBuffer, bytesToCopy);
        bytesRead += bytesToCopy;
    }

    return bytesRead;
}

// **Create - Write - Unlink**
// resuse existing data blocks if possible
// if new file uses fewer data blocks than the old file, then free the extra
// data blocks. if new file uses more data blocks than the old file, then
// allocate new data blocks.

// Allocate both an inode and disk lock for directories.
// If can't allocate one or the other, free allocated inodes or disk blocks and
// return an error.

/**
 * Makes a file or directory.
 *
 * Makes a file (type == UFS_REGULAR_FILE) or directory (type == UFS_DIRECTORY)
 * in the parent directory specified by parentInodeNumber of name name.
 *
 * Success: return the inode number of the new file or directory
 * Failure: -EINVALIDINODE, -EINVALIDNAME, -EINVALIDTYPE, -ENOTENOUGHSPACE.
 * Failure modes: parentInodeNumber does not exist or is not a directory, or
 * name is too long. If name already exists and is of the correct type,
 * return success, but if the name already exists and is of the wrong type,
 * return an error.
 */
int LocalFileSystem::create(int parentInodeNumber, int type, string name)
{

    super_t super;
    this->readSuperBlock(&super);

    // Validate parameters first
    int validationResult = validateCreateParameters(this, &super, parentInodeNumber, type, name);
    if (validationResult != 0) {
        return validationResult; // Return error or existing inode
    }

    // START TRANSACTION
    this->disk->beginTransaction();

    // // Process Inode bit map
    // size_t inodeBitmapBytes = static_cast<int>(ceil(super.num_data / 8));
    // unsigned char* inodeBitmap = new unsigned char[inodeBitmapBytes];
    // this->readInodeBitmap(&super, inodeBitmap);

    // // find first free bit (free inode)
    // int freeInodeIndex = findFirstFreeBit(inodeBitmap, inodeBitmapBytes, super.num_inodes - 1);
    // setBit(inodeBitmap, freeInodeIndex); // set new bits

    // // write bitmap to disk
    // this->writeInodeBitmap(&super, inodeBitmap);

    int newInodeNum = allocateNewInode(this, &super, type);
    if (newInodeNum < 0) {
        // ROLLBACK
        this->disk->rollback();
        return newInodeNum;
    }

    // going to hold inodes
    vector<inode_t> inodes(super.num_inodes);
    this->readInodeRegion(&super, inodes.data());

    // create new inode (file or directory)
    if (type == UFS_REGULAR_FILE) {
        inodes[newInodeNum].size = 0;
        inodes[newInodeNum].type = UFS_REGULAR_FILE;
    } else {
        inodes[newInodeNum].size = sizeof(dir_ent_t) * 2;
        inodes[newInodeNum].type = UFS_DIRECTORY;
        // TODO: Initialize directory with . and .. entires
    }

    // inode_t& parentInode = inodes[parentInodeNumber];

    // update parent directory
    int updateResult = updateParentDirectory(this, &super, inodes, parentInodeNumber, newInodeNum, name);
    if (updateResult < 0) {
        this->disk->rollback();
        return updateResult;
    }

    // // Read directory data
    // vector<dir_ent_t> dirEntries;
    // size_t dirSize = parentInode.size;
    // char buffer[dirSize];
    // if (this->read(parentInodeNumber, buffer, dirSize) < 0) {
    //     // printError();
    //     return -1;
    // }

    // // Extract directory entries into vector
    // size_t entryCount = dirSize / sizeof(dir_ent_t);
    // for (size_t j = 0; j < entryCount; j++) {
    //     dir_ent_t dirEntry;
    //     memcpy(&dirEntry, buffer + (j * sizeof(dir_ent_t)), sizeof(dir_ent_t));
    //     dirEntries.push_back(dirEntry);
    // }

    // // create new dir entry for parent directory
    // dir_ent_t newDirEntry;
    // newDirEntry.inum = newInodeNum;
    // strncpy(newDirEntry.name, name.c_str(), sizeof(newDirEntry.name) - 1);
    // newDirEntry.name[sizeof(newDirEntry.name) - 1] = '\0'; // Ensure null-termination
    // dirEntries.push_back(newDirEntry); // add new dir entry to parent directory

    // size_t oldParentSize = parentInode.size;
    // parentInode.size = oldParentSize + sizeof(dir_ent_t);
    // cout << "\n\nSize of parent Inode after adding new dir entry: " << parentInode.size << endl;

    // // copy parent directory data to buffer to write to disk
    // size_t newDirSize = parentInode.size;
    // size_t newEntryCount = newDirSize / sizeof(dir_ent_t);
    // char newBuf[newDirSize];
    // for (size_t j = 0; j < newEntryCount; j++) {
    //     memcpy(newBuf + (j * sizeof(dir_ent_t)), &dirEntries[j], sizeof(dir_ent_t));
    // }

    // size_t blocksNeeded = std::ceil(static_cast<double>(newDirSize) / UFS_BLOCK_SIZE);
    // size_t allocatedBlocks = std::ceil(static_cast<double>(oldParentSize) / UFS_BLOCK_SIZE);
    // if (blocksNeeded > 30) {
    //     // ROLL BACK
    //     this->disk->rollback();
    //     return -ENOTENOUGHSPACE;
    // }

    // // check if new block for parent inode
    // if (blocksNeeded > allocatedBlocks) {
    //     // Process Data bit map
    //     size_t dataBitmapBytes = static_cast<int>(ceil(super.num_data / 8));
    //     unsigned char* dataBitmap = new unsigned char[dataBitmapBytes];
    //     this->readDataBitmap(&super, dataBitmap);

    //     // find first free bit (free block)
    //     int freeDataBlockIndex = findFirstFreeBit(dataBitmap, dataBitmapBytes, super.num_data - 1);
    //     cout << "\n\nindex of first 0 bit: " << freeDataBlockIndex << endl;
    //     setBit(dataBitmap, freeDataBlockIndex); // set new bits
    //     this->writeDataBitmap(&super, dataBitmap); // write bitmap to disk (TODO: put into a transaction)

    //     // allocate new block
    //     char emptyBlock[UFS_BLOCK_SIZE] = { 0 };
    //     this->disk->writeBlock(freeDataBlockIndex, emptyBlock);
    //     parentInode.direct[allocatedBlocks] = freeDataBlockIndex;

    //     delete[] dataBitmap;
    // }

    // // handle writing directories to disk
    // size_t bytesToWrite = newDirSize;
    // size_t bytesWritten = 0;
    // char bufferToWrite[UFS_BLOCK_SIZE];
    // for (size_t i = 0; i < blocksNeeded; i++) {
    //     size_t blockToWrite = parentInode.direct[i];
    //     size_t destOffset = i * UFS_BLOCK_SIZE; // Offset in destination buffer to start writing from
    //     size_t bytesToCopy = min(static_cast<size_t>(UFS_BLOCK_SIZE), static_cast<size_t>(bytesToWrite - bytesWritten));

    //     memset(bufferToWrite, 0, UFS_BLOCK_SIZE);
    //     memcpy(bufferToWrite, newBuf + destOffset, bytesToCopy);

    //     this->disk->writeBlock(blockToWrite, bufferToWrite); // TODO: add to transaction
    //     cout << "wrote directory entries to disk" << endl;
    //     bytesWritten += bytesToCopy;
    // }

    // write inode new inode and parent inode to disk
    this->writeInodeRegion(&super, inodes.data());

    // COMMIT
    this->disk->commit();

    return newInodeNum;
}

// **Write**
// Not enough storage, then write as many bytes as you can.
// Return sucess to the caller with the number of bytes written.
/**
 * Write the contents of a file.
 *
 * Writes a buffer of size to the file, replacing any content that
 * already exists.
 *
 * Success: number of bytes written
 * Failure: -EINVALIDINODE, -EINVALIDSIZE, -EINVALIDTYPE.
 * Failure modes: invalid inodeNumber, invalid size, not a regular file
 * (because you can't write to directories).
 */
int LocalFileSystem::write(int inodeNumber, const void* buffer, int size)
{
    // (1) Find the inode
    // (2) Read the raw bytes from the direct blocks
    // (3) Copy those bytes to the provided buffer
    // (4) Return how many bytes were read
    // Find the inode
    inode_t inode;
    if (this->stat(inodeNumber, &inode) != 0) {
        return -EINVALIDINODE;
    }

    size_t blocksNeeded = std::ceil(static_cast<double>(inode.size) / UFS_BLOCK_SIZE);
    size_t bytesToRead = min(static_cast<size_t>(size), static_cast<size_t>(inode.size));
    size_t bytesRead = 0;

    // Read the raw bytes from the direct blocks
    for (size_t i = 0; i < blocksNeeded; i++) {
        char blockBuffer[UFS_BLOCK_SIZE];
        this->disk->readBlock(inode.direct[i], blockBuffer);

        // Calculate where in the destination buffer this block goes
        size_t destOffset = i * UFS_BLOCK_SIZE; // Offset in destination buffer to start writing from
        size_t bytesToCopy = min(static_cast<size_t>(UFS_BLOCK_SIZE),
            static_cast<size_t>(bytesToRead - bytesRead));
        memcpy((char*)buffer + (destOffset), blockBuffer, bytesToCopy);
        bytesRead += bytesToCopy;
    }

    return 0;
}

int LocalFileSystem::unlink(int parentInodeNumber, string name) { return 0; }

