#ifndef SYNC_CHECK_SEND_CLIENT_H
#define SYNC_CHECK_SEND_CLIENT_H

#include "fileTree.h"
#include "serverThread.h"

void syncCheck(int *fd,char *directory,char *fileName,struct FileNode** root);


#endif