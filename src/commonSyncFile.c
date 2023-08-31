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

#include "commonSyncFile.h"
#include "fileTree.h"
#include "common.h"
#include "fileOperations.h"
#include "serverThread.h"
#include "syncFileServer.h"

#define BUFFER_SIZE 512
#define RECEIVE_BUFFER_SIZE 2048
#define MAX_FORMAT_LENGTH 2048

void sendSyncFileWrapper(struct FileNode* node, int *fd) {

    char buffer[20];  // Adjust the buffer size as needed
    buffer[0] = '\0';
    sprintf(buffer, "{SYNC}");
    write((*fd), buffer, strlen(buffer));

    sendSyncFile(node,fd,"");

    buffer[0] = '\0';  // Initialize the buffer as an empty string
    sprintf(buffer, "{SYNC_DONE}");
    write((*fd), buffer, strlen(buffer));
}

void handleSyncGetRequests(int *fd,char *directory){

    int bufferSize=256;
    char *buffer = (char *)malloc(sizeof(char)*bufferSize);
    buffer[0]='\0';
    int bufferIndex=0;

    char currentChar;

    while (read(*fd, &currentChar, sizeof(char)) > 0) { 
        if(currentChar=='{'){
            continue;
        }
        else if(currentChar=='}'){

            if (strcmp(buffer, "SYNC_WRITE_DONE") == 0) {
                free(buffer);
                return;
            }

            char filePath[strlen(directory)+strlen(buffer)+1];
            filePath[0]='\0';

            sprintf(filePath,"%s%s",directory,buffer);

            // printf("OPENING file: |%s| |%s| |%s|\n",filePath,directory,buffer);
            // TODO
            //  test edilecek
            if(strlen(filePath)>254){
                printf("File path too long. Skipping file %s.\n",filePath);
                continue;
            }
            int file_fd = open(filePath, O_RDWR);
            if (file_fd < 0) {
                free(buffer);
                perror("Error opening filee");
                exit(EXIT_FAILURE);
            }
            // printf("OPENED to file: %s\n",filePath);
            //read from file_fd and write it to fd
            char readBuffer[RECEIVE_BUFFER_SIZE];
            readBuffer[0]='\0';
            int readSize;
            while ((readSize = read(file_fd, readBuffer, RECEIVE_BUFFER_SIZE)) >= 0) {
                
                readBuffer[readSize]='\0';
                if (readSize == 0) {
                    readBuffer[0] = '\0';
                    sprintf(readBuffer, "{EMPTY_FILE}");
                    if (write((*fd), readBuffer, strlen(readBuffer)) < 0) {
                        perror("Error writing to file descriptor");
                        exit(EXIT_FAILURE);
                    }
                    buffer[0]='\0';
                    bufferIndex=0;
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

            close(file_fd);
            buffer[0]='\0';
            bufferIndex=0;
        }else{

            if(currentChar=='\0'){
                continue;
            }
            buffer[bufferIndex++] = currentChar;
            buffer[bufferIndex]='\0';
            if(bufferIndex == bufferSize-1){
                bufferSize = reallocateMemory(&buffer,bufferSize);
            }
        }

    }
    perror("Error reading from file descriptor");
    exit(EXIT_FAILURE);
}

struct FileNodeList* receiveSyncFiles(int *fd){

    int bufferSize=256;
    char *buffer = (char *)malloc(sizeof(char)*bufferSize);
    buffer[0]='\0';
    int bufferIndex=0;
    long fileSize;
    int fileType;
    char currentChar;

    struct FileNodeList* fileList = (struct FileNodeList*)malloc(sizeof(struct FileNodeList));
    fileList->list = (struct FileNode**)malloc(sizeof(struct FileNode*)*8);
    fileList->size = 8;
    fileList->currentIndex = 0;

    while (read(*fd, &currentChar, sizeof(char)) > 0) {
        if(currentChar=='{'){
            continue;
        }
        else if(currentChar=='}'){

            if (strcmp(buffer, "SYNC_DONE") == 0) {
                free(buffer);
                return fileList;
            }

            char newPath[strlen(buffer)];
            newPath[0]='\0';

            sscanf(buffer, "%[^,],%ld,%d", newPath, &fileSize, &fileType);
            buffer[0]='\0';
            bufferIndex=0;

            struct FileNode* newNode = createNode(newPath, fileSize, 0, fileType,0);
            if (fileList->currentIndex == fileList->size) {
                fileList->size *= 2;
                fileList->list = (struct FileNode**)realloc(fileList->list, sizeof(struct FileNode*) * fileList->size);
            }
            fileList->list[fileList->currentIndex++] = newNode;
        }else{
            buffer[bufferIndex++] = currentChar;
            buffer[bufferIndex]='\0';
            if(bufferIndex == bufferSize-1){
                bufferSize = reallocateMemory(&buffer,bufferSize);
            }
        }
    }
    perror("Error reading from file descriptor");
    exit(1);
}


void syncServerWithfd(int *fd,char *directory,struct FileNodeList* fileList,int isClient){
    //sync client files
    for(int i=0;i<fileList->currentIndex;i++){
        syncFile(fileList->list[i]->name,fd,fileList->list[i]->type,directory,isClient);
    }

    char buffer[20];  // Adjust the buffer size as needed
    buffer[0] = '\0';
    sprintf(buffer, "{SYNC_WRITE_DONE}");
    if(write((*fd), buffer, strlen(buffer))<0){
        perror("Error writing to file descriptor");
        exit(1);
    }
}

void sendSyncFile(struct FileNode* node, int *fd,char *path) {
    if (node == NULL) {
        return;
    }
    
    int lenPath = strlen(path);
    int lenName = strlen(node->name);
    char *newPath = (char*)malloc(sizeof(char)*(lenPath+lenName+2));
    newPath[0]='\0';
    sprintf(newPath, "%s/%s", path, node->name);

    char buffer[BUFFER_SIZE + strlen(newPath)];
    buffer[0]='\0';

    sprintf(buffer,"{%s,%ld,%d}", newPath, node->size, node->type); 
    write((*fd), buffer, strlen(buffer));

    for (int i = 0; i < node->childirenSize; i++) {
        sendSyncFile(&(node->children[i]),fd,newPath);
    }

    free(newPath);
}