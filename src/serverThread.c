#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/wait.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <sys/types.h>

#include "serverThread.h"
#include "common.h"
#include "syncFileServer.h"
#include "fileOperations.h"
#include "fileTree.h"
#include "syncCheckSendServer.h"
#include "syncCheckSendCommon.h"
#include "syncCheckReceive.h"
#include "lockManager.h"


struct thrArg;

#define ERROR_BUFFER_SIZE 512
#define BUFFER_SIZE 512
#define SOCKET_BUFFER_SIZE 1024
#define ERROR -1

#define TRUE 0
#define FALSE 1


int busyThreads=0;
int serverWait=0;
int isSIGINT=0;

/*Mutex for synronization between threads and main*/
pthread_mutex_t thread_m = PTHREAD_MUTEX_INITIALIZER; 

/*Cond. variable for when there is no available thread*/
pthread_cond_t server_cond=PTHREAD_COND_INITIALIZER;

/*Cond. variable for signal handling for threads*/
pthread_cond_t sig_cond=PTHREAD_COND_INITIALIZER;



/*Thread function*/
void *socketThread(void *arg){
    int error=0;
    char lineBufferTh[BUFFER_SIZE];

    struct thrArg *myArg=((struct thrArg*)arg);

    pthread_cleanup_push(cleanupHandlerThreads,NULL);


    sprintf(lineBufferTh,"Thread %d is ready.\n",myArg->no);
    printf("%s\n",lineBufferTh);

    while(1){
        
        if(myArg->fileName!=NULL){
            free(myArg->fileName);
            myArg->fileName=NULL;
        }

        if(myArg->logfd!=NULL){
            free(myArg->logfd);
            myArg->logfd=NULL;
        }

        //cond variable for parent to assign a task
        error=pthread_cond_wait((myArg->thr_cond),&thread_m);
        if(error!=0){
            perror("Failed while waiting conditional variable.\n");
            exit(EXIT_FAILURE);
        }
        unlockMutexMain();



        //get root filename from client
        setFileName(myArg);

        char fullDir[strlen(myArg->fileName)+strlen(myArg->directory)+1];
        fullDir[0]='\0';
        if(myArg->directory[strlen(myArg->directory)-1]!='/' && myArg->fileName[0]!='/'){
            sprintf(fullDir,"%s/%s",myArg->directory,myArg->fileName);
        }
        else{

            if(myArg->directory[strlen(myArg->directory)-1]=='/' && myArg->fileName[0]=='/'){
                char temp[strlen(myArg->directory)];
                temp[0]='\0';
                strncpy(temp,myArg->directory,strlen(myArg->directory)-2);
                sprintf(fullDir,"%s%s",temp,myArg->fileName);
            }else{
                sprintf(fullDir,"%s%s",myArg->directory,myArg->fileName);
            }
        }

        sprintf(fullDir,"%s%s",myArg->directory,myArg->fileName);

        printf("Thread connected to file %s for client. \n",myArg->fileName);



        char logFile[strlen(fullDir)+20];
        logFile[0]='\0';
        if(fullDir[strlen(fullDir)-1]=='/'){
            sprintf(logFile,"%slog.txt",fullDir);
        }
        else{
            sprintf(logFile,"%s/log.txt",fullDir);
        }

        myArg->logfd = malloc(sizeof(int)*1);

        char pathLog[strlen(myArg->directory) + strlen(myArg->fileName) + 2];
        pathLog[0]='\0';

        if(myArg->directory[strlen(myArg->directory)-1]=='/' || myArg->fileName[strlen(myArg->fileName)-1]=='/'){
            sprintf(pathLog,"%s%s",myArg->directory,myArg->fileName);
        }
        else{
            sprintf(pathLog,"%s/%s",myArg->directory,myArg->fileName);
        }

        if (!folderExists(pathLog)){
            if (mkdir(pathLog, 0777 | S_IFDIR) == ERROR) {
                if (errno == EACCES) {
                    perror("Please run your terminal in sudo mode in order to server to create folder");
                    exit(EXIT_SUCCESS);
                } 
                perror("Failed to create folder");
            }
        }

        printf("Log file is %s\n",logFile);
        if( (*(myArg->logfd)=(open(logFile,O_CREAT | O_RDWR, 0777))) == ERROR){
            perror("Error while opening file.");
            exit(EXIT_FAILURE);
        }




        //handle incoming requests
        struct FileNode* root;
        handleRequest(myArg,&root);

        ////////////////////////////////////////////////////////////////

        lockMutexMain();
        handleSIGINTIfSignalArrived(myArg);
        sendSignalIfMainThreadWaiting();

        busyThreads--;
        myArg->isBusy=0;
        free(myArg->fileName);
    }
    pthread_cleanup_pop(0);
}

