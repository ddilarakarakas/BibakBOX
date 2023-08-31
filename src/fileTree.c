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

#include "fileTree.h"

#define PATH_BUFFER_SIZE 4096


struct FileNode* createNode(const char* name, long long fileSize, time_t lastModified, enum NodeType type,int flag) {
    struct FileNode* newNode = (struct FileNode*)malloc(sizeof(struct FileNode));
    strncpy(newNode->name, name, sizeof(newNode->name));
    newNode->children = NULL;
    newNode->childirenSize = 0;
    newNode->size = fileSize;
    newNode->lastModifiedTime = lastModified;
    newNode->type = type;
    newNode->flag = flag;
    return newNode;
}

void buildTree(char* path, struct FileNode* parentNode) {
    DIR* rootFile = opendir(path);
    if (rootFile == NULL) {
        perror("opendir");
        return;
    }

    struct dirent* dirFile;
    while ((dirFile = readdir(rootFile)) != NULL) {
        //ignore . and ..
        if (strcmp(dirFile->d_name, ".") == 0 || strcmp(dirFile->d_name, "..") == 0 || strcmp(dirFile->d_name, "log.txt") == 0) {
            continue;
        }

        //take child path
        char childPath[strlen(path) + strlen(dirFile->d_name) + 2];
        snprintf(childPath, sizeof(childPath), "%s/%s", path, dirFile->d_name);
        
        //get child info
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

        addChild(parentNode, childNode);

        if (type == FOLDER_NODE) {
            buildTree(childPath, &parentNode->children[parentNode->childirenSize - 1]);
        }
    }

    closedir(rootFile);
}

void addChild(struct FileNode* p, struct FileNode* c) {
    p->children = (struct FileNode*)realloc(p->children, (p->childirenSize + 1) * sizeof(struct FileNode));
    p->children[p->childirenSize] = *c;
    p->childirenSize++;
}

char* getNameFromPath(char* path){
    int length = strlen(path);
    int end = length - 1;

    while (end >= 0 && path[end] == '/') {
        end--;
    }

    int start = end;
    while (start >= 0 && path[start] != '/') {
        start--;
    }

    if (start < 0) {
        char* fullPath = malloc((length + 1) * sizeof(char));
        strcpy(fullPath, path);
        return fullPath;
    }

    int childSize = end - start;
    
    char* lastName = malloc((childSize + 1) * sizeof(char));

    strncpy(lastName, path + start + 1, childSize);
    lastName[childSize] = '\0';

    return lastName;
}

struct FileNode* copyFileTree(const struct FileNode* node) {
    struct FileNode* copy = (struct FileNode*)malloc(sizeof(struct FileNode));

    strcpy(copy->name, node->name);
    copy->childirenSize = node->childirenSize;
    copy->size = node->size;
    copy->lastModifiedTime = node->lastModifiedTime;
    copy->type = node->type;
    copy->flag = node->flag;

    if (node->type == FOLDER_NODE) {
        copy->children = (struct FileNode*)malloc(node->childirenSize * sizeof(struct FileNode));
        if (copy->children == NULL) {
            perror("Error allocating memory for children");
            free(copy);
            return NULL;
        }

        for (int i = 0; i < node->childirenSize; i++) {
            struct FileNode* child = copyFileTree(&(node->children[i]));
            copy->children[i] = *child;
        }
    } else {
        copy->children = NULL;
    }

    return copy;
}



// void freeFileTree(struct FileNode* root) {
//     if (root == NULL) {
//         return;
//     }
    
//     for (int i = 0; i < root->childirenSize; i++) {
//         freeFileTree(root->children[i]);
//     }

//     free(root);
// }




// char* getRootPath(char* path) {
//     if (path == NULL)
//         return NULL;

//     int pathSize = strlen(path);
//     int i;
//     for (i = 0; i < pathSize; i++) {
//         if (path[i] == '/')
//             break;
//     }

//     if (i == pathSize) {
//         // The path doesn't contain any '/'
//         return NULL;
//     }

//     // Calculate the length of the root path
//     int rootSize = i;

//     // Allocate memory for the root path
//     char* root = (char*)malloc((rootSize + 1) * sizeof(char));
//     if (root == NULL) {
//         printf("Memory allocation failed.\n");
//         return NULL;
//     }

//     // Copy the root path into the new string
//     strncpy(root, path, rootSize);
//     root[rootSize] = '\0';

//     // Create a new string for the child path (excluding the root)
//     char* child = strdup(path + i);

//     // Recursive call to get the root of the child path
//     char* childRoot = getRootPath(child);

//     // Free memory allocated for the child path
//     free(child);

//     if (childRoot == NULL) {
//         // If the child path doesn't have a root, return the current root
//         return root;
//     } else {
//         // If the child path has a root, free memory allocated for the current root
//         free(root);
//         return childRoot;
//     }
// }



















