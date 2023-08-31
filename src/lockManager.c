#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "lockManager.h"

struct DynamicArray mutexArr; 
pthread_mutex_t pathListMutex;



void takeLock(char* path){
    int flag = 0;
    int i;
    pthread_mutex_lock(&pathListMutex);
    for(i = 0;i < mutexArr.size;i++){
        if(strcmp(mutexArr.array[i].path, path) == 0){
            flag = 1;
            pthread_mutex_unlock(&pathListMutex);
            pthread_mutex_lock(&(mutexArr.array[i].mutex));
            break;
        }
    }
    if(flag != 1){
        addItem(path);
        lockItemMutex(path);
        pthread_mutex_unlock(&pathListMutex);
    }
}


void initArray() {
    mutexArr.size = 0;
    mutexArr.capacity = 1;
    mutexArr.array = (struct Item*)malloc(mutexArr.capacity * sizeof(struct Item));
}

void destroyArray() {
    for (int i = 0; i < mutexArr.size; i++) {
        free(mutexArr.array[i].path);
    }
    free(mutexArr.array);
}

void addItem(char* path) {
    if (mutexArr.size == mutexArr.capacity-1) {
        mutexArr.capacity *= 2;
        mutexArr.array = (struct Item*)realloc(mutexArr.array, mutexArr.capacity * sizeof(struct Item));
    }

    struct Item* newItem = &(mutexArr.array[mutexArr.size]);
    newItem->path = strdup(path);  // Duplicate the string
    pthread_mutex_init(&(newItem->mutex), NULL);

    mutexArr.size++;
}

void lockItemMutex(char* path) {
    int i;
    for (i = 0; i < mutexArr.size; i++) {
        if (strcmp(mutexArr.array[i].path, path) == 0) {
            pthread_mutex_lock(&(mutexArr.array[i].mutex));
            break;
        }
    }
}

void unlockLock(char* path) {
    int i;
    for (i = 0; i < mutexArr.size; i++) {
        if (strcmp(mutexArr.array[i].path, path) == 0) {
            pthread_mutex_unlock(&(mutexArr.array[i].mutex));
            break;
        }
    }
}
