#include "LocalFileSystem.h"
#include "ufs.h"
#include <assert.h>
#include <bitset>
#include <climits>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

static int findFirstFreeBit(unsigned char* bitmap, size_t bitmapBytes, size_t totalBits)
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
static void setBit(unsigned char* bitmap, const int position)
{
    int byteIndex = position / 8;
    int bitOffset = position % 8;
    bitmap[byteIndex] |= (1 << bitOffset);
}

// Set a bit at position
static void clearBit(unsigned char* bitmap, const int position)
{
    int byteIndex = position / 8;
    int bitOffset = position % 8;
    bitmap[byteIndex] &= ~(1 << bitOffset);
}

static int allocateInode(LocalFileSystem* const fs, super_t* super)
{
    // Process Inode bitmap
    size_t inodeBitmapBytes = static_cast<int>(ceil(super->num_inodes / 8.0));
    unsigned char* inodeBitmap = new unsigned char[inodeBitmapBytes];
    fs->readInodeBitmap(super, inodeBitmap);
    // Find first free bit (free inode)
    int freeInodeIndex = findFirstFreeBit(inodeBitmap, inodeBitmapBytes, super->num_inodes);
    if (freeInodeIndex < 0) {
        delete[] inodeBitmap;
        return -ENOTENOUGHSPACE;
    }
    setBit(inodeBitmap, freeInodeIndex); // Set bit in bitmap
    fs->writeInodeBitmap(super, inodeBitmap); // Write bitmap to disk
    delete[] inodeBitmap;
    return freeInodeIndex;
}

static int deallocateInode(LocalFileSystem* const fs, super_t* super, int inodeToFree)
{
    // Process Inode bitmap
    size_t inodeBitmapBytes = static_cast<int>(ceil(super->num_inodes / 8.0));
    unsigned char* inodeBitmap = new unsigned char[inodeBitmapBytes];
    fs->readInodeBitmap(super, inodeBitmap);
    clearBit(inodeBitmap, inodeToFree); // Set bit in bitmap
    fs->writeInodeBitmap(super, inodeBitmap); // Write bitmap to disk
    delete[] inodeBitmap;
    return 0;
}

static int deallocateDataBlock(LocalFileSystem* const fs, super_t* const super, int dataBlock)
{
    // Process Data bitmap
    size_t dataBitmapBytes = static_cast<int>(ceil(super->num_data / 8.0));
    unsigned char* dataBitmap = new unsigned char[dataBitmapBytes];
    fs->readDataBitmap(super, dataBitmap);
    int dataBlockToFree = dataBlock - super->data_region_addr;

    // Validate dataBlockToFree is within range
    if (dataBlockToFree < 0 || dataBlockToFree >= super->num_data) {
        delete[] dataBitmap;
        return -1;
    }
    clearBit(dataBitmap, dataBlockToFree); // clear bit in bitmap
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
    size_t dataBitmapBytes = static_cast<int>(ceil(super->num_data / 8.0));
    unsigned char* dataBitmap = new unsigned char[dataBitmapBytes];
    fs->readDataBitmap(super, dataBitmap);

    // Find first free bit (free block)
    int freeDataBlockIndex = findFirstFreeBit(dataBitmap, dataBitmapBytes, super->num_data);
    int actualDataBlock = freeDataBlockIndex + super->data_region_addr;
    if (freeDataBlockIndex < 0) {
        delete[] dataBitmap;
        return -ENOTENOUGHSPACE;
    }
    setBit(dataBitmap, freeDataBlockIndex); // Set bit in bitmap
    fs->writeDataBitmap(super, dataBitmap); // Write bitmap to disk
    char emptyBlock[UFS_BLOCK_SIZE] = { 0 }; // Initialize the block with zeros
    fs->disk->writeBlock(actualDataBlock, emptyBlock);
    delete[] dataBitmap;
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
    return 0;
}

