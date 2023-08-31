#ifndef FILEOPERATIONS_H
#define FILEOPERATIONS_H


void syncFile(char *path, int *fd,int fileType,char *directory,int isClient);
int fileExists(char* path);
int folderExists(const char* path);
void readAndWriteFileFromFd(char *path, int *fd, int *fileFd);
void renameFileWithUnique(char* originalFileName);
#endif