void handleRequest(struct thrArg *myArg,struct FileNode** root){
    int bufferSize=256;
    char *buffer = (char *)malloc(sizeof(char)*bufferSize);
    int bufferIndex=0;

    char bufferChar[2];
    buffer[0]='\0';
    buffer[1]='\0';

    while(1){
        if(isSIGINT==1){
            free(myArg->fileName);
            free(buffer);
            exit(EXIT_SUCCESS);
        }
        if(read(myArg->fd,bufferChar,1)==ERROR){
            perror("Error while reading from socket.");
            exit(EXIT_FAILURE);
        }

        if(bufferChar[0]=='\0'){
            continue;
        }
        
        if(isSIGINT==1){
            free(myArg->fileName);
            free(buffer);
            exit(EXIT_SUCCESS);
        }
        
        if(bufferChar[0]=='{'){
            continue;
        }
        else if(bufferChar[0]=='}'){

            if(handleCase(buffer,&(myArg->fd),myArg,root)==FALSE){
                printf("Thread %d is done.\n",myArg->no);
                free(buffer);
                break;
            }
            buffer[0]='\0';
            bufferIndex=0;
        }
        else{
            buffer[bufferIndex++] = bufferChar[0];
            buffer[bufferIndex] = '\0';
            if(bufferIndex == bufferSize-1){
                bufferSize = reallocateMemory(&buffer,bufferSize);
            }
        }

    }
}

int handleCase(char *synCase, int *fd,struct thrArg *myArg,struct FileNode** root){

    if(strcmp(synCase,"SYNC")==0){
        printf("Syncing files with client. %s \n",myArg->fileName);
        
        char path[strlen(myArg->directory)+strlen(myArg->fileName)+1];
        path[0]='\0';
        sprintf(path,"%s/%s",myArg->directory,myArg->fileName);

        takeLock(path);
        syncServer(fd,myArg,root);
        unlockLock(path);

        printf("Files synched with client.  %s \n",myArg->fileName);
        return TRUE;
    }
    if(strcmp(synCase,"DONE_CONTROL")==0){

        //printf("Waiting x sec for files modification.%s \n",myArg->fileName);
        sleep(10);

        if(isSIGINT==1){
            printf("EXITINGGG\n");
            // freeFileTree(root);
            return FALSE;
        }
        printf("10 sec passed. Sync starting...  %s \n",myArg->fileName);

        syncCheckServer(fd,myArg->directory,myArg->fileName,root,myArg);
        //printf("All modified files sent to client.%s \n",myArg->fileName);

        char readBuf[SOCKET_BUFFER_SIZE];
        readBuf[0]='\0';
        sprintf(readBuf,"{DONE_CONTROL}");
        if(write((*fd),readBuf,strlen(readBuf))==ERROR){
            perror("Error while writing to socket.");
            return FALSE;
        }

        printf("Server started to listen client for modified files.  %s \n",myArg->fileName);

        takeLock(myArg->directory);
        syncCheckReceive(fd,myArg->directory,myArg->fileName,myArg->logfd);

        char treeRoot[strlen(myArg->directory)+strlen(myArg->fileName)+1];
        treeRoot[0]='\0';
        sprintf(treeRoot,"%s%s",myArg->directory,myArg->fileName);
        (*root) = createNode(myArg->fileName, 0, 0, FOLDER_NODE,0);
        buildTree(treeRoot, (*root));

        printf("All files synced with client.  %s \n",myArg->fileName);
        unlockLock(myArg->directory);
        return TRUE;
    }
    else if(strcmp(synCase,"EXIT")==0){
        return FALSE;
    }
    return TRUE;
}

void setFileName(struct thrArg* myArg){
    int error=0;
    int bufferSize=256;
    char *buffer = (char *)malloc(sizeof(char)*bufferSize);
    int bufferIndex=0;

    char bufferChar[2];
    buffer[0]='\0';
    buffer[1]='\0';

    while(1){

        error=read(myArg->fd,bufferChar,1);
        if(error==ERROR){
            perror("Error while read operation.");
            exit(EXIT_FAILURE);
        }

        if(bufferChar[0]=='{'){
            continue;
        }
        else if(bufferChar[0]=='}'){
            break;
        }
        else{
            buffer[bufferIndex++] = bufferChar[0];
            buffer[bufferIndex] = '\0';
            if(bufferIndex == bufferSize-1){
                bufferSize = reallocateMemory(&buffer,bufferSize);
            }
        }
    }

    myArg->fileName = buffer;
    printf("File name is %s \n",myArg->fileName);
}