static int writeData(LocalFileSystem* const fs, super_t* const super, inode_t& inode, char* data, size_t dataSize, size_t oldSize)
{
    size_t blocksNeeded = std::ceil(static_cast<double>(dataSize) / UFS_BLOCK_SIZE);
    size_t allocatedBlocks = std::ceil(static_cast<double>(oldSize) / UFS_BLOCK_SIZE);

    if (allocatedBlocks == 0) {
        for (int j = 0; j < DIRECT_PTRS; j++) {
            inode.direct[j] = UINT_MAX;
        }
    }

    // DeAllocate blocks if needed
    if (blocksNeeded < allocatedBlocks) {
        for (size_t i = blocksNeeded; i < allocatedBlocks; i++) {
            int isFreeBlock = deallocateDataBlock(fs, super, inode.direct[i]);
            if (isFreeBlock != 0) {
                return -1;
            }
            inode.direct[i] = UINT_MAX;
        }

        if (blocksNeeded == 0) {
            return 0;
        }
    }

    // Allocate additional blocks if needed
    if (blocksNeeded > allocatedBlocks) {
        int actualBlocksAllocated = allocatedBlocks; // keep track of allocated blocks (needed if allocating fails)
        for (size_t i = allocatedBlocks; i < blocksNeeded; i++) {
            int newBlockNum = allocateDataBlock(fs, super);
            if (newBlockNum < 0) {
                blocksNeeded = actualBlocksAllocated; // No more disk space
                break;
            }
            inode.direct[i] = newBlockNum;
            actualBlocksAllocated++;
        }

        // 0 blocks were allocated (no space for more)
        if (actualBlocksAllocated == 0) {
            return -ENOTENOUGHSPACE;
        }
    }

    if (blocksNeeded > 0 && inode.direct[0] < 0) {
        return -ENOTENOUGHSPACE;
    }

    // Write data to blocks
    size_t bytesWritten = 0;
    char bufferToWrite[UFS_BLOCK_SIZE];

    for (size_t i = 0; i < blocksNeeded; i++) {
        size_t blockToWrite = inode.direct[i];
        size_t destOffset = i * UFS_BLOCK_SIZE;
        size_t bytesToCopy = min(static_cast<size_t>(UFS_BLOCK_SIZE), static_cast<size_t>(dataSize - bytesWritten));
        memset(bufferToWrite, 0, UFS_BLOCK_SIZE);
        memcpy(bufferToWrite, data + destOffset, bytesToCopy);
        fs->disk->writeBlock(blockToWrite, bufferToWrite);
        bytesWritten += bytesToCopy;
    }

    return bytesWritten;
}

static vector<dir_ent_t> readDirectoryEntries(LocalFileSystem* const fs, const vector<inode_t>& inodes, int dirInodeNumber)
{
    // create inode/dir_ent stuctures
    vector<dir_ent_t> dirEntries;
    inode_t dirInode = inodes.at(dirInodeNumber);
    if (dirInode.type != UFS_DIRECTORY) {
        return dirEntries; // Return empty vector if not directory
    }
    // Read directory data
    size_t dirSize = dirInode.size;
    char* dirBuffer = new char[dirSize];
    memset(dirBuffer, 0, dirSize);
    if (fs->read(dirInodeNumber, dirBuffer, dirSize) < 0) {
        delete[] dirBuffer;
        return dirEntries; // Return empty vector on read error
    }
    // Extract directory entries into vector
    size_t entryCount = dirSize / sizeof(dir_ent_t);
    for (size_t j = 0; j < entryCount; j++) {
        dir_ent_t dirEntry;
        memcpy(&dirEntry, dirBuffer + (j * sizeof(dir_ent_t)), sizeof(dir_ent_t));
        dirEntries.push_back(dirEntry);
    }
    delete[] dirBuffer;
    return dirEntries;
}

static int writeDirectoryEntries(LocalFileSystem* const fs, super_t* const super, vector<inode_t>& inodes, int dirInodeNumber, const vector<dir_ent_t>& dirEntries)
{
    inode_t& dirInode = inodes.at(dirInodeNumber);
    if (dirInode.type != UFS_DIRECTORY) {
        return -EINVALIDTYPE;
    }
    // Calculate old and new sizes
    size_t oldDirSize = dirInode.size;
    size_t newDirSize = dirEntries.size() * sizeof(dir_ent_t);

    // Prepare directory data for writing to disk
    char* newDirBuffer = new char[newDirSize];
    memset(newDirBuffer, 0, newDirSize);

    // Copy entries into buffer
    for (size_t j = 0; j < dirEntries.size(); j++) {
        memcpy(newDirBuffer + (j * sizeof(dir_ent_t)), &dirEntries.at(j), sizeof(dir_ent_t));
    }
    dirInode.size = newDirSize;

    // Write the directory data to disk
    int bytesWritten = writeData(fs, super, dirInode, newDirBuffer, newDirSize, oldDirSize);
    delete[] newDirBuffer;
    return bytesWritten;
}

