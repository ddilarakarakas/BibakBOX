#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <errno.h>

#include "common.h"
#include "fileTree.h"
#include "commonSyncFile.h"
#include "fileOperations.h"
#include "syncCheckReceive.h"
#include "syncCheckSendClient.h"
#include "syncCheckSendCommon.h"



#define ERROR_BUFFER_SIZE 512
#define BUFFER_SIZE 512
#define RETURN_FAIL -1
#define RETURN_SUCCESS 0
#define ERROR -1
#define SOCKET_QUEUE_SIZE 200
#define ZERO 0
#define SOCKET_BUFFER_SIZE 1024

char errorBuffer[ERROR_BUFFER_SIZE];
char readBuffer[SOCKET_BUFFER_SIZE];


void sigIntHandlerMain();
void setSignalHandler(sigset_t* mask, sigset_t* oldmask);
void connectServer(int *clientfd,struct sockaddr_in *addr,int portNumber,char *ip);
void sendFileName(char *filename, int *fd);
struct FileNodeList * receiveSyncFilesWrapper(int *fd);
void sendExit(int *fd);
struct FileNode* syncPartOfClient(char **dirName,int *clientfd);

int main(int argc, char *argv[])
{
    struct sockaddr_in addr;
    sigset_t mask;    
    sigset_t oldmask;
    char *ip = "127.0.0.1";
    int clientfd;

    if (argc < 3 || argc > 4){
        sprintf(errorBuffer,"Usage: BibakBOXClient [dirName] [portnumber]\n");
        printError(errorBuffer);
        return RETURN_FAIL;
    }

    if(argc == 4){
        ip = argv[3];
    }

    char *dirName2 = argv[1];
    char *dirName;
    if(dirName2[strlen(dirName2)-1] != '/'){
        dirName = (char *)malloc(sizeof(char)*strlen(dirName2)+3);
        dirName[0] = '\0';
        strcpy(dirName,dirName2);
        strcat(dirName,"/");
    }
    else{
        dirName = dirName2;
    }

    int portNumber = atoi(argv[2]);

    if (!folderExists(dirName)){
        if (mkdir(dirName, 0777 | S_IFDIR) == ERROR) {
            if (errno == EACCES) {
                perror("Please run your terminal in sudo mode in order to server to create folder");
                exit(EXIT_SUCCESS);
            } 
            perror("Failed to create folder");
        }
    }

    setSignalHandler(&mask,&oldmask);

    //block sigint until server connection is done
    //blockSIGINT(&mask,&oldmask);
    connectServer(&clientfd,&addr,portNumber,ip);
    printf("Connection established with IP : %s\n ",ip);
    //unblockSIGINT(&mask,&oldmask);


    blockSIGINT(&mask,&oldmask);
    ////// SYNC ///////
    struct FileNode* root = syncPartOfClient(&dirName,&clientfd);
    unblockSIGINT(&mask,&oldmask);


    //send sync check done flag to client!
    char buffer[BUFFER_SIZE];
    buffer[0] = '\0';
    sprintf(buffer,"{DONE_CONTROL}");
    if(write(clientfd,buffer,strlen(buffer)) == -1){
        perror("write");
        exit(EXIT_FAILURE);
    }
    


    while(1){
        struct FileNode *tempTree = copyFileTree(root);

        printf("Listening server for modifications.\n");
        syncCheckReceive(&clientfd, dirName, root->name,NULL);
        //printf("Sending modified files to server.\n");
        syncCheck(&clientfd,dirName,root->name,&tempTree);
        root = tempTree;
        
        char buffer[BUFFER_SIZE];
        buffer[0] = '\0';
        sprintf(buffer,"{DONE_CONTROL}");
        if(write(clientfd,buffer,strlen(buffer)) == -1){
            perror("write");
            exit(EXIT_FAILURE);
        }
        if(write(clientfd,buffer,strlen(buffer)) == -1){
            perror("write");
            exit(EXIT_FAILURE);
        }
    }

    sendExit(&clientfd);
    return RETURN_SUCCESS;
}

