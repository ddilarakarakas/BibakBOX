#ifndef SYNC_CHECK_SEND_COMMON_H
#define SYNC_CHECK_SEND_COMMON_H

#include "fileTree.h"
#include "serverThread.h"

void sendCreateFilesWrapper(char *path, char *relativePath,struct FileNode* root, int *fd );
void sendCreateFiles(char *path, char *relativePath,struct FileNode* root, int *fd );
void sendDeletedFilesAndFreeWrapper(char *relativePath,struct FileNode** root, int *fd );
void sendDeletedFilesAndFree(char *relativePath,struct FileNode** root, int *fd );
void sendFileNameForSync(int *fd,char *relativePath);
void buildTreeForCheckAndSendUpdates(char* path,char *relativePath, 
            struct FileNode* parentNode, struct FileNode *oldRoot, int *fd);
void updateNode(char *childPath, enum NodeType type, int *fd, char *serverRelativePath);
void readFromFileWriteToSocket(char *childPath, int *fd);





void printTree(struct FileNode* node, int level);

#endif