static int addDirectoryEntry(LocalFileSystem* const fs, super_t* const super, vector<inode_t>& inodes, int parentInodeNumber, int newInodeNum, string name)
{
    vector<dir_ent_t> dirEntries = readDirectoryEntries(fs, inodes, parentInodeNumber);
    dir_ent_t newDirEntry; // Create new directory entry
    newDirEntry.inum = newInodeNum;
    strncpy(newDirEntry.name, name.c_str(), sizeof(newDirEntry.name) - 1);
    newDirEntry.name[sizeof(newDirEntry.name) - 1] = '\0';
    dirEntries.push_back(newDirEntry); // Add new entry

    // Write updated entries back
    return writeDirectoryEntries(fs, super, inodes, parentInodeNumber, dirEntries);
}

static int removeDirectoryEntry(LocalFileSystem* const fs, super_t* const super, vector<inode_t>& inodes, int parentInodeNumber, string name)
{
    vector<dir_ent_t> dirEntries = readDirectoryEntries(fs, inodes, parentInodeNumber);
    // remove entry
    for (int i = 0; i < static_cast<int>(dirEntries.size()); i++) {
        if (strcmp(dirEntries.at(i).name, name.c_str()) == 0) {
            dirEntries.erase(dirEntries.begin() + i);
            break;
        }
    }
    return writeDirectoryEntries(fs, super, inodes, parentInodeNumber, dirEntries);
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

void LocalFileSystem::readInodeBitmap(super_t* super, unsigned char* inodeBitmap)
{
    int inode_bitmap_block = super->inode_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(inode_bitmap_block, buffer);
    memcpy(inodeBitmap, buffer, static_cast<int>(std::ceil(super->num_inodes / 8.0)));
}

// Inode bitmap bits are allocated when creating new file/directories
void LocalFileSystem::writeInodeBitmap(super_t* super, unsigned char* inodeBitmap)
{
    int inode_bitmap_block = super->inode_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    memcpy(buffer, inodeBitmap, static_cast<int>(std::ceil(super->num_inodes / 8.0)));
    this->disk->writeBlock(inode_bitmap_block, buffer);
}

void LocalFileSystem::readDataBitmap(super_t* super, unsigned char* dataBitmap)
{
    int data_bitmap_block = super->data_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    this->disk->readBlock(data_bitmap_block, buffer);
    memcpy(dataBitmap, buffer, static_cast<int>(std::ceil(super->num_data / 8.0)));
}

// data bitmap bits are allocated when writing file content
void LocalFileSystem::writeDataBitmap(super_t* super, unsigned char* dataBitmap)
{
    int data_bitmap_block = super->data_bitmap_addr;
    char buffer[UFS_BLOCK_SIZE];
    memcpy(buffer, dataBitmap, static_cast<int>(std::ceil(super->num_data / 8.0)));
    this->disk->writeBlock(data_bitmap_block, buffer);
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
}

int LocalFileSystem::lookup(int parentInodeNumber, string name)
{
    inode_t parentInode;
    if (this->stat(parentInodeNumber, &parentInode) != 0) {
        return -EINVALIDINODE;
    }

    size_t dirSize = parentInode.size;
    if (parentInode.type != UFS_DIRECTORY || dirSize < (sizeof(dir_ent_t) * 2)) {
        return -EINVALIDINODE;
    }

    // Read in raw data
    char* buffer = new char[dirSize];
    if (this->read(parentInodeNumber, buffer, dirSize) != static_cast<int>(dirSize)) {
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

    delete[] buffer;
    return -ENOTFOUND;
}

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

int LocalFileSystem::read(int inodeNumber, void* buffer, int size)
{
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

int LocalFileSystem::create(int parentInodeNumber, int type, string name)
{
    // START TRANSACTION
    this->disk->beginTransaction();

    super_t super;
    this->readSuperBlock(&super);

    // Validate parameters first
    int validationResult = validateCreateParameters(this, &super, parentInodeNumber, type, name);
    if (validationResult != 0) {
        this->disk->rollback();
        return validationResult;
    }

    int newInodeNum = allocateInode(this, &super);
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
        for (int i = 0; i < DIRECT_PTRS; i++) {
            inodes[newInodeNum].direct[i] = UINT_MAX;
        }
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
    int bytesWritten = addDirectoryEntry(this, &super, inodes, parentInodeNumber, newInodeNum, name);
    if (bytesWritten < 0) {
        this->disk->rollback();
        return bytesWritten;
    }

    // COMMIT
    this->writeInodeRegion(&super, inodes.data());
    this->disk->commit();

    return newInodeNum;
}

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

    // update inode in disk (read and write inode region)
    inode.size = bytesWritten;
    vector<inode_t> inodes(super.num_inodes);
    this->readInodeRegion(&super, inodes.data());
    inodes.at(inodeNumber) = inode;
    this->writeInodeRegion(&super, inodes.data());

    // COMMIT TRANSACTION
    this->disk->commit();

    return bytesWritten;
}

int LocalFileSystem::unlink(int parentInodeNumber, std::string name)
{
    // Check for invalid names (. and ..)
    if (strcmp(name.c_str(), ".") == 0 || strcmp(name.c_str(), "..") == 0) {
        return -EINVALIDNAME;
    }

    // Begin transaction
    this->disk->beginTransaction();

    // Check if entry exists in parent directory
    int inodeToDelete = this->lookup(parentInodeNumber, name);
    if (inodeToDelete == -EINVALIDINODE) {
        this->disk->rollback();
        return -EINVALIDINODE;
    } else if (inodeToDelete == -ENOTFOUND) {
        this->disk->commit();
        return -ENOTFOUND;
    }

    super_t super;
    this->readSuperBlock(&super);

    // Read inode region
    vector<inode_t> inodes(super.num_inodes);
    this->readInodeRegion(&super, inodes.data());

    // Handle directory deletion
    int ret;
    if (inodes.at(inodeToDelete).type == UFS_DIRECTORY) {
        // Check if directory is empty (except for . and ..)
        if (inodes.at(inodeToDelete).size > static_cast<int>(sizeof(dir_ent_t) * 2)) {
            this->disk->rollback();
            return -EDIRNOTEMPTY;
        }

        // Remove . and .. entries
        if ((ret = removeDirectoryEntry(this, &super, inodes, inodeToDelete, ".")) < 0) {
            this->disk->rollback();
            return ret;
        }

        if ((ret = removeDirectoryEntry(this, &super, inodes, inodeToDelete, "..")) < 0) {
            this->disk->rollback();
            return ret;
        }
    }

    // Remove the entry from the parent directory
    if ((ret = removeDirectoryEntry(this, &super, inodes, parentInodeNumber, name)) < 0) {
        this->disk->rollback();
        return ret;
    }

    // Deallocate data blocks of the inode being deleted
    if (inodes.at(inodeToDelete).size > 0) {
        for (int i = 0; i < DIRECT_PTRS; i++) {
            if (inodes.at(inodeToDelete).direct[i] != UINT_MAX) {
                if (deallocateDataBlock(this, &super, inodes.at(inodeToDelete).direct[i]) != 0) {
                    this->disk->rollback();
                    return -EUNLINKNOTALLOWED;
                }
                inodes.at(inodeToDelete).direct[i] = UINT_MAX;
            }
        }
    }

    // Deallocate the inode bitmap entry
    if (deallocateInode(this, &super, inodeToDelete) != 0) {
        this->disk->rollback();
        return -EUNLINKNOTALLOWED;
    }

    // Write the updated inodes to disk
    this->writeInodeRegion(&super, inodes.data());

    // Commit the transaction
    this->disk->commit();
    return 0;
}

