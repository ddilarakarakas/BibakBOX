#ifndef SYNC_CHECK_RECEIVE_H
#define SYNC_CHECK_RECEIVE_H

void syncCheckReceive(int *fd, char *dirname, char *rootName,int *logfd);
void syncReceiveFile(int *fd,char *directory,int *logfd);
void readSocketWriteFile(int *fd,int *file_fd);
void deleteReceiveFile(int *fd, char *directory,int *logfd);
int remove_directory(char *path);
void createReceiveFile(int *fd, char *directory, int *logfd);

#endif