struct FileNode* syncPartOfClient(char **dirName,int *clientfd){
    // dirname: 1=>/home/yck/Desktop/final/clientDir/
    // rootname: clientDir
    char *rootName = getNameFromPath((*dirName));

    struct FileNode* root = createNode(rootName, 0, 0, FOLDER_NODE,0);
    buildTree((*dirName), root);

    int lenDirName = strlen((*dirName));
    int lenRootName = strlen(rootName);
    (*dirName)[lenDirName - lenRootName - 2] = '\0';

    sendFileName(rootName,clientfd);

    sendSyncFileWrapper(root, clientfd);

    handleSyncGetRequests(clientfd,(*dirName));

    struct FileNodeList *list = receiveSyncFilesWrapper(clientfd);
    syncServerWithfd(clientfd,(*dirName),list,1);
    free(rootName);


    // TODO
    //clear and rebuild fileNode
    return root;
}

struct FileNodeList * receiveSyncFilesWrapper(int *fd){
    //read {SYNC}
    char lineBuffer[BUFFER_SIZE];

    int error=read((*fd),readBuffer,6);
    if(error==ERROR){
        sprintf(lineBuffer,"Error while read operation.!\n");
        printError(lineBuffer);
        exit(EXIT_FAILURE);
    }
    if(strcmp(readBuffer,"{SYNC}")==0){
        return receiveSyncFiles(fd);
    }

    return NULL;

}

void sendFileName(char *filename, int *fd){
    char writeBuffer[SOCKET_BUFFER_SIZE];
    writeBuffer[0]='\0';
    sprintf(writeBuffer,"{%s}",filename);
    int error=write((*fd),writeBuffer,strlen(writeBuffer));
    if(error==ERROR){
        perror("Error while write operation.!\n");
        exit(EXIT_FAILURE);
    }
}

void sendExit(int *fd){
    char writeBuffer[SOCKET_BUFFER_SIZE];
    writeBuffer[0]='\0';
    sprintf(writeBuffer,"{EXIT}");
    int error=write((*fd),writeBuffer,strlen(writeBuffer));
    if(error==ERROR){
        perror("Error while write operation.!\n");
        exit(EXIT_FAILURE);
    }
}

/*In case of CTRL + C, this handler invoke for main and free all 
	the resources and exits gracefully.*/
void sigIntHandlerMain(){
    printScreen("\nCTRL+C signal catched. Cleaning resources and exiting gracefully.\n");
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

void connectServer(int *clientfd,struct sockaddr_in *addr,int portNumber,char *ip){
    int error=0;
    char lineBuffer[BUFFER_SIZE];

    /*Open socket.*/
    if(((*clientfd)=socket(PF_INET, SOCK_STREAM,0))==ERROR){
        sprintf(lineBuffer,"error while socket operation!\n");
        printError(lineBuffer);
        exit(EXIT_FAILURE);
    }

    addr->sin_family=AF_INET;
    addr->sin_port=htons(portNumber);/*Set server port. Using htons provide 
                                        proper byte order.*/ 

    /* convert command line argument to numeric IP */
    if (inet_pton(AF_INET,ip,&(addr->sin_addr))!=1) {
        sprintf(lineBuffer,"Invalid IP address\n");
        printError(lineBuffer);
        exit(EXIT_FAILURE);
    }


    if((error=connect((*clientfd),(struct sockaddr*)(addr),sizeof((*addr))))==ERROR){
            /* close socket*/
        if(close((*clientfd))<0){
            perror("socket close error");
            exit(EXIT_FAILURE);
        }

        sprintf(lineBuffer,"connect error:%s,%d!\n",strerror(errno),errno);
        printError(lineBuffer);
        exit(EXIT_FAILURE);
    }

    sprintf(lineBuffer,"Connected to server.\n");
    printScreen(lineBuffer);
}

