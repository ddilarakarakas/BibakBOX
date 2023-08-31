
CFLAGS = -Wall -Wextra -lm -pthread

all:
	gcc -c BibakBOXServer.c common.c serverThread.c fileTree.c syncFileServer.c commonSyncFile.c fileOperations.c syncCheckSendServer.c syncCheckSendCommon.c syncCheckReceive.c lockManager.c $(CFLAGS)
	gcc BibakBOXServer.o common.o serverThread.o fileTree.o syncFileServer.o commonSyncFile.o fileOperations.o syncCheckSendServer.o syncCheckSendCommon.o syncCheckReceive.o lockManager.o $(CFLAGS) -o BibakBOXServer

	gcc -c BibakBOXClient.c common.c fileTree.c syncFileServer.c commonSyncFile.c fileOperations.c syncCheckSendClient.c syncCheckSendCommon.c  syncCheckReceive.c $(CFLAGS)
	gcc BibakBOXClient.o  common.o fileTree.o syncFileServer.o commonSyncFile.o fileOperations.o syncCheckSendClient.o syncCheckSendCommon.o syncCheckReceive.o $(CFLAGS) -o BibakBOXClient

clean:
	rm -f BibakBOXClient BibakBOXServer *.o
