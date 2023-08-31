# BibakBOX - A Simple Dropbox-like Application

This project aims to implement a simplified version of Dropbox, functioning as a multi-threaded internet server capable of handling multiple clients simultaneously. Upon establishing a connection with the server, the directories on both the server and client sides should be synchronized. This means that any new file created, deleted, or updated on the server should be reflected with the same changes on the client side, and vice versa.

Unlike the official Dropbox service, the server in this case should also maintain a log file under the respective client's directory. This log file should record the names and access times of created, deleted, and updated files. Additionally, handling the SIGINT signal on both the server and client sides is important.

## Server Usage

The usage for the server should be as follows:

```bash
BibakBOXServer [directory] [threadPoolSize] [portNumber]
```

- `directory`: The server's specific area for file operations.
- `threadPoolSize`: The maximum number of threads active at a time.
- `portNumber`: The port the server will wait for connections on.

When the server is executed, it should prompt a message to the screen when a client connection is accepted, including the connection address.

## Client Usage

The example call from the client might be in the following format:

```bash
BibakBOXClient [dirName] [portNumber]
```

- `dirName`: The name of the directory on the server side.
- `portNumber`: The connection port of the server.

The client should return with a proper message when the server is stopped and, when a client connection is accepted, the server should prompt a message to the screen including the connection address.
