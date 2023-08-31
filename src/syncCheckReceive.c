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
#include "syncCheckReceive.h"

#define BUFFER_SIZE 256
#define ERROR -1
#define RECEIVE_BUFFER_SIZE 2048
#define LOG_BUFFER_SIZE 2048
#define TIME_BUFFER_SIZE 1024

void syncCheckReceive(int *fd, char *dirname, char *rootName,int *logfd){

    int bufferSize=256;
    char *buffer = (char *)malloc(sizeof(char)*bufferSize);
    buffer[0]='\0';
    int bufferIndex=0;

    char readChar;

    while ( (read((*fd),&readChar,1)) > 0 ){
        if(readChar=='{'){
            continue;
        }
        else if(readChar=='}'){

            if (strcmp(buffer, "SYNC_FILE") == 0) {
                printf("Syncing modified files.\n");
                char fullDir[strlen(dirname) + strlen(rootName) + 2];
                fullDir[0]='\0';
                sprintf(fullDir,"%s/%s",dirname,rootName);
                printf("File updated. Path : %s\n",fullDir);

                syncReceiveFile(fd,fullDir,logfd);
                buffer[0]='\0';
                bufferIndex=0;
            }
            else if(strcmp(buffer,"SYNC_UPDATE_DONE")==0){
                buffer[0]='\0';
                bufferIndex=0;
                continue;
            }
            else if(strcmp(buffer,"SYNC_DELETE")==0){
                //printf("Syncing deleted files.\n");
                
                char fullDir[strlen(dirname) + strlen(rootName) + 2];
                fullDir[0]='\0';

                if(dirname[strlen(dirname)-1]=='/' || rootName[0]=='/'){
                    sprintf(fullDir,"%s%s",dirname,rootName);
                }
                else{
                    sprintf(fullDir,"%s/%s",dirname,rootName);
                }
                
                deleteReceiveFile(fd,fullDir,logfd);

                buffer[0]='\0';
                bufferIndex=0;

            }
            else if(strcmp(buffer,"SYNC_DELETE_DONE")==0){
                buffer[0]='\0';
                bufferIndex=0;
                continue;
            }
            else if(strcmp(buffer,"SYNC_CREATE")==0){
                //printf("Syncing created files.\n");

                char fullDir[strlen(dirname) + strlen(rootName) + 2];
                fullDir[0]='\0';
            
                if(dirname[strlen(dirname)-1]=='/' || rootName[0]=='/'){
                    sprintf(fullDir,"%s%s",dirname,rootName);
                }
                else{
                    sprintf(fullDir,"%s/%s",dirname,rootName);
                }

                // sprintf(fullDir,"%s/%s",dirname,rootName);

                createReceiveFile(fd,fullDir,logfd);
                buffer[0]='\0';
                bufferIndex=0;
            }
            else if(strcmp(buffer,"SYNC_CREATE_DONE")==0){
                buffer[0]='\0';
                bufferIndex=0;
            }
            else if(strcmp(buffer,"DONE_CONTROL")==0){
                free(buffer);
                break;
            }

        }else{
            buffer[bufferIndex++] = readChar;
            buffer[bufferIndex]='\0';
            if(bufferIndex == bufferSize-1){
                bufferSize = reallocateMemory(&buffer,bufferSize);
            }
        }
    }
}

