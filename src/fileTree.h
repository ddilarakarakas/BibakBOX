
#ifndef FILE_TREE_H
#define FILE_TREE_H


#define NAME_BUFFER_LENGH 2048

enum NodeType {
    FILE_NODE,
    FOLDER_NODE
};

struct FileNode {
    char name[NAME_BUFFER_LENGH];
    struct FileNode* children;
    int childirenSize;
    long size;
    time_t lastModifiedTime;
    enum NodeType type;
    int flag; // used only for deleted files
};

struct FileNodeList {
    struct FileNode** list;
    int size;
    int currentIndex;
};

// void printTree(struct FileNode* node, int level);
struct FileNode* createNode(const char* name, long long fileSize, time_t lastModified, enum NodeType type,int flag);
void buildTree(char* path, struct FileNode* parentNode);
char* getNameFromPath(char* path);
void addChild(struct FileNode* p, struct FileNode* c);
struct FileNode* copyFileTree(const struct FileNode* node);
#endif
