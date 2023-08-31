#ifndef SYNC_CHECK_SEND_SERVER_H
#define SYNC_CHECK_SEND_SERVER_H

#include "fileTree.h"
#include "serverThread.h"

void syncCheckServer(int *fd,char *directory,char *fileName,struct FileNode** root,struct thrArg *myArg);
void buildTreeForCheckAndSendUpdatesServer(char* path,char *relativePath, struct FileNode* parentNode, 
                    struct FileNode *oldRoot, int *fd, int *logfd);
void updateNodeServerServer(char *childPath, enum NodeType type, int *fd, char *serverRelativePath, int *logfd);
void sendDeletedFilesAndFreeWrapperServer(char *relativePath,struct FileNode** root, int *fd, int* logfd);
void sendDeletedFilesAndFreeServer(char *relativePath,struct FileNode** root, int *fd, int* logfd);
void sendFileNameForSyncDelete(int *fd,char *relativePath,int *logfd);
void sendCreateFilesWrapperServer(char *path, char *relativePath,struct FileNode* root, int *fd,int *logfd);
void sendCreateFilesServer(char *path, char *relativePath,struct FileNode* root, int *fd, int *logfd);
void readFromFileWriteToSocketServer(char *childPath, int *fd);

#endif