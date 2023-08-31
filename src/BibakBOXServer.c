
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h> /* For socket, connect, socklen_t */
#include <arpa/inet.h> /* sockaddr_in, inet_pton */
#include <sys/wait.h>
#include <pthread.h>
#include <errno.h>

#include "common.h"
#include "serverThread.h"
#include "syncFileServer.h"
#include "fileTree.h"
#include "fileOperations.h"
#include "lockManager.h"

#define ERROR_BUFFER_SIZE 512
#define BUFFER_SIZE 512
#define RETURN_FAIL -1
#define RETURN_SUCCESS 0
#define ERROR -1
#define SOCKET_QUEUE_SIZE 200
#define ZERO 0

char errorBuffer[ERROR_BUFFER_SIZE];
char lineBuffer[BUFFER_SIZE];


void sigIntHandlerMain();
void setSignalHandler(sigset_t* mask, sigset_t* oldmask);
void createAndListenSocket(struct sockaddr_in *addr,int portNumber,int *serverfd);
void waitAwailableThreadIfNecessary();
void findAvailableThreadAndAssign(int *fd);

struct thrArg** threadList;
int threadPoolSize=0;


int main(int argc, char *argv[])
{
    struct sockaddr_in addr;
    socklen_t cLen;
    int conn_fd;
    int serverfd;

    sigset_t mask;    
    sigset_t oldmask;

    if (argc != 4){
        sprintf(errorBuffer,"Usage: BibakBOXServer [directory] [threadPoolSize] [portnumber]\n");
        printError(errorBuffer);
        return RETURN_FAIL;
    }

    char *dirName2 = argv[1];
    char *directory;
    if(dirName2[strlen(dirName2)-1] != '/'){
        directory = (char *)malloc(sizeof(char)*strlen(dirName2)+3);
        directory[0] = '\0';
        strcpy(directory,dirName2);
        strcat(directory,"/");
    }
    else{
        directory = dirName2;
    }

    threadPoolSize = atoi(argv[2]);
    int portNumber = atoi(argv[3]);

    if (!folderExists(directory)){
        if (mkdir(directory, 0777 | S_IFDIR) == ERROR) {
            if (errno == EACCES) {
                perror("Please run your terminal in sudo mode in order to server to create folder");
                exit(EXIT_SUCCESS);
            } 
            perror("Failed to create folder");
        }
    }


    setSignalHandler(&mask,&oldmask);

    initArray();
    //block sigint until all initialization done
    blockSIGINT(&mask,&oldmask);

    createAndListenSocket(&addr,portNumber,&serverfd);
    threadList = createThreadList(threadPoolSize,directory);

    unblockSIGINT(&mask,&oldmask);


    while(1){
        printf("Waiting for new connection...");
        cLen = sizeof (addr);
        if((conn_fd = accept(serverfd,(struct sockaddr*) &addr,&cLen)) < ZERO) {
            sprintf(lineBuffer,"Error on accept.\n");
            printError(lineBuffer);
            exit(EXIT_FAILURE);
        }

        lockMutexMain();
        waitAwailableThreadIfNecessary();
        findAvailableThreadAndAssign(&conn_fd);

        unlockMutexMain();
    }

    return RETURN_SUCCESS;
}


void waitAwailableThreadIfNecessary(){
    int error=0;

    if(busyThreads == threadPoolSize){
        serverWait=1;

        sprintf(lineBuffer,"Waiting available thread.\n");
        printf("%s\n",lineBuffer);

        /*Waits for any one of threads will be available.*/
        error=pthread_cond_wait(&server_cond,&thread_m);
        if(error!=0){
            sprintf (lineBuffer, "Failed while waiting conditional variable.\n");
            printError(lineBuffer);
            exit(EXIT_FAILURE);
        }

        sprintf(lineBuffer,"Thread become available.\n");
        printf("%s\n",lineBuffer);
        
        serverWait=0;
    }
}

void findAvailableThreadAndAssign(int *fd){
    int error=0;

    /*Find available thread*/
    for (int i = 0; i < threadPoolSize; i++) {
        if (threadList[i]->isBusy==0) {

            sprintf(lineBuffer,"Assigning client to thread %d\n",threadList[i]->no);
            printf("%s\n",lineBuffer);
            
            threadList[i]->isBusy=1;
            busyThreads++;

            threadList[i]->fd=dup((*fd));

            /*Send signal to thread*/
            error=pthread_cond_signal((threadList[i]->thr_cond));
            if(error!=0){
                sprintf (lineBuffer, "Error while signalling conditional variable.\n");
                printError(lineBuffer);
                exit(EXIT_FAILURE);
            }
            break;
        }
    }
}

/*In case of CTRL + C, this handler invoke for main and free all 
	the resources and exits gracefully.*/
void sigIntHandlerMain(){
    printScreen("\nCTRL+C signal catched. Cleaning resources and exiting gracefully.\n");

    isSIGINT=1;

    int error=pthread_mutex_lock(&thread_m);
    if(error!=0){
        sprintf(errorBuffer,"Failed while locking mutex.\n");
        printError(errorBuffer);
        exit(EXIT_FAILURE);
    }

    while(busyThreads!=0){
        error=pthread_cond_wait(&sig_cond,&thread_m);
        if(error!=0){
            sprintf (errorBuffer, "Failed while waiting conditional variable.\n");
            printError(errorBuffer);
            exit(EXIT_FAILURE);
        }      
    }


    for(int i=0; i<threadPoolSize; i++){
        pthread_cancel(*(threadList[i]->th));

        unlockMutexMain();

        pthread_join(*(threadList[i]->th),NULL);
        free(threadList[i]->th);


        if(pthread_cond_destroy(threadList[i]->thr_cond)!=0){
            sprintf (errorBuffer, "Failed while destroying conditional variable.\n");
            printError(errorBuffer);
            exit(EXIT_FAILURE);
        }

        free(threadList[i]->thr_cond);

        if(pthread_mutex_destroy(threadList[i]->thr_mutex)!=0){
            sprintf (errorBuffer, "Failed while destroying mutex.\n");
            printError(errorBuffer);
            exit(EXIT_FAILURE);
        }


        free(threadList[i]->thr_mutex);


        if(threadList[i]->fileName!=NULL){
            free(threadList[i]->fileName);
        }
        if(threadList[i]->logfd!=NULL){
            free(threadList[i]->logfd);
        }

        // if(threadList[i]->fd!=-1)
        //         close(threadList[i]->fd);
        // }


        free(threadList[i]);

        lockMutexMain();

    }

    destroyArray();
    free(threadList);

    exit(EXIT_SUCCESS);
}

void setSignalHandler(sigset_t* mask, sigset_t* oldmask){
    struct sigaction sigint_action; /*This variables for handling sigint.*/

    initializeSIGBlockParams(mask,oldmask);

    /*Sets the signal handler for SIGINT.*/
	memset(&sigint_action,0,sizeof(sigint_action));
	sigint_action.sa_handler = &sigIntHandlerMain;
	if(sigaction(SIGINT,&sigint_action,NULL)==ERROR){
        sprintf(errorBuffer,"\nError while sigaction operation.\n");                
        printError(errorBuffer);
    }
}

void createAndListenSocket(struct sockaddr_in *addr,int portNumber,int *serverfd){

    // struct sockaddr acceptAddr;

    memset(addr, 0, sizeof((*addr))); /*Reset structure.*/
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    addr->sin_port = htons(portNumber);/*Set server port. Using htons provide proper byte order.*/

    /*SOCKET*/
    (*serverfd)=socket(AF_INET, SOCK_STREAM, 0);
    if((*serverfd)==ERROR){
        sprintf(lineBuffer,"Error on socket.\n");
        printError(lineBuffer);
        exit(EXIT_FAILURE);
    }

    /*Bind created socket.*/
    if(bind((*serverfd), (struct sockaddr*) addr, sizeof((*addr))) < ZERO){
        sprintf(lineBuffer,"Error on bind:%s.\n",strerror(errno));
        printError(lineBuffer);
        exit(EXIT_FAILURE);
    }

    if(listen((*serverfd),SOCKET_QUEUE_SIZE) < 0){
        sprintf(lineBuffer,"Error on listen.\n");
        printError(lineBuffer);
        exit(EXIT_FAILURE);
    }

}

