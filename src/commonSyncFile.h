#ifndef COMMON_SYNC_FILE_H
#define COMMON_SYNC_FILE_H

#include "fileTree.h"
#include "serverThread.h"

void sendSyncFileWrapper(struct FileNode* node, int *fd);
void sendSyncFile(struct FileNode* node, int *fd,char *path);
void handleSyncGetRequests(int *fd,char *directory);
struct FileNodeList* receiveSyncFiles(int *fd);
void syncServerWithfd(int *fd,char *directory,struct FileNodeList* fileList,int isClient);
#endif