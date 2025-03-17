#include <algorithm>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <vector>

#include "ClientError.h"
#include "DistributedFileSystemService.h"
#include "WwwFormEncodedDict.h"
#include "ufs.h"

using namespace std;

enum FileSystemResult {
    SUCCESS = 0,
    NOT_FOUND = -1,
    BAD_REQUEST = -2,
    INTERNAL_ERROR = -4
};

// Helper functions
bool compareByName(const dir_ent_t& a, const dir_ent_t& b);
int validatePathComponents(const vector<string>& pathComponents, size_t& rootInode);
int validateName(string fileName);
void sendResponse(HTTPResponse* response, int rc, const string& successMessage);

// Directory operations
vector<dir_ent_t> getDirectoryEntries(LocalFileSystem* const fileSystem, const inode_t* inode, size_t inodeNum);
int formatDirectoryEntries(LocalFileSystem* const fs, const vector<dir_ent_t>& entries, std::stringstream& stringStream);

// Content handling
int handleFileContent(LocalFileSystem* const fs, vector<string>& pathComponents, stringstream& output, const size_t targetInodeSize, const size_t targetInodeNumber);
int handleDirectoryContent(LocalFileSystem* const fs, const inode_t& targetInode, const size_t targetInodeNumber, stringstream& output);

// Main filesystem operations
int navigateToDirectory(LocalFileSystem* const fs, const vector<string>& pathComponents, size_t& targetInode);
int getPathContents(LocalFileSystem* const fs, vector<string>& pathComponents, stringstream& output);
int ensurePathExists(LocalFileSystem* const fs, const vector<string>& pathComponents, size_t& targetInode);
int createOrUpdateFile(LocalFileSystem* const fs, const vector<string>& pathComponents, const string& fileContent);
int deleteEntry(LocalFileSystem* const fs, vector<string>& pathComponents);

DistributedFileSystemService::DistributedFileSystemService(string diskFile)
    : HttpService("/ds3/")
{
    this->fileSystem = new LocalFileSystem(new Disk(diskFile, UFS_BLOCK_SIZE));
}

void DistributedFileSystemService::get(HTTPRequest* request, HTTPResponse* response)
{
    vector<string> pathComponents = request->getPathComponents();
    stringstream stringStream;
    int rc = getPathContents(this->fileSystem, pathComponents, stringStream);
    sendResponse(response, rc, stringStream.str());
}

void DistributedFileSystemService::put(HTTPRequest* request, HTTPResponse* response)
{
    vector<string> pathComponents = request->getPathComponents();
    string fileContent = request->getBody();
    int rc = createOrUpdateFile(this->fileSystem, pathComponents, fileContent);
    sendResponse(response, rc, "File Successfully updated\n");
}

void DistributedFileSystemService::del(HTTPRequest* request, HTTPResponse* response)
{
    vector<string> pathComponents = request->getPathComponents();
    int rc = deleteEntry(this->fileSystem, pathComponents);
    sendResponse(response, rc, "Entry has been deleted\n");
}

bool compareByName(const dir_ent_t& a, const dir_ent_t& b)
{
    return std::strcmp(a.name, b.name) < 0;
}

void sendResponse(HTTPResponse* response, int rc, const string& successMessage)
{
    if (rc == SUCCESS) {
        response->setStatus(200);
        response->setBody(successMessage);
    } else if (rc == NOT_FOUND) {
        response->setStatus(404);
        response->setBody("Not Found");
        return;
    } else if (rc == BAD_REQUEST) {
        response->setStatus(400);
        response->setBody("Bad Request");
        return;
    } else {
        response->setStatus(500);
        response->setBody("Internal Server Error");
    }
}

int validatePathComponents(const vector<string>& pathComponents, size_t& rootInode)
{
    if (pathComponents.empty()) {
        return BAD_REQUEST;
    }
    if (pathComponents[0] != "ds3") {
        return BAD_REQUEST;
    }
    if (pathComponents.size() == 1) {
        rootInode = 0;
        return SUCCESS;
    }
    return SUCCESS;
}

vector<dir_ent_t> getDirectoryEntries(LocalFileSystem* const fs, const inode_t* inode, size_t inodeNum)
{
    // Read directory data
    vector<dir_ent_t> dirEntries;
    size_t dirSize = inode->size;
    unique_ptr<char[]> buffer(new char[dirSize]);
    if (fs->read(inodeNum, buffer.get(), dirSize) == 0) {
        return dirEntries;
    }

    // Extract directory entries
    size_t entryCount = dirSize / sizeof(dir_ent_t);
    for (size_t j = 0; j < entryCount; j++) {
        dir_ent_t dirEntry;
        memcpy(&dirEntry, buffer.get() + (j * sizeof(dir_ent_t)), sizeof(dir_ent_t));
        dirEntries.push_back(dirEntry);
    }

    // Sort entries by name
    sort(dirEntries.begin(), dirEntries.end(), compareByName);

    return dirEntries;
}

int formatDirectoryEntries(LocalFileSystem* const fs, const vector<dir_ent_t>& entries, stringstream& stringStream)
{
    if (entries.empty()) {
        return SUCCESS;
    }
    for (size_t i = 0; i < entries.size(); i++) {
        if (strcmp(entries.at(i).name, ".") == 0 || strcmp(entries.at(i).name, "..") == 0) { // skip .. and . entries
            continue;
        }
        inode_t inode;
        int rc = fs->stat(entries.at(i).inum, &inode);
        if (rc < 0) {
            return rc;
        }
        if (inode.type == UFS_DIRECTORY) {
            stringStream << entries[i].name << "/" << endl;
            continue;
        }
        stringStream << entries[i].name << endl;
    }
    return SUCCESS;
}

