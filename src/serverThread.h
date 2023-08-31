#ifndef SERVER_THREAD_H
#define SERVER_THREAD_H

#include "fileTree.h"

/*Stores thread arguments for each thread..*/
struct thrArg{
    pthread_t *th;
    int fd; /*Socket fd for communicating client.*/
    pthread_cond_t *thr_cond; /*Thread contion for client*/
    int no; /*Thread no.*/
    char *fileName;
    pthread_mutex_t *thr_mutex;
    int isBusy;
    char *directory;
    int *logfd;
};

void *socketThread(void *arg);
void handleRequest(struct thrArg *myArg,struct FileNode** root);
int handleCase(char *synCase, int *fd,struct thrArg *myArg, struct FileNode** root);
void setFileName(struct thrArg* myArg);
struct thrArg* createThreadArg(int id,char *directory);
struct thrArg** createThreadList(int size,char *directory);
void lockMutexMain();
void unlockMutexMain();
void sendSignalIfMainThreadWaiting();
void handleSIGINTIfSignalArrived(struct thrArg *myArg);
void cleanupHandlerThreads();
// void printTree(struct FileNode* node, int level);

extern int busyThreads;
extern pthread_mutex_t thread_m;
extern pthread_cond_t server_cond;
extern pthread_cond_t sig_cond;
extern int serverWait;
extern int isSIGINT;

#endif