#include "LocalFileSystem.h"
#include "ufs.h"
#include <assert.h>
#include <bitset>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// A disk image is essentially a file that contains a complete copy or replica
// of a storage device, like a hard drive or a floppy disk

using namespace std;

int findFirstFreeBit(unsigned char* bitmap, size_t bitmapBytes, size_t totalBits)
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

// Set a bit at position
void clearBit(unsigned char* bitmap, const int position)
{
    int byteIndex = position / 8;
    int bitOffset = position % 8;
    bitmap[byteIndex] &= ~(1 << bitOffset);
}

static int allocateNewInode(LocalFileSystem* const fs, super_t* super, int type)
{
    // Process Inode bitmap
    size_t inodeBitmapBytes = static_cast<int>(ceil(super->num_inodes / 8));
    unsigned char* inodeBitmap = new unsigned char[inodeBitmapBytes];
    fs->readInodeBitmap(super, inodeBitmap);

    // Find first free bit (free inode)
    int freeInodeIndex = findFirstFreeBit(inodeBitmap, inodeBitmapBytes, super->num_inodes);
    if (freeInodeIndex < 0) {
        delete[] inodeBitmap;
        return -ENOTENOUGHSPACE;
    }

    // cout << "Inode bit map BEFORE setting" << endl;
    // for (size_t i = 0; i < inodeBitmapBytes; i++) {
    //     bitset<8> bits(inodeBitmap[i]);
    //     cout << "(" << (i * 8) + 7 << "-" << i * 8 << ")" << bits << endl;
    // }
    // cout << "Free data block: " << freeInodeIndex << endl;

    setBit(inodeBitmap, freeInodeIndex); // Set bit in bitmap

    // cout << "Inode bit map AFTER setting" << endl;
    // for (size_t i = 0; i < inodeBitmapBytes; i++) {
    //     bitset<8> bits(inodeBitmap[i]);
    //     cout << "(" << (i * 8) + 7 << "-" << i * 8 << ")" << bits << endl;
    // }
    fs->writeInodeBitmap(super, inodeBitmap); // Write bitmap to disk
    delete[] inodeBitmap;

    return freeInodeIndex;
}

static int deAllocateDataBlock(LocalFileSystem* const fs, super_t* const super, int dataBlock)
{
    // Process Data bitmap
    size_t dataBitmapBytes = static_cast<int>(ceil(super->num_data / 8));
    unsigned char* dataBitmap = new unsigned char[dataBitmapBytes];
    fs->readDataBitmap(super, dataBitmap);

    int dataBlockToFree = dataBlock - super->data_region_addr;
    // cout << "Block to to free: " << dataBlockToFree << endl;

    // cout << "Data bit map BEFORE clearing" << endl;
    // for (size_t i = 0; i < dataBitmapBytes; i++) {
    //     bitset<8> bits(dataBitmap[i]);
    //     cout << "(" << (i * 8) + 7 << "-" << i * 8 << ")" << bits << endl;
    // }

    clearBit(dataBitmap, dataBlockToFree); // clear bit in bitmap

    // cout << "Data bit map AFTER clearing" << endl;
    // for (size_t i = 0; i < dataBitmapBytes; i++) {
    //     bitset<8> bits(dataBitmap[i]);
    //     cout << "(" << (i * 8) + 7 << "-" << i * 8 << ")" << bits << endl;
    // }

    fs->writeDataBitmap(super, dataBitmap); // Write bitmap to disk

    // Clear the block with zeros
    char emptyBlock[UFS_BLOCK_SIZE] = { 0 };
    fs->disk->writeBlock(dataBlock, emptyBlock);

    delete[] dataBitmap;

    return 0;
}

