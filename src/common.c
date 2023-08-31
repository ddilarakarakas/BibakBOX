#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "common.h"
#include "fileTree.h"

#define ERROR -1
#define BUFFER_SIZE 1024

char errorBufffer[BUFFER_SIZE];


/*This function using as printf if
    something unexpected happens.*/
void printError (char *message){   
    write (STDERR_FILENO, message, strlen(message));
}

/*This function using instead of printf.*/
void printScreen(char *message){
    int error=0;
    char line[BUFFER_SIZE];

    sprintf(line,"%s\n",message);

    error=write (STDOUT_FILENO, line, strlen(line));
    if(error==ERROR){
        sprintf(line,"Error while write operation:%s\n",strerror(error));
        printError(line);
        exit(EXIT_FAILURE);
    }
}

void blockSIGINT(sigset_t* mask, sigset_t* oldmask){
    /*If ctrl+c comes while cleaning process it may create problem so after here
        process ignores ctrl+c.*/
    int error=sigprocmask(SIG_BLOCK,mask,oldmask);
    if(error==ERROR){
        sprintf(errorBufffer,"Error while blocking SIGINT with sigprocmask.\n");
        printError(errorBufffer);
        exit(EXIT_FAILURE);
    }

}

void unblockSIGINT(sigset_t* mask, sigset_t* oldmask){
    int error=sigprocmask(SIG_SETMASK,oldmask,mask);
    if(error==ERROR){
        sprintf(errorBufffer,"Error while unblocking SIGINT with sigprocmask.\n");
        printError(errorBufffer);
        exit(EXIT_FAILURE);
    }
}

void initializeSIGBlockParams(sigset_t* mask, sigset_t* oldmask){
    /*For blocking SIGINT in critical sections.*/
    sigemptyset(mask);
    sigaddset(mask,SIGINT);
    sigemptyset(oldmask);
}

//return ne size
int reallocateMemory(char **buffer, int size) {
    char* c = realloc(*buffer, (size*2));
    if (c == NULL) return size;

    (*buffer) = c;
    return size * 2;
}


void freeFileTree(struct FileNode** root) {

    if ((*root) == NULL) return;
    
    for (int i = 0; i < (*root)->childirenSize-1; i++) {
        struct FileNode* child = &(((*root)->children[i]));
        freeFileTree(&child);
    }

    free((*root)->children);
    free((*root));
    (*root)=NULL;
}



void freeFileTree2(struct FileNode** root) {

    if ((*root) == NULL) return;
    
    for (int i = 0; i < (*root)->size; i++) {
        struct FileNode* child = &(((*root)->children[i]));
        freeFileTree(&child);
    }

    if((*root)->children!=NULL){
        free((*root)->children);
    }
    free((*root));
    (*root)=NULL;
}