int handleFileContent(LocalFileSystem* const fs, vector<string>& pathComponents, stringstream& output, const size_t targetInodeSize, const size_t targetInodeNumber)
{
    // reads file contents from disk
    unique_ptr<char[]> buffer(new char[targetInodeSize]);
    int bytesRead = fs->read(targetInodeNumber, buffer.get(), targetInodeSize);
    if (bytesRead < 0) {
        return INTERNAL_ERROR;
    }
    output.write(buffer.get(), bytesRead); // fills up output string stream
    return SUCCESS;
}

int handleDirectoryContent(LocalFileSystem* const fs, const inode_t& targetInode, const size_t targetInodeNumber, stringstream& output)
{
    vector<dir_ent_t> entries = getDirectoryEntries(fs, &targetInode, targetInodeNumber);
    int rc = formatDirectoryEntries(fs, entries, output); // fills up output string stream
    if (rc < 0) {
        return rc;
    }
    return SUCCESS;
}

int navigateToDirectory(LocalFileSystem* const fs, const vector<string>& pathComponents, size_t& targetInode)
{
    // Validate input (paths)
    int validationResult = validatePathComponents(pathComponents, targetInode);
    if (validationResult != SUCCESS) {
        return validationResult;
    }

    // Navigate through path (start after ds3)
    size_t currentInode = 0;
    for (size_t i = 1; i < pathComponents.size(); i++) {
        int nextInode = fs->lookup(currentInode, pathComponents[i]);
        if (nextInode == -EINVALIDINODE || nextInode == -ENOTFOUND) {
            return NOT_FOUND;
        }
        currentInode = nextInode;
    }
    targetInode = currentInode;

    return SUCCESS;
}

int validateName(string fileName)
{
    if (fileName.empty() || fileName.length() >= DIR_ENT_NAME_SIZE || fileName.find('/') != string::npos) {
        return BAD_REQUEST;
    }
    return SUCCESS;
}

int ensurePathExists(LocalFileSystem* const fs, const vector<string>& pathComponents, size_t& targetInode)
{
    // Validate path first
    int validationResult = validatePathComponents(pathComponents, targetInode);
    if (validationResult != SUCCESS) {
        return validationResult;
    }

    // Navigate through path (start after ds3)
    size_t currentInode = 0;
    bool restOfPathExists = true;
    ssize_t nextInode = 0;
    for (size_t i = 1; i < pathComponents.size(); i++) {
        const string& component = pathComponents.at(i);
        if (validateName(component) != SUCCESS) {
            return BAD_REQUEST;
        }

        // look up next inode
        if (restOfPathExists) {
            nextInode = fs->lookup(currentInode, component);
        }

        // create next inode (file or directory)
        if (nextInode < 0) {
            restOfPathExists = false;
            bool isLastComponent = (i == pathComponents.size() - 1);
            int newInodeType = isLastComponent ? UFS_REGULAR_FILE : UFS_DIRECTORY;
            int newInodeNum = fs->create(currentInode, newInodeType, component);
            if (newInodeNum < 0) {
                return BAD_REQUEST;
            }
            currentInode = newInodeNum;
            continue;
        }
        currentInode = nextInode;
    }
    targetInode = currentInode;

    return SUCCESS;
}

int deleteEntry(LocalFileSystem* const fs, vector<string>& pathComponents)
{
    if (pathComponents.size() > 1) {
        string entryNameToDelete = pathComponents.back();
        pathComponents.pop_back();

        // validate name to delete
        int rc = validateName(entryNameToDelete);
        if (rc != SUCCESS) {
            return rc;
        }

        // grab inode number of parent directory
        size_t parentDirInodeNum;
        rc = navigateToDirectory(fs, pathComponents, parentDirInodeNum);
        if (rc != SUCCESS) {
            return rc;
        }

        // delete entry
        rc = fs->unlink(parentDirInodeNum, entryNameToDelete);
        if (rc == -ENOTFOUND) {
            return NOT_FOUND;
        }

        if (rc != SUCCESS) {
            return rc;
        }
    }

    return SUCCESS;
}

int createOrUpdateFile(LocalFileSystem* const fs, const vector<string>& pathComponents, const string& fileContent)
{
    // Ensure path exists and get target inode number
    size_t targetInodeNumber;
    int rc = ensurePathExists(fs, pathComponents, targetInodeNumber);
    if (rc != SUCCESS) {
        return rc;
    }

    // Verify target is a file, not a directory
    inode_t inode;
    if (fs->stat(targetInodeNumber, &inode) != 0) {
        return INTERNAL_ERROR;
    }

    if (inode.type == UFS_DIRECTORY) {
        return BAD_REQUEST; // Can't write to a directory
    }

    if (fileContent.empty()) {
        return SUCCESS;
    }

    // Write the content to the file
    int bytesWritten = fs->write(targetInodeNumber, fileContent.c_str(), fileContent.length());
    if (bytesWritten != static_cast<int>(fileContent.length())) {
        return INTERNAL_ERROR;
    }

    return SUCCESS;
}

int getPathContents(LocalFileSystem* const fs, vector<string>& pathComponents, stringstream& output)
{
    // Navigate to target directory and get inode number
    size_t targetInodeNumber;
    int navResult = navigateToDirectory(fs, pathComponents, targetInodeNumber);
    if (navResult != SUCCESS) {
        return navResult;
    }

    // Get inode information for target
    inode_t targetInode;
    if (fs->stat(targetInodeNumber, &targetInode) != 0) {
        return BAD_REQUEST;
    }

    // Handle file and directory differently
    if (targetInode.type == UFS_REGULAR_FILE) {
        return handleFileContent(fs, pathComponents, output, targetInode.size, targetInodeNumber);
    } else {
        return handleDirectoryContent(fs, targetInode, targetInodeNumber, output);
    }
}
