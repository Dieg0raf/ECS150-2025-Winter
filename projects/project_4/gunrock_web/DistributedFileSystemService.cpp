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

enum PathNavigationResult {
  SUCCESS = 0,
  NOT_FOUND = -1,
  BAD_REQUEST = -2,
  INSUFFICIENT_STORAGE = -3
};

// Helper functions
bool compareByName(const dir_ent_t &a, const dir_ent_t &b);

// Directory operations
vector<dir_ent_t> getDirectoryEntries(LocalFileSystem *const fileSystem,
                                      const inode_t *inode, size_t inodeNum);
void formatDirectoryEntries(const vector<dir_ent_t> &entries,
                            std::stringstream &stringStream);

// Content handling
int handleFileContent(vector<string> &pathComponents, stringstream &output,
                      size_t targetInode);

// Main filesystem operations
int navigateToDirectory(LocalFileSystem *const fileSystem,
                        const vector<string> &pathComponents,
                        size_t &targetInode);
int getPathContents(LocalFileSystem *const fileSystem,
                    vector<string> &pathComponents, stringstream &output);

DistributedFileSystemService::DistributedFileSystemService(string diskFile)
    : HttpService("/ds3/") {
  this->fileSystem = new LocalFileSystem(new Disk(diskFile, UFS_BLOCK_SIZE));
}

void DistributedFileSystemService::get(HTTPRequest *request,
                                       HTTPResponse *response) {
  // grab path
  vector<string> pathComponents = request->getPathComponents();
  stringstream stringStream;

  int rc = getPathContents(this->fileSystem, pathComponents, stringStream);
  if (rc == NOT_FOUND) {
    response->setStatus(404);
    response->setBody("Not Found");
    return;
  } else if (rc == BAD_REQUEST) {
    response->setStatus(400);
    response->setBody("Bad Request");
    return;
  } else if (rc == SUCCESS) {
    response->setStatus(200);
    response->setBody(stringStream.str());
  } else {
    response->setStatus(500);
    response->setBody("Internal Error");
  }
}

void DistributedFileSystemService::put(HTTPRequest *request,
                                       HTTPResponse *response) {
  response->setBody("This is a response from the PUT");
}

void DistributedFileSystemService::del(HTTPRequest *request,
                                       HTTPResponse *response) {
  response->setBody("This is a response from the DEL");
}

bool compareByName(const dir_ent_t &a, const dir_ent_t &b) {
  return std::strcmp(a.name, b.name) < 0;
}

int validatePathComponents(const vector<string> &pathComponents,
                           size_t &rootInode) {
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

vector<dir_ent_t> getDirectoryEntries(LocalFileSystem *const fileSystem,
                                      const inode_t *inode, size_t inodeNum) {
  // Read directory data
  vector<dir_ent_t> dirEntries;
  size_t dirSize = inode->size;
  unique_ptr<char[]> buffer(new char[dirSize]);
  if (fileSystem->read(inodeNum, buffer.get(), dirSize) == 0) {
    return dirEntries;
  }

  // Extract directory entries
  size_t entryCount = dirSize / sizeof(dir_ent_t);
  for (size_t j = 0; j < entryCount; j++) {
    dir_ent_t dirEntry;
    memcpy(&dirEntry, buffer.get() + (j * sizeof(dir_ent_t)),
           sizeof(dir_ent_t));
    dirEntries.push_back(dirEntry);
  }

  // Sort entries by name
  sort(dirEntries.begin(), dirEntries.end(), compareByName);

  return dirEntries;
}

void formatDirectoryEntries(const vector<dir_ent_t> &entries,
                            std::stringstream &stringStream) {
  if (entries.empty()) {
    return;
  }
  for (size_t i = 0; i < entries.size(); i++) {
    if (strcmp(entries.at(i).name, ".") == 0 ||
        strcmp(entries.at(i).name, "..") == 0) { // skip .. and . entries
      continue;
    }
    stringStream << entries[i].inum << "\t" << entries[i].name << endl;
  }
}

int handleFileContent(vector<string> &pathComponents, stringstream &output,
                      size_t targetInode) {
  string fileName = pathComponents.back();
  output << targetInode << "\t" << fileName << endl;
  return SUCCESS;
}

int navigateToDirectory(LocalFileSystem *const fileSystem,
                        const vector<string> &pathComponents,
                        size_t &targetInode) {
  // Validate input (paths)
  int validationResult = validatePathComponents(pathComponents, targetInode);
  if (validationResult != SUCCESS) {
    return validationResult;
  }

  // Navigate through path (start after ds3)
  size_t currentInode = 0;
  for (size_t i = 1; i < pathComponents.size(); i++) {
    int nextInode = fileSystem->lookup(currentInode, pathComponents[i]);
    if (nextInode == -EINVALIDINODE || nextInode == -ENOTFOUND) {
      return NOT_FOUND;
    }
    currentInode = nextInode;
  }
  targetInode = currentInode;

  return SUCCESS;
}

int getPathContents(LocalFileSystem *const fileSystem,
                    vector<string> &pathComponents, stringstream &output) {
  // Navigate to target directory
  size_t targetInode;
  int navResult = navigateToDirectory(fileSystem, pathComponents, targetInode);
  if (navResult != SUCCESS) {
    return navResult;
  }

  // Get inode information for target
  inode_t inode;
  if (fileSystem->stat(targetInode, &inode) != 0) {
    return BAD_REQUEST;
  }

  // Handle file and directory differently
  if (inode.type == UFS_REGULAR_FILE) {
    return handleFileContent(pathComponents, output, targetInode);
  } else {
    vector<dir_ent_t> entries =
        getDirectoryEntries(fileSystem, &inode, targetInode);
    formatDirectoryEntries(entries, output);
    return SUCCESS;
  }
}
