
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <dirent.h>  
#include <pthread.h>

#include "syncFileServer.h"
#include "fileTree.h"
#include "common.h"
#include "fileOperations.h"
#include "serverThread.h"
#include "commonSyncFile.h"
#include "syncCheckSendClient.h"
#include "syncCheckSendCommon.h"


// // root/
// //   a =>1
// //   b/ =>1
// //     c =>0

// // root/
// //   a*
// //   b/
// //   d/
    

// // root/
// //     a* => 0
// //     b/ => 0
// //     d => 1

// // {SYNC_FILE}
// // {root/a}xds{SYNC_DONE}
// // {SYNC_UPDATE_DONE}

// {SYNC_FILE}
//     {/a/x1} / {SYNC_UPDATE_DONE}
//         1111{SYNC_DONE}

// {/a/x2}111dsadsdsa{dsa1{SYNC_DONE}
// {/a/b/x3}sadsadas{SYNC_DONE}
// {/a/x1}1111{SYNC_DONE}
// {/a/x1}1111{SYNC_DONE}
// {SYNC_UPDATE_DONE}

// {SYNC_DELETE}{/b/x2}{/b/x2}{/b/x2}{/b/x2}{/b/x2}{/b/x2}{SYNC_DELETE_DONE}

// {SYNC_CREATE}
// {SYNC_FILE}{/b/123}asasas{SYNC_DONE}
// {SYNC_CREATE_DONE}


#define BUFFER_SIZE 256
#define ERROR -1
#define RECEIVE_BUFFER_SIZE 2048

//void syncCheck(int *fd,struct thrArg *myArg,struct FileNode** root);
void syncCheck(int *fd,char *directory,char *fileName,struct FileNode** root){

    //build File Tree;
    char treeRoot[strlen(directory)+strlen(fileName)+1];
    treeRoot[0]='\0';

    if(directory[strlen(directory)-1] == '/'){
        sprintf(treeRoot,"%s%s",directory,fileName);
    }
    else{
        sprintf(treeRoot,"%s/%s",directory,fileName);
    }

    char serverRelativePathRoot[2];
    serverRelativePathRoot[0] = '\0';
    
    //printf("Modified files sending to client.\n");
    struct FileNode* newRoot = createNode(fileName, 0, 0, FOLDER_NODE,0);
    buildTreeForCheckAndSendUpdates(treeRoot,serverRelativePathRoot,newRoot,(*root),fd);
    //printf("Modified files sent to client.\n");

    char done[BUFFER_SIZE];
    done[0]='\0';

    sprintf(done,"{SYNC_UPDATE_DONE}");
    if (write((*fd),done,strlen(done)) < 0){
        perror("Failed to write to socket");
        return;  
    }
    //printf("Deleted files sending to client.");
    sendDeletedFilesAndFreeWrapper(serverRelativePathRoot,root,fd);
    //printf("Deleted files sent to client.\n");
    
    //printf("Created files sending to client.");
    sendCreateFilesWrapper(treeRoot,serverRelativePathRoot,newRoot,fd);
    //printf("Created files sending to client.\n");

    (*root) = newRoot;
}