static int allocateDataBlock(LocalFileSystem* const fs, super_t* const super)
{
    // Process Data bitmap
    size_t dataBitmapBytes = static_cast<int>(ceil(super->num_data / 8));
    unsigned char* dataBitmap = new unsigned char[dataBitmapBytes];
    fs->readDataBitmap(super, dataBitmap);

    // Find first free bit (free block)
    int freeDataBlockIndex = findFirstFreeBit(dataBitmap, dataBitmapBytes, super->num_data);
    int actualDataBlock = freeDataBlockIndex + super->data_region_addr;
    if (freeDataBlockIndex < 0) {
        delete[] dataBitmap;
        return -ENOTENOUGHSPACE;
    }

    // cout << "Data bit map BEFORE setting" << endl;
    // for (size_t i = 0; i < dataBitmapBytes; i++) {
    //     bitset<8> bits(dataBitmap[i]);
    //     cout << "(" << (i * 8) + 7 << "-" << i * 8 << ")" << bits << endl;
    // }
    // cout << "Free data block: " << freeDataBlockIndex << endl;
    // cout << "Actual block to write: " << actualDataBlock << endl;

    setBit(dataBitmap, freeDataBlockIndex); // Set bit in bitmap

    // cout << "Data bit map AFTER setting" << endl;
    // for (size_t i = 0; i < dataBitmapBytes; i++) {
    //     bitset<8> bits(dataBitmap[i]);
    //     cout << "(" << (i * 8) + 7 << "-" << i * 8 << ")" << bits << endl;
    // }
    fs->writeDataBitmap(super, dataBitmap); // Write bitmap to disk

    // Initialize the block with zeros
    char emptyBlock[UFS_BLOCK_SIZE] = { 0 };
    fs->disk->writeBlock(actualDataBlock, emptyBlock);

    delete[] dataBitmap;
    // cout << "Return value from allocateDataBlock: " << actualDataBlock << endl;
    return actualDataBlock;
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
        return -EINVALIDTYPE;
    }

    // Check if name already exists in parent directory
    int existingInodeNum = fs->lookup(parentInodeNumber, name);
    if (existingInodeNum > 0) {
        inode_t existingInode;
        fs->stat(existingInodeNum, &existingInode);
        if (existingInode.type == type) {
            return existingInodeNum;
        }
        return -EINVALIDTYPE;
    }

    if (existingInodeNum == -EINVALIDINODE) {
        return existingInodeNum;
    }

    return 0; // All checks passed
}