void handleSIGINTIfSignalArrived(struct thrArg *myArg){
    int error=0;
    char errorBuffferTh[ERROR_BUFFER_SIZE];
    if(isSIGINT==1){

        busyThreads--;
        myArg->isBusy=0;

        /*Send signal to SIGINT handler for notifying */
        error=pthread_cond_signal(&(sig_cond));
        if(error!=0){
            sprintf(errorBuffferTh,"Error while signalling conditional variable.\n");
            printError(errorBuffferTh);
            pthread_exit(0);
        }

        error=pthread_mutex_unlock(&thread_m);
        if(error!=0){
            sprintf(errorBuffferTh,"Failed while unlocking mutex.\n");
            printError(errorBuffferTh);
            pthread_exit(0);
        }

        pthread_exit(0);
    }
}

void sendSignalIfMainThreadWaiting(){
    int error=0;
    char errorBuffferTh[ERROR_BUFFER_SIZE];
    if(serverWait==1){
        /*Send signal to main funct. for notifying 
            there is available thread.*/
        error=pthread_cond_signal(&(server_cond));
        if(error!=0){
            sprintf (errorBuffferTh, "Error while signalling conditional variable.\n");
            printError(errorBuffferTh);
            exit(EXIT_FAILURE);
        }
    }   
}

struct thrArg* createThreadArg(int id,char *directory){
    struct thrArg *arg=NULL;
    int error=0;

    arg=(struct thrArg *)malloc(sizeof(struct thrArg));
    arg->fd=-1;
    arg->thr_cond=(pthread_cond_t *)malloc(sizeof(pthread_cond_t));
    error=pthread_cond_init(arg->thr_cond,NULL);
    if(error!=0){
        free(arg->thr_cond);
        return NULL;
    }
    arg->no=id;
    arg->th=(pthread_t*)malloc(sizeof(pthread_t));
    arg->fileName=NULL;
    arg->thr_mutex=(pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    arg->isBusy=0;
    arg->directory=directory;
    arg->logfd=NULL;
    
    return arg;
}

struct thrArg** createThreadList(int size,char *directory){
    int i;
    int error=0;
    char errorBuffferTh[ERROR_BUFFER_SIZE];

    struct thrArg **thrArg=(struct thrArg**)malloc(sizeof(struct thrArg*)*size);

    for(i=0;i<size;i++){
        thrArg[i]=createThreadArg(i,directory);

        error=pthread_create(thrArg[i]->th,NULL,socketThread,thrArg[i]);
        if(error!=0){
            sprintf(errorBuffferTh,"Thread couldn't create!.\n");
            printError(errorBuffferTh);
            exit(EXIT_FAILURE);
        }

        //init mutex
        error=pthread_mutex_init(thrArg[i]->thr_mutex,NULL);
        if(error!=0){
            sprintf(errorBuffferTh,"Thread mutex couldn't create!.\n");
            printError(errorBuffferTh);
            exit(EXIT_FAILURE);
        }
    }

    return thrArg;
}

void lockMutexMain(){
    char errorBuffferTh[ERROR_BUFFER_SIZE];
    int error=0;
    error=pthread_mutex_lock(&thread_m);
    if(error!=0){
        sprintf(errorBuffferTh,"Failed while locking mutex.\n");
        printError(errorBuffferTh);
        exit(EXIT_FAILURE);
    }
}

void unlockMutexMain(){
    char errorBuffferTh[ERROR_BUFFER_SIZE];
    int error=0;
    error=pthread_mutex_unlock(&thread_m);
    if(error!=0){
        sprintf(errorBuffferTh,"Failed while locking mutex.\n");
        printError(errorBuffferTh );
        exit(EXIT_FAILURE);
    }
}

/*Clean handler for server threads.*/
void cleanupHandlerThreads(){
    char errorBuffferTh[ERROR_BUFFER_SIZE];
    int error=0;
    error=pthread_mutex_unlock(&thread_m);
    if(error!=0){
        sprintf(errorBuffferTh,"Failed while locking mutex.\n");
        printError(errorBuffferTh);
        exit(EXIT_FAILURE);
    }
 
}

