
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
#include "lockManager.h"


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


void sendCreateFilesWrapper(char *path, char *relativePath,struct FileNode* root, int *fd ){
    char buffer[BUFFER_SIZE];
    buffer[0] = '\0';

    sprintf(buffer,"{SYNC_CREATE}");
    if(write(*fd,buffer,strlen(buffer)) == -1){
        perror("write");
        exit(EXIT_FAILURE);
    }

    sendCreateFiles(path, relativePath,root,fd);
    
    sprintf(buffer,"{SYNC_CREATE_DONE}");
    if(write(*fd,buffer,strlen(buffer)) == -1){
        perror("write");
        exit(EXIT_FAILURE);
    }
}

void sendCreateFiles(char *path, char *relativePath,struct FileNode* root, int *fd ){
    if(root->flag == 1){
        char fullPath[strlen(relativePath) + strlen(root->name) + 2];
        fullPath[0]='\0';

        sprintf(fullPath,"%s%s",path,relativePath);
        updateNode(fullPath, root->type, fd,relativePath);
        root->flag=0;
    }

    for(int i=0; i<root->childirenSize; i++){
        char relativePath2[strlen(relativePath) + strlen(root->children[i].name) + 2];
        snprintf(relativePath2, sizeof(relativePath2), "%s/%s", relativePath, root->children[i].name);
        sendCreateFiles(path,relativePath2,&(root->children[i]),fd);
    }
}


void sendDeletedFilesAndFreeWrapper(char *relativePath,struct FileNode** root, int *fd ){

    char buffer[BUFFER_SIZE];
    buffer[0] = '\0';

    sprintf(buffer,"{SYNC_DELETE}");
    if(write(*fd,buffer,strlen(buffer)) == -1){
        perror("write");
        exit(EXIT_FAILURE);
    }

    (*root)->flag = 1;
    sendDeletedFilesAndFree(relativePath,root,fd);
    
    sprintf(buffer,"{SYNC_DELETE_DONE}");
    if(write(*fd,buffer,strlen(buffer)) == -1){
        perror("write");
        exit(EXIT_FAILURE);
    }
}

void sendDeletedFilesAndFree(char *relativePath,struct FileNode** root, int *fd ){
    // delete root FileNode recursively and send relativePath if root.isDeleted == 1
    if((*root)->flag == 0){
        sendFileNameForSync(fd,relativePath);
    }

    for(int i=0; i<(*root)->childirenSize; i++){
        char relativePath2[strlen(relativePath) + strlen((*root)->children[i].name) + 2];
        snprintf(relativePath2, sizeof(relativePath2), "%s/%s", relativePath, (*root)->children[i].name);
        struct FileNode* child = &(((*root)->children[i]));
        sendDeletedFilesAndFree(relativePath2,&child,fd);
    }
    if((*root)->childirenSize>0){
        free((*root)->children);
    }
    // free((*root));
}

void sendFileNameForSync(int *fd,char *relativePath){
    char buffer[strlen(relativePath)+2];
    buffer[0] = '\0';

    sprintf(buffer,"{%s}",relativePath);
    if(write(*fd,buffer,strlen(buffer)) == -1){
        perror("write");
        exit(EXIT_FAILURE);
    }
}

// void printTree(struct FileNode* node, int level) {
//     for (int i = 0; i < level; i++) {
//         printf("  ");
//     }
//     if (node->type == FOLDER_NODE) {
//         printf("[%s] [%d]|\n", node->name,node->childirenSize);
//     } else {
//         char* timeString = ctime(&(node->lastModifiedTime));
//         size_t length = strlen(timeString);
//         timeString[length - 1] = '\0'; //remove new line
//         printf("%s (Size: %ld, Last Modified: %s)\n", node->name, node->size, timeString);
//     }
//     for (int i = 0; i < node->childirenSize; i++) {
//         printTree(&(node->children[i]), level + 1);
//     }
// }