static int writeData(LocalFileSystem* const fs, super_t* const super, inode_t& parentInode, char* data, size_t dataSize, size_t oldSize)
{

    size_t blocksNeeded = std::ceil(static_cast<double>(dataSize) / UFS_BLOCK_SIZE);
    size_t allocatedBlocks = std::ceil(static_cast<double>(oldSize) / UFS_BLOCK_SIZE);

    // cout << "\nBlocks needed: " << blocksNeeded << endl;
    // cout << "Blocks allocated: " << allocatedBlocks << endl;

    if (allocatedBlocks == 0) {
        for (int j = 0; j < DIRECT_PTRS; j++)
            parentInode.direct[j] = -1;
    }

    // DeAllocate blocks if needed
    if (blocksNeeded < allocatedBlocks) {
        // cout << "New File is smaller than before" << endl;
        for (size_t i = blocksNeeded; i < allocatedBlocks; i++) {
            // cout << "Deallocating data block" << endl;
            int isFreeBlock = deAllocateDataBlock(fs, super, parentInode.direct[i]);
            if (isFreeBlock != 0) {
                return -1; // Error code from allocateDataBlock
            }
            parentInode.direct[i] = -1;
        }
    }

    // Allocate additional blocks if needed
    if (blocksNeeded > allocatedBlocks) {
        // cout << "New file is bigger than before" << endl;
        int actualBlocksAllocated = allocatedBlocks; // keep track of allocated blocks (needed if allocating fails)
        for (size_t i = allocatedBlocks; i < blocksNeeded; i++) {
            // cout << "Allocated data block" << endl;
            int newBlockNum = allocateDataBlock(fs, super);
            if (newBlockNum < 0) {
                blocksNeeded = actualBlocksAllocated; // No more disk space
                break;
            }
            parentInode.direct[i] = newBlockNum;
            actualBlocksAllocated++;
        }

        // 0 blocks were allocated (no space for more)
        if (actualBlocksAllocated == 0) {
            return -ENOTENOUGHSPACE;
        }
    }

    if (blocksNeeded > 0 && parentInode.direct[0] < 0) {
        return -ENOTENOUGHSPACE;
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

    return bytesWritten;
}

static int updateParentDirectory(LocalFileSystem* const fs, super_t* const super, vector<inode_t>& inodes, int parentInodeNumber, int newInodeNum, string name)
{
    inode_t& parentInode = inodes[parentInodeNumber];

    // Read directory data
    vector<dir_ent_t> dirEntries;
    size_t dirSize = parentInode.size;
    char* dirBuffer = new char[dirSize];
    memset(dirBuffer, 0, dirSize);
    if (fs->read(parentInodeNumber, dirBuffer, dirSize) < 0) {
        delete[] dirBuffer;
        return -EINVALIDINODE;
    }

    // Extract directory entries into vector
    size_t entryCount = dirSize / sizeof(dir_ent_t);
    for (size_t j = 0; j < entryCount; j++) {
        dir_ent_t dirEntry;
        memcpy(&dirEntry, dirBuffer + (j * sizeof(dir_ent_t)), sizeof(dir_ent_t));
        dirEntries.push_back(dirEntry);
    }

    delete[] dirBuffer;

    // Create new directory entry
    dir_ent_t newDirEntry;
    newDirEntry.inum = newInodeNum;
    strncpy(newDirEntry.name, name.c_str(), sizeof(newDirEntry.name) - 1);
    newDirEntry.name[sizeof(newDirEntry.name) - 1] = '\0'; // ensure null termination
    dirEntries.push_back(newDirEntry);

    // Update parent directory size
    size_t oldParentDirSize = parentInode.size;
    parentInode.size = oldParentDirSize + sizeof(dir_ent_t);

    // Prepare directory data for writing to disk
    size_t newDirSize = parentInode.size;
    size_t newEntryCount = newDirSize / sizeof(dir_ent_t);
    char* newDirBuffer = new char[newDirSize];
    memset(newDirBuffer, 0, newDirSize);
    for (size_t j = 0; j < newEntryCount; j++) {
        memcpy(newDirBuffer + (j * sizeof(dir_ent_t)), &dirEntries[j], sizeof(dir_ent_t));
    }

    // Check if we need more blocks than available
    size_t blocksNeeded = ceil(static_cast<double>(newDirSize) / UFS_BLOCK_SIZE);
    if (blocksNeeded > DIRECT_PTRS) {
        delete[] newDirBuffer;
        return -ENOTENOUGHSPACE;
    }

    // Write the directory data to disk
    int bytesWritten = writeData(fs, super, parentInode, newDirBuffer, newDirSize, oldParentDirSize);
    if (bytesWritten < 0) {
        delete[] newDirBuffer;
        return bytesWritten;
    }

    delete[] newDirBuffer;
    return bytesWritten;
}

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

void LocalFileSystem::readInodeBitmap(super_t* super,
    unsigned char* inodeBitmap)
{
    int inode_bitmap_block = super->inode_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(inode_bitmap_block, buffer);
    memcpy(inodeBitmap, buffer, static_cast<int>(std::ceil(super->num_inodes / 8)));
    // cout << "finished reading inode bit map" << endl;
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
    // cout << "wrote inode bitmap" << endl;
}

void LocalFileSystem::readDataBitmap(super_t* super,
    unsigned char* dataBitmap)
{
    int data_bitmap_block = super->data_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(data_bitmap_block, buffer);
    memcpy(dataBitmap, buffer, static_cast<int>(std::ceil(super->num_data / 8)));
    // cout << "finished reading data bit map" << endl;
}

// data bitmap bits are allocated when writing file content
void LocalFileSystem::writeDataBitmap(super_t* super, unsigned char* dataBitmap)
{
    int data_bitmap_block = super->data_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    memcpy(buffer, dataBitmap, static_cast<int>(std::ceil(super->num_data / 8)));
    this->disk->writeBlock(data_bitmap_block, buffer);
    // cout << "wrote data bitmap" << endl;
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
        size_t inodes_to_copy = min(inodes_per_block, num_inodes - (i * inodes_per_block)); // how many inodes to copy from array
        if (inodes_to_copy <= 0) {
            break;
        }
        memset(buffer, 0, UFS_BLOCK_SIZE);
        memcpy(buffer, &inodes[i * inodes_per_block], inodes_to_copy * sizeof(inode_t)); // copy inodes from inodes array to buffer
        this->disk->writeBlock(inode_region_block + i, buffer);
    }

    // cout << "finished writing to inode region" << endl;
    // cout << "inodes up to date" << endl;
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

    // cout << "parent inode type: " << parentInode.type << endl;
    // cout << "parent inode size: " << parentInode.size << endl;
    size_t dirSize = parentInode.size;
    // cout << "size of two directories: " << sizeof(dir_ent_t) * 2 << endl;
    if (parentInode.type != UFS_DIRECTORY || dirSize < (sizeof(dir_ent_t) * 2)) {
        return -EINVALIDINODE;
    }

    // cout << " getting passing first check" << endl;

    // Read in raw data
    char* buffer = new char[dirSize];
    if (this->read(parentInodeNumber, buffer, dirSize) != static_cast<int>(dirSize)) {
        // cout << "failing on read" << endl;
        delete[] buffer;
        return -EINVALIDINODE;
    }

    // Iterate through directory entires
    size_t amountOfEntiresToRead = dirSize / sizeof(dir_ent_t);
    for (size_t j = 0; j < amountOfEntiresToRead; j++) {
        dir_ent_t dirEntry;
        memcpy(&dirEntry, buffer + (j * sizeof(dir_ent_t)), sizeof(dir_ent_t));
        if (strcmp(dirEntry.name, name.c_str()) == 0) {
            delete[] buffer;
            return dirEntry.inum;
        }
    }

    // cout << "not found" << endl;

    delete[] buffer;
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
    // if size IS LESS than the size of object to read, then return only those
    // bytes. if size IS GREATER than the size of object to read, then return the
    // bytes in the object.

    // (1) Find the inode
    // (2) Read the raw bytes from the direct blocks
    // (3) Copy those bytes to the provided buffer
    // (4) Return how many bytes were read

    if (size < 0) {
        return -EINVALIDSIZE;
    }

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
        size_t bytesToCopy = min(static_cast<size_t>(UFS_BLOCK_SIZE), static_cast<size_t>(bytesToRead - bytesRead));
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

    // START TRANSACTION
    this->disk->beginTransaction();

    super_t super;
    this->readSuperBlock(&super);

    // Validate parameters first
    int validationResult = validateCreateParameters(this, &super, parentInodeNumber, type, name);
    if (validationResult != 0) {
        return validationResult;
    }

    int newInodeNum = allocateNewInode(this, &super, type);
    if (newInodeNum < 0) {
        // ROLLBACK
        this->disk->rollback();
        return newInodeNum;
    }

    // Read in inodes from disk
    vector<inode_t> inodes(super.num_inodes);
    this->readInodeRegion(&super, inodes.data());

    // create new inode (file or directory)
    if (type == UFS_REGULAR_FILE) {
        inodes[newInodeNum].size = 0;
        inodes[newInodeNum].type = UFS_REGULAR_FILE;
    } else {
        size_t newDirSize = sizeof(dir_ent_t) * 2;
        inodes[newInodeNum].size = newDirSize;
        inodes[newInodeNum].type = UFS_DIRECTORY;

        // Initialize directory with . and .. entries
        dir_ent_t* entries = new dir_ent_t[2];
        entries[0].inum = newInodeNum;
        strcpy(entries[0].name, ".");
        entries[1].inum = parentInodeNumber;
        strcpy(entries[1].name, "..");

        // allocate data block for new directory
        int bytesWritten = writeData(this, &super, inodes[newInodeNum], (char*)entries, newDirSize, 0);
        delete[] entries;
        if (bytesWritten < 0) {
            this->disk->rollback();
            return bytesWritten;
        }
    }

    // update parent directory (size and data)
    int bytesWritten = updateParentDirectory(this, &super, inodes, parentInodeNumber, newInodeNum, name);
    if (bytesWritten < 0) {
        this->disk->rollback();
        return bytesWritten;
    }

    // COMMIT
    this->writeInodeRegion(&super, inodes.data());
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
    // validate size
    if (size < 0 || size >= DIRECT_PTRS * UFS_BLOCK_SIZE) {
        return -EINVALIDSIZE;
    }

    // BEGIN TRANSACTION
    this->disk->beginTransaction();

    // find inode
    inode_t inode;
    if (this->stat(inodeNumber, &inode) != 0) {
        this->disk->rollback();
        return -EINVALIDINODE;
    }

    if (inode.type != UFS_REGULAR_FILE) {
        this->disk->rollback();
        return -EINVALIDTYPE;
    }

    super_t super;
    this->readSuperBlock(&super);

    // write buffer data to blocks
    size_t oldSize = inode.size;
    int bytesWritten = writeData(this, &super, inode, (char*)buffer, size, oldSize);
    if (bytesWritten < 0) {
        this->disk->rollback();
        return bytesWritten;
    }

    // cout << "Bytes written from write data: " << bytesWritten << endl;

    // update inode in disk (read and write inode region)
    inode.size = bytesWritten;
    vector<inode_t> inodes(super.num_inodes);
    this->readInodeRegion(&super, inodes.data());
    inodes[inodeNumber] = inode;
    this->writeInodeRegion(&super, inodes.data());

    // COMMIT TRANSACTION
    this->disk->commit();
    return bytesWritten;
}

