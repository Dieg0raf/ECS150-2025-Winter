#include "Disk.h"
#include "LocalFileSystem.h"
#include "ufs.h"
#include <bitset>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

void printError()
{
    cerr << "Error creating file" << endl;
}

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

// Directory entry comparison for sorting
bool compareByName(const dir_ent_t& a, const dir_ent_t& b)
{
    return std::strcmp(a.name, b.name) < 0;
}

/*
- Each write to a file logically generates five I/Os:
    - read the data bitmap to find a free block
    - write the bitmap (to reflect its new state to disk)
    - two more to read and then write the inode (which is updated with the new block's location)
    - write the actual block itself
*/
int main(int argc, char* argv[])
{
    if (argc != 4) {
        cerr << argv[0] << ": diskImageFile parentInode fileName" << endl;
        cerr << "For example:" << endl;
        cerr << "    $ " << argv[0] << " a.img 0 a.txt" << endl;
        return 1;
    }

    // Parse command line arguments
    Disk* disk = new Disk(argv[1], UFS_BLOCK_SIZE);
    LocalFileSystem* fileSystem = new LocalFileSystem(disk);
    int parentInode = stoi(argv[2]);
    string fileName = string(argv[3]);

    if (fileName.size() > 28) {
        return -ENOTENOUGHSPACE;
    }

    super_t super;
    fileSystem->readSuperBlock(&super);

    // Process Inode bit map
    size_t inodeBitmapBytes = static_cast<int>(ceil(super.num_data / 8));
    unsigned char* inodeBitmap = new unsigned char[inodeBitmapBytes];
    fileSystem->readInodeBitmap(&super, inodeBitmap);

    cout << "Inode bitmap" << endl;
    for (size_t i = 0; i < inodeBitmapBytes; i++) {
        bitset<8> bits(inodeBitmap[i]);
        cout << "(" << (i * 8) + 7 << "-" << i * 8 << ")" << bits << endl;
    }

    // find first free bit (free inode)
    int freeInodeIndex = findFirstFreeBit(inodeBitmap, inodeBitmapBytes, super.num_inodes - 1);
    cout << "\n\nindex of first 0 bit: " << freeInodeIndex << endl;
    setBit(inodeBitmap, freeInodeIndex); // set new bits
    fileSystem->writeInodeBitmap(&super, inodeBitmap); // write bitmap to disk (TODO: put into a transaction)

    // going to hold inodes
    vector<inode_t> inodes(super.num_inodes);
    fileSystem->readInodeRegion(&super, inodes.data());

    for (int i = 0; i < inodes.size(); i++) {
        cout << i << ": " << inodes[i].type << " " << inodes[i].size << endl;
    }

    // create new inode for new file
    inodes[freeInodeIndex].size = 0;
    inodes[freeInodeIndex].type = UFS_REGULAR_FILE;

    // Write new inode back to disk (TODO: put into a transaction)
    // TODO: can wait to write inode at the end - in question
    fileSystem->writeInodeRegion(&super, inodes.data());

    cout << "\n\nAfter writing inode region to disk" << endl;
    fileSystem->readInodeRegion(&super, inodes.data());
    for (int i = 0; i < inodes.size(); i++) {
        cout << i << ": " << inodes[i].type << " " << inodes[i].size << endl;
    }

    // update parent directory
    inode_t& parentInodeObj = inodes[parentInode];
    cout << "Size of parent Inode: " << parentInodeObj.size << endl;

    // Read directory data
    vector<dir_ent_t> dirEntries;
    size_t dirSize = parentInodeObj.size;
    char buffer[dirSize];
    if (fileSystem->read(parentInode, buffer, dirSize) < 0) {
        printError();
    }

    // Extract directory entries
    size_t entryCount = dirSize / sizeof(dir_ent_t);
    for (size_t j = 0; j < entryCount; j++) {
        dir_ent_t dirEntry;
        memcpy(&dirEntry, buffer + (j * sizeof(dir_ent_t)), sizeof(dir_ent_t));
        dirEntries.push_back(dirEntry);
    }

    // create new dir entry for parent directory
    dir_ent_t newDirEntry;
    newDirEntry.inum = freeInodeIndex;
    strncpy(newDirEntry.name, fileName.c_str(), sizeof(newDirEntry.name) - 1);
    newDirEntry.name[sizeof(newDirEntry.name) - 1] = '\0'; // Ensure null-termination
    dirEntries.push_back(newDirEntry); // add new dir entry to parent directory

    // print out dir entires for parent directory (see if new one was added)
    // sort(dirEntries.begin(), dirEntries.end(), compareByName);
    cout << endl;
    for (size_t i = 0; i < dirEntries.size(); i++) {
        cout << dirEntries[i].inum << "\t" << dirEntries[i].name << endl;
    }

    // update parent obj size
    size_t oldParentSize = parentInodeObj.size;
    parentInodeObj.size = oldParentSize + sizeof(dir_ent_t);
    cout << "\n\nSize of parent Inode after adding new dir entry: " << parentInodeObj.size << endl;

    // copy dir data to buf
    size_t newDirSize = parentInodeObj.size;
    size_t newEntryCount = newDirSize / sizeof(dir_ent_t);
    char newBuf[newDirSize];
    for (size_t j = 0; j < newEntryCount; j++) {
        memcpy(newBuf + (j * sizeof(dir_ent_t)), &dirEntries[j], sizeof(dir_ent_t));
    }

    size_t blocksNeeded = std::ceil(static_cast<double>(newDirSize) / UFS_BLOCK_SIZE);
    size_t allocatedBlocks = std::ceil(static_cast<double>(oldParentSize) / UFS_BLOCK_SIZE);

    cout << "Blocks Needed: " << blocksNeeded << endl;
    cout << "Allocated Needed: " << allocatedBlocks << endl;

    if (blocksNeeded > 30) {
        return -ENOTENOUGHSPACE;
    }

    // allocate new block for parent inode
    int freeDataBlockIndex = 0;
    if (blocksNeeded > allocatedBlocks) {
        // Process Data bit map
        size_t dataBitmapBytes = static_cast<int>(ceil(super.num_data / 8));
        unsigned char* dataBitmap = new unsigned char[dataBitmapBytes];
        fileSystem->readDataBitmap(&super, dataBitmap);

        cout << "Data bitmap" << endl;
        for (size_t i = 0; i < dataBitmapBytes; i++) {
            bitset<8> bits(dataBitmap[i]);
            cout << "(" << (i * 8) + 7 << "-" << i * 8 << ")" << bits << endl;
        }

        // find first free bit (free block)
        int freeIndex = findFirstFreeBit(dataBitmap, dataBitmapBytes, super.num_data - 1);
        cout << "\n\nindex of first 0 bit: " << freeIndex << endl;
        setBit(dataBitmap, freeIndex); // set new bits
        fileSystem->writeDataBitmap(&super, dataBitmap); // write bitmap to disk (TODO: put into a transaction)
        freeDataBlockIndex = freeIndex;
    }

    // allocate new block
    if (freeDataBlockIndex != 0) {
        char emptyBlock[UFS_BLOCK_SIZE] = { 0 };
        fileSystem->disk->writeBlock(freeDataBlockIndex, emptyBlock);
        parentInodeObj.direct[allocatedBlocks] = freeDataBlockIndex;
    }

    // handle writing directories to disk
    size_t bytesToWrite = newDirSize;
    size_t bytesWritten = 0;
    char bufferToWrite[UFS_BLOCK_SIZE];
    for (size_t i = 0; i < blocksNeeded; i++) {
        size_t blockToWrite = parentInodeObj.direct[i];
        size_t destOffset = i * UFS_BLOCK_SIZE; // Offset in destination buffer to start writing from
        size_t bytesToCopy = min(static_cast<size_t>(UFS_BLOCK_SIZE), static_cast<size_t>(bytesToWrite - bytesWritten));

        memset(bufferToWrite, 0, UFS_BLOCK_SIZE);
        memcpy(bufferToWrite, newBuf + destOffset, bytesToCopy);

        fileSystem->disk->writeBlock(blockToWrite, bufferToWrite); // TODO: add to transaction
        cout << "wrote directory entries to disk" << endl;
        bytesWritten += bytesToCopy;
    }

    cout << "\n\nBlocks needed: " << blocksNeeded << endl;
    for (size_t i = 0; i < DIRECT_PTRS; i++) {
        cout << i << ": " << parentInodeObj.direct[i] << endl;
    }

    // update parent inode in inode region
    // inodes[parentInode].size = parentInodeObj.size;
    cout << "\nWrite inode region to disk again!" << endl;
    fileSystem->writeInodeRegion(&super, inodes.data());

    // refetch databit map to check
    // inodeBitmap = new unsigned char[inodeBitmapBytes];
    // fileSystem->readInodeBitmap(&super, inodeBitmap);
    // cout << "Inode bitmap after writing to disk" << endl;
    // for (size_t i = 0; i < inodeBitmapBytes; i++) {
    //     bitset<8> bits(inodeBitmap[i]);
    //     cout << "(" << (i * 8) + 7 << "-" << i * 8 << ")" << bits << endl;
    // }

    delete[] inodeBitmap;

    return 0;
}

