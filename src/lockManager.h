
#ifndef LOCK_MANAGER_H_
#define LOCK_MANAGER_H_

struct Item {
    char* path;
    pthread_mutex_t mutex;
};

struct DynamicArray{
    struct Item* array;
    int size;
    int capacity;
};



extern struct DynamicArray mutexArr; 
extern pthread_mutex_t pathListMutex;

void initArray(void);
void destroyArray(void);
void addItem(char* path);
void lockItemMutex(char* path);
void unlockLock(char* path);

void takeLock(char* path);

#endif 
