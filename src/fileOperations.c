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
#include <errno.h>
#include <string.h>

#include "fileOperations.h"
#include "syncFileServer.h"
#include "commonSyncFile.h"
#include "fileTree.h"
#include "common.h"

#define ERROR -1
#define BUFFER_SIZE 512
#define FOLDER_EXIST 1
#define FOLDER_NOT_EXIST 0
#define FILE_EXIST 1
#define FILE_NOT_EXIST 0

// Sync received path files to server side. 
void syncFile(char *path, int *fd,int fileType,char *directory,int isClient) {
    
    char serverFilePath [strlen(directory)+strlen(path)+1];
    serverFilePath[0]='\0';
    char serverPath [strlen(directory)+strlen(path)+1];
    serverPath[0]='\0';
    sprintf(serverPath,"%s",directory);
    if(serverPath[strlen(serverPath)-1]=='/'){
        serverPath[strlen(serverPath)-1] = '\0';
    }
    sprintf(serverFilePath,"%s%s",serverPath,path);

    int fileFd;

    if (fileType == FILE_NODE) {
        //file case
        if(fileExists(serverFilePath)){

            if(isClient==1){
                return;
            }

            renameFileWithUnique(serverFilePath);
        }
        
        (fileFd) = open(serverFilePath, O_WRONLY | O_CREAT | O_EXCL, 0644);

        if (fileFd == ERROR) {
            perror("Failed to create file");
            exit(EXIT_FAILURE);
        }

        readAndWriteFileFromFd(path,fd, &fileFd);

        close(fileFd);

    }
    else {

        //folder case
        if (!folderExists(serverFilePath)){
            
            if (mkdir(serverFilePath, 0777 | S_IFDIR) == ERROR) {
                if (errno == EACCES) {
                    perror("Please run your terminal in sudo mode in order to server to create folder");
                } 
                perror("Failed to create folder");
            }
        }
    }
}


void readAndWriteFileFromFd(char *path, int *fd, int *fileFd){


    char buffer[strlen(path) + BUFFER_SIZE];
    buffer[0] = '\0';
    char readBuffer[BUFFER_SIZE];

    sprintf(buffer, "{%s}", path);

    if (write((*fd), buffer, strlen(buffer))<0){
        perror("Failed to write to socket");
        return;
    }

    int dataRead = 0;
    while ((dataRead = read((*fd), readBuffer, BUFFER_SIZE)) > 0) {
        readBuffer[dataRead] = '\0';
        if(strcmp(readBuffer,"{EMPTY_FILE}")==0){
            break;
        }

        if (write((*fileFd), readBuffer, strlen(readBuffer))<0){
            perror("Failed to write to socket");
            return;
        }

        if(dataRead!=BUFFER_SIZE || readBuffer[dataRead-1]=='\0'){
            break;
        }
    }
}


int fileExists(char* path) {
    int fileFd = open(path, O_RDONLY);
    if (fileFd == ERROR) {
        if (errno != ENOENT) {
            perror("Failed to open file");
            return 0;
        }
    } else {
        close(fileFd);
        return FILE_EXIST;
    }
    return FILE_NOT_EXIST;
}

int folderExists(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return FOLDER_EXIST;
    }
    return FOLDER_NOT_EXIST;
}

void renameFileWithUnique(char* originalFileName) {
    time_t currentTime = time(NULL);
    struct tm* timeInfo = localtime(&currentTime);
    char timestamp[100];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", timeInfo);

    char newFileName[strlen(originalFileName) + strlen(timestamp) + 2];
    snprintf(newFileName, sizeof(newFileName), "%s_%s", originalFileName, timestamp);

    int result = rename(originalFileName, newFileName);
    if (result != 0) {
        perror("Error renaming file");
    }
}

