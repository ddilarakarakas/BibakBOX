
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "syncFileServer.h"
#include "fileTree.h"
#include "common.h"
#include "fileOperations.h"
#include "serverThread.h"
#include "commonSyncFile.h"

#define BUFFER_SIZE 512
#define RECEIVE_BUFFER_SIZE 2048
#define MAX_FORMAT_LENGTH 2048


void freeFileNodeLIst(struct FileNodeList* list){
    for(int i=0; i<list->currentIndex;i++){
        free(list->list[i]);
    }
    free(list->list);
}



void syncServer(int *fd,struct thrArg *myArg,struct FileNode** root){

    struct FileNodeList* fileList = receiveSyncFiles(fd);
    syncServerWithfd(fd,myArg->directory,fileList,0);
    freeFileNodeLIst(fileList);
    free(fileList);



    // if(root!=NULL && (*root)!=NULL){
    //     freeFileTree(root);
    // }

    // freeFileTree(root);
    //build File Tree;
    char treeRoot[strlen(myArg->directory)+strlen(myArg->fileName)+1];
    treeRoot[0]='\0';
    sprintf(treeRoot,"%s%s",myArg->directory,myArg->fileName);
    (*root) = createNode(myArg->fileName, 0, 0, FOLDER_NODE,0);
    buildTree(treeRoot, (*root));


    sendSyncFileWrapper((*root), fd);

    char dir_temp[strlen(myArg->directory)+1];
    dir_temp[0]='\0';
    sprintf(dir_temp,"%s",myArg->directory);
    dir_temp[strlen(dir_temp)-1]='\0';

    handleSyncGetRequests(fd,dir_temp);


    // TODO
    //clear and rebuild root

    // freeFileTree(root);


    treeRoot[0]='\0';
    sprintf(treeRoot,"%s%s",myArg->directory,myArg->fileName);
    (*root) = createNode(myArg->fileName, 0, 0, FOLDER_NODE,0);
    buildTree(treeRoot, (*root));

}

