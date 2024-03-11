# C Project: ChatServer

## Introduction

ChatServer is a robust, multi-client chat server implemented in C. It leverages socket programming to facilitate real-time text communication between clients. Upon connecting, clients can send messages to the server, which then capitalizes and broadcasts these messages to all connected clients. The server is designed to be non-blocking, using the `select` system call to manage multiple client connections efficiently.

## Features

ChatServer boasts a range of features, including:

- Support for multiple client connections.
- Non-blocking I/O operations, allowing the server to handle I/O in a single-threaded manner without delays.
- Real-time message broadcasting to all connected clients, with messages capitalized by the server before distribution.
- Graceful shutdown handling through signal integration (`SIGINT`).
- Dynamic management of client connections and message queues.

## Components

The server's main components include:

- `chatServer.c`: Contains the main server logic, handling client connections, message broadcasting, and server shutdown procedures.
- `chatServer.h`: Header file with declarations for server functions and structures.

## How It Works

1. The server initializes and starts listening on a specified port for incoming client connections.
2. When a client connects, it's added to a connection pool and monitored for incoming messages.
3. Incoming messages from clients are read, capitalized, and broadcasted to all other connected clients.
4. The server can handle multiple clients simultaneously, using non-blocking I/O and `select` to manage all connections efficiently.
5. Upon receiving a shutdown signal (`SIGINT`), the server gracefully terminates by closing all client connections and then shutting down.

## Running the Server

To compile and run ChatServer, follow these steps:

1. Ensure you have a C compiler (e.g., gcc) installed.
2. Compile the server using the following command: `gcc chatServer.c -o chatServer -lpthread`
3. Start the server by specifying a port number: `./chatServer <port>`

## Testing

To test the server's functionality:

1. Connect to the server using a telnet client or any TCP client tool: `telnet localhost <port>`
2. Send messages from different client instances.
3. Observe that messages are capitalized and broadcasted to all connected clients.

## Remarks

- ChatServer is designed for educational purposes to demonstrate non-blocking I/O, socket programming, and server-client communication in C.
- It handles basic text messaging and is not intended for production use.
- The server can be extended with features like private messaging, nickname support, or encrypted communication.

## Getting Started

To get started with ChatServer:

1. Compile the project as described in the "Running the Server" section.
2. Run the server on your desired port.
3. Connect multiple clients to test real-time messaging capabilities.

## Example Scenarios

This section illustrates a few example scenarios to demonstrate the chat server's behavior under different conditions:

### Multi-Client Chat Session
A conversation involving three clients, where the third client joins partway through the chat. This scenario demonstrates the server's ability to manage multiple connections and distribute messages accordingly.

![Full Chat Session](https://github.com/OmriNaor/chatServer/assets/106623821/eb8eacf4-c153-4e29-bd43-bc38dbe7187a)

### Client Disconnection
An example where the first client disconnects from the chat. This demonstrates the server's capability to handle client disconnections gracefully, ensuring the remaining clients can continue their conversation without interruption.

![Client Leaves Chat](https://github.com/OmriNaor/chatServer/assets/106623821/7b0634d0-1577-4c23-9354-53044ef1ad7d)

### Server Shutdown
This scenario shows what happens when the server shuts down unexpectedly during an ongoing conversation. It illustrates the server's behavior in closing connections and the impact on the chat session.

![Server Shutdown](https://github.com/OmriNaor/chatServer/assets/106623821/f83cdf5a-367b-4f23-9069-d534e766ca4d)