void syncReceiveFile(int *fd,char *directory,int *logfd){
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

            if(strcmp(buffer,"SYNC_UPDATE_DONE")==0){
                free(buffer);
                return;
            }

            char filePath[strlen(directory)+strlen(buffer)+1];
            filePath[0]='\0';
            sprintf(filePath,"%s%s",directory,buffer);

            // TODO
            //  test edilecek
            if(strlen(filePath)>254){
                printf("File path too long. Skipping file %s.\n",filePath);
                continue;
            }

            int file_fd = open(filePath, O_RDWR);
            if (file_fd < 0) {
                free(buffer);
                perror("Error opening fileee");
                exit(EXIT_FAILURE);
            }
            
            // erase all file content
            int status = ftruncate(file_fd, 0);
            if (status == -1) {
                perror("Error truncating the file");
                close(file_fd);
                exit(EXIT_FAILURE);
            }

            if(logfd!=NULL){
                //lock buffer
                char logBuffer[LOG_BUFFER_SIZE];
                logBuffer[0]='\0';
                //timestamp for logging
                time_t t = time(NULL);
                struct tm tm = *localtime(&t);
                char timeBuffer[TIME_BUFFER_SIZE];
                timeBuffer[0]='\0';
                sprintf(timeBuffer, "%d-%d-%d %d:%d:%d ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

                sprintf(logBuffer,"Client: UPDATED NODE : %s | ACCESS_TIME: %s \n", buffer,timeBuffer);
                if(write((*logfd), logBuffer, strlen(logBuffer)) == ERROR){
                    perror("write error ");
                    return;
                }
            }


            readSocketWriteFile(fd,&file_fd);

            close(file_fd);
            buffer[0]='\0';
            bufferIndex=0;
            free(buffer);
            return;
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
}

//read file instance and write to server file with socket
void readSocketWriteFile(int *fd,int *file_fd){
    int bufferSize2=256;
    char *buffer2 = (char *)malloc(sizeof(char)*bufferSize2);
    buffer2[0]='\0';
    int bufferIndex2=0;
    char readChar2;
    int flag=0;
    int dataRead2=0;
    while ((dataRead2 = read((*fd), &readChar2, 1)) > 0) {
        if(readChar2=='{'){

            if(flag==1){
                char par = '{';
                if(write((*file_fd),&par,1)<0){
                    perror("Failed to write to socket");
                    return;
                }
                flag=0;
            }
            
            if(bufferIndex2==0) continue;

            if (write((*file_fd), buffer2, strlen(buffer2))<0){
                perror("Failed to write to socket");
                return;
            }
            flag = 1;
            buffer2[0]='\0';
            bufferIndex2=0;
            continue;
        }
        else if(readChar2=='}'){

            if (strcmp(buffer2, "SYNC_DONE") == 0) {
                free(buffer2);
                return;
            }

            if (write((*file_fd), buffer2, strlen(buffer2))<0){
                perror("Failed to write to socket");
                return;
            }

            buffer2[0]='\0';
            bufferIndex2=0;
            continue;
        }else{
            buffer2[bufferIndex2++] = readChar2;
            buffer2[bufferIndex2]='\0';
            if(bufferIndex2 == bufferSize2-1){
                bufferSize2 = reallocateMemory(&buffer2,bufferSize2);
            }
        }
    }
}

void deleteReceiveFile(int *fd, char *directory,int *logfd){
    int bufferSize=256;
    char *buffer = (char *)malloc(sizeof(char)*bufferSize);
    buffer[0]='\0';
    int bufferIndex=0;

    char currentChar;
    while (read(*fd, &currentChar, sizeof(char)) > 0) { 
        //printf("deleteReceiveFile currentChar : %c\n",currentChar);
        if(currentChar=='{'){
            continue;
        }
        else if(currentChar=='}'){

            if(strcmp(buffer,"SYNC_DELETE_DONE")==0){
                free(buffer);
                return;
            }

            char filePath[strlen(directory)+strlen(buffer)+1];

            if(directory[strlen(directory)-1]=='/' && buffer[0]=='/'){
                char temp[strlen(directory)+1];
                temp[0]='\0';
                sprintf(temp,"%s",directory);
                temp[strlen(temp)-2]='\0';
                sprintf(filePath,"%s%s",directory,buffer);
            }
            else{
                filePath[0]='\0';
                sprintf(filePath,"%s%s",directory,buffer);
            }


            if(strlen(filePath)>254){
                printf("File path too long. Skipping file %s.\n",filePath);
                continue;
            }
            if (folderExists(filePath)){
                struct stat file_path_info;
                if(stat(filePath,&file_path_info) != 0){
                    perror("Failed to get path information");
                }

                if(logfd!=NULL){
                    //lock buffer
                    char logBuffer[LOG_BUFFER_SIZE];
                    logBuffer[0]='\0';
                    //timestamp for logging
                    time_t t = time(NULL);
                    struct tm tm = *localtime(&t);
                    char timeBuffer[TIME_BUFFER_SIZE];
                    timeBuffer[0]='\0';
                    sprintf(timeBuffer, "%d-%d-%d %d:%d:%d ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

                    sprintf(logBuffer,"Client: DELETED FOLDER NODE : %s | ACCESS_TIME: %s \n", buffer,timeBuffer);
                    if(write((*logfd), logBuffer, strlen(logBuffer)) == ERROR){
                        perror("write error ");
                        return;
                    }
                }

                
                if(S_ISDIR(file_path_info.st_mode)){
                    int result = remove_directory(filePath);
                    if (result == 0) {
                        printf("The directory and its contents have been deleted. Path : %s\n",filePath);
                    } else {
                        perror("Directory deletion failed");
                    }
                }
            }
            else if(fileExists(filePath)){

                if(logfd!=NULL){
                    //lock buffer
                    char logBuffer[LOG_BUFFER_SIZE];
                    logBuffer[0]='\0';
                    //timestamp for logging
                    time_t t = time(NULL);
                    struct tm tm = *localtime(&t);
                    char timeBuffer[TIME_BUFFER_SIZE];
                    timeBuffer[0]='\0';
                    sprintf(timeBuffer, "%d-%d-%d %d:%d:%d ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

                    sprintf(logBuffer,"Client: DELETED FILE NODE : %s | ACCESS_TIME: %s \n", buffer,timeBuffer);
                    if(write((*logfd), logBuffer, strlen(logBuffer)) == ERROR){
                        perror("write error ");
                        return;
                    }
                }


                if(unlink(filePath) == 0){
                        printf("File has been deleted. Path : %s\n",filePath);
                    }
                    else{
                        perror("File cannot delete");
                    }
            }
            
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
}

int remove_directory(char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("Could not open directory");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char file_path[strlen(path) + strlen(entry->d_name) + 2];
        file_path[0]='\0';
        sprintf(file_path, "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_DIR) {
            remove_directory(file_path);
        } else {
            if (unlink(file_path) == -1) {
                perror("File cannot delete");
                closedir(dir);
                return -1;
            }
        }
    }

    closedir(dir);
    
    if (rmdir(path) != 0) {
        perror("Could not delete directory");
        return -1;
    }

    return 0;
}

void createReceiveFile(int *fd, char *directory, int *logfd){
    int bufferSize=256;
    char *buffer = (char *)malloc(sizeof(char)*bufferSize);
    buffer[0]='\0';
    int bufferIndex=0;

    int folderFlag = 0;
    int fileFlag = 0;
    int contentCount = 0;

    char *pathBuffer = (char *)malloc(sizeof(char)*bufferSize);

    char currentChar;
    while (read(*fd, &currentChar, sizeof(char)) > 0) { 
        if(currentChar=='{'){
            //Write file
            if(contentCount == 1){
                char filePath[strlen(directory)+strlen(pathBuffer)+1];
                filePath[0]='\0';
                if(strcmp("EMPTY_FILE",buffer) != 0 && strcmp("EMPTY_FILE",pathBuffer) != 0){
                    sprintf(filePath,"%s%s",directory,pathBuffer);
                    //printf("directory : %s, pathBuffer : %s , buffer : %s\n , ", directory, pathBuffer,buffer); 

                    if(logfd!=NULL){
                        //lock buffer
                        char logBuffer[LOG_BUFFER_SIZE];
                        logBuffer[0]='\0';
                        //timestamp for logging
                        time_t t = time(NULL);
                        struct tm tm = *localtime(&t);
                        char timeBuffer[TIME_BUFFER_SIZE];
                        timeBuffer[0]='\0';
                        sprintf(timeBuffer, "%d-%d-%d %d:%d:%d ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

                        sprintf(logBuffer,"Client: CREATED NODE : %s | ACCESS_TIME: %s \n", pathBuffer,timeBuffer);
                        if(write((*logfd), logBuffer, strlen(logBuffer)) == ERROR){
                            perror("write error ");
                            return;
                        }
                    }

                    int newFileFd = open(filePath, O_RDWR | O_CREAT | O_TRUNC, 0777);
                    if(newFileFd == -1){
                        perror("File could not open");
                    }
                    if(strlen(buffer) != 0){
                        size_t contentSize = strlen(buffer);
                        if(write(newFileFd,buffer,contentSize) == -1){
                            perror("Could not write the file");
                            close(newFileFd);
                        }
                    }
                    close(newFileFd);
                    
                }
                
                contentCount = 0;
                buffer[0]='\0';
                bufferIndex=0;
                

            }
            continue;
        }
        else if(currentChar=='}'){
            if(strcmp(buffer,"SYNC_FOLDER") == 0){
                folderFlag = 1;
            }
            else if(strcmp(buffer,"SYNC_DONE") == 0 && folderFlag == 1){
                folderFlag = 0;
            }
            else if(folderFlag){
                char filePath[strlen(directory)+strlen(buffer)+1];
                filePath[0]='\0';
                sprintf(filePath,"%s%s",directory,buffer);
                if (!folderExists(filePath)){
                    if (mkdir(filePath, 0777 | S_IFDIR) == ERROR) {
                        if (errno == EACCES) {
                            perror("Please run your terminal in sudo mode in order to server to create folder");
                            exit(EXIT_SUCCESS);
                        } 
                        perror("Failed to create folder");
                    }
                    printf("Folder created. Path : %s\n",filePath);
                }
            }
            else if(strcmp(buffer,"SYNC_FILE") == 0){
                fileFlag = 1;
            }
            else if(strcmp(buffer,"SYNC_DONE") == 0 && fileFlag == 1){
                fileFlag = 0;
            }
            else if(fileFlag){
                if(contentCount == 0){
                    strcpy(pathBuffer, buffer);
                    contentCount = 1;
                }
            }
            else if(strcmp(buffer,"SYNC_CREATE_DONE")==0){
                free(buffer);
                free(pathBuffer);
                return;
            }
            
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
}