void buildTreeForCheckAndSendUpdates(char* path,char *relativePath, struct FileNode* parentNode, 
                    struct FileNode *oldRoot, int *fd) {
    
    DIR* rootFile = opendir(path);
    if (rootFile == NULL) {
        perror("opendir");
        return;
    }

    struct dirent* dirFile;
    while ((dirFile = readdir(rootFile)) != NULL) { //a //b

        if (strcmp(dirFile->d_name, ".") == 0 || strcmp(dirFile->d_name, "..") == 0) {
            continue;
        }

        char relativePath2[strlen(relativePath) + strlen(dirFile->d_name) + 2];
        snprintf(relativePath2, sizeof(relativePath2), "%s/%s", relativePath, dirFile->d_name);

        char childPath[strlen(path) + strlen(dirFile->d_name) + 2];
        snprintf(childPath, sizeof(childPath), "%s/%s", path, dirFile->d_name);
        
        struct stat st;
        if (stat(childPath, &st) == -1) {
            perror("stat");
            continue;
        }

        enum NodeType type;

        if(S_ISDIR(st.st_mode)){
            type = FOLDER_NODE;
        }
        else{
            type = FILE_NODE;
        }

        struct FileNode* childNode = createNode(dirFile->d_name, st.st_size, st.st_mtime, type,0);


        struct FileNode *node = NULL;

        int flag=0;
        if(oldRoot!=NULL){
            for(int i=0; i<oldRoot->childirenSize;i++){
                if(strcmp(oldRoot->children[i].name,childNode->name)==0){
                    flag=1;
                    node = &oldRoot->children[i];
                    oldRoot->children[i].flag = 1;
                    if( !(oldRoot->children[i].lastModifiedTime == childNode->lastModifiedTime) ){
                        if(type==FILE_NODE){
                            updateNode(childPath, type, fd, relativePath2);
                        }
                    }
                }
            }
        }
        
        if(oldRoot==NULL || flag==0){
            childNode->flag=1;
        }

        if (type == FOLDER_NODE) {
            buildTreeForCheckAndSendUpdates(childPath, relativePath2, childNode, node, fd);
        }
        addChild(parentNode, childNode);
    }

    closedir(rootFile);
}

void updateNode(char *childPath, enum NodeType type, int *fd, char *serverRelativePath){

    char buffer[BUFFER_SIZE];
    buffer[0]='\0';

    if(type == FOLDER_NODE){
        sprintf(buffer, "{SYNC_FOLDER}");
    }
    else{
        sprintf(buffer, "{SYNC_FILE}");
    }

    if(write((*fd), buffer, strlen(buffer)) == ERROR){
        perror("write error ");
        return;
    }

    char filePath[strlen(serverRelativePath)+4];
    filePath[0]='\0';
    sprintf(filePath, "{%s}", serverRelativePath); // TODO
    if(write((*fd), filePath, strlen(filePath)) == ERROR){
        perror("write error ");
        return;
    }
    if(type==FILE_NODE){
        readFromFileWriteToSocket(childPath, fd);
    }

    buffer[0]='\0';
    sprintf(buffer, "{SYNC_DONE}");
    if(write((*fd), buffer, strlen(buffer)) == ERROR){
        perror("write error ");
        return;
    }

}

void readFromFileWriteToSocket(char *childPath, int *fd){
    int childFd = open(childPath, O_RDWR);
    if(childFd == ERROR){
        perror("open error ");
        return;
    }

    char readBuffer[RECEIVE_BUFFER_SIZE];
    readBuffer[0]='\0';
    int readSize;

    while ((readSize = read(childFd, readBuffer, RECEIVE_BUFFER_SIZE)) >= 0) {
        readBuffer[readSize]='\0';
        if (readSize == 0) {
            readBuffer[0] = '\0';
            sprintf(readBuffer, "{EMPTY_FILE}");
            //printf("************ READ BUFFER : %s ***************** \n",readBuffer);
            if (write((*fd), readBuffer, strlen(readBuffer)) < 0) {
                perror("Error writing to file descriptor");
                exit(EXIT_FAILURE);
            }
            break;
        }

        if (write((*fd), readBuffer, readSize) < 0) {
            perror("Error writing to file descriptor");
            exit(EXIT_FAILURE);
        }
        if(readSize<RECEIVE_BUFFER_SIZE){
            break;
        }
    }
    close(childFd);
}

