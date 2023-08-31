#ifndef COMMON_H
#define COMMON_H

#include "fileTree.h"
void printError(char *message);
void printScreen(char *message);
void blockSIGINT(sigset_t* mask, sigset_t* oldmask);
void unblockSIGINT(sigset_t* mask, sigset_t* oldmask);
void initializeSIGBlockParams(sigset_t* mask, sigset_t* oldmask);
int reallocateMemory(char **buffer, int size);
void freeFileTree(struct FileNode** root);

#endif