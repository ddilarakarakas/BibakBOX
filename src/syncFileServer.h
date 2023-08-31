#ifndef SYNC_FILE_SERVER_H
#define SYNC_FILE_SERVER_H

#include "fileTree.h"
#include "serverThread.h"

void freeFileNodeLIst(struct FileNodeList* list);
void syncServer(int *fd,struct thrArg *myArg,struct FileNode** root);

#endif