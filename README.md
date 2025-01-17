# Offline Messenger

## Overview

**Offline Messenger** is a client-server messaging application designed as an educational project. It demonstrates key concepts in network programming, database management, and concurrent client handling in a messaging system. The application allows users to exchange messages in real-time or offline while supporting replies to specific messages and viewing message history.

## Features

1. **Real-Time Messaging**:
   - Users connected to the server can exchange messages instantly.

2. **Offline Messaging**:
   - Messages sent to offline users are stored and delivered when they reconnect.

3. **Reply to Messages**:
   - Users can reply to specific messages in their chat history.

4. **Message History**:
   - Users can view their conversation history, including sent and received messages.

5. **Scalable Server**:
   - The server supports multiple clients simultaneously using multi-threading.

## Implementation Details

### 1. **Client-Server Communication**
- **Protocol**: Communication is implemented using TCP sockets.
- **Port**: The server listens on port `8086`.

### 2. **Database**
- **SQLite**:
  - The server uses an SQLite database (`mess.db`) to store:
    - User information.
    - Messages and metadata (e.g., sender, recipient, content).
    - Delivery statuses (e.g., `Pending`, `Delivered`).
  - SQL queries handle user validation, message storage, and retrieval.

### 3. **Core Components**
- **`client.c`**:
  - Handles client-side interactions, such as sending/receiving messages and viewing conversation history.
  - Users can terminate the session by typing `quit`.
- **`server.c`**:
  - Manages client connections and message delivery.
  - Uses threads to support multiple simultaneous clients.
  - Queues offline messages for later delivery.
- **`utils.h`**:
  - Provides utility functions and definitions, such as `Client` structure and message parsing functions.
- **`database.h`**:
  - Handles database operations like storing messages and retrieving user/message data.

### 4. **Client Structure**
Each client is represented as:
```c
typedef struct {
  int socket;
  int id;
  char username[64];
  int to_close;
} Client;
```

### 5. **Multi-threading**
- The server uses `pthread` to handle multiple clients concurrently, ensuring seamless operations like message delivery and history retrieval.

## How to Run

### Server
1. Compile the server code:
   ```bash
   gcc server.c -o server -lpthread -lsqlite3
   ```
2. Run the server:
   ```bash
   ./server
   ```

### Client
1. Compile the client code:
   ```bash
   gcc client.c -o client
   ```
2. Run the client:
   ```bash
   ./client
   ```

## Sample Usage

1. Start the server:
   ```bash
   ./server
   ```
2. Connect multiple clients by running:
   ```bash
   ./client
   ```
3. Each client enters a username and can start sending messages.
4. Offline messages are stored and delivered when the recipient reconnects.

## Educational Purpose

This project was developed for educational purposes to explore the following topics:
- Socket programming and network communication.
- Multi-threaded server implementation.
- Database integration using SQLite.
- Handling real-time and offline messaging.

It serves as a learning resource for understanding client-server architecture and implementing robust server-side logic.

## Future Enhancements

- Support for group chats.
- Encryption for secure communication.
- A more user-friendly interface with additional features. 

This application demonstrates foundational principles in system programming and networking, making it a valuable educational tool.
