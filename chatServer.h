#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>

#define BUFFER_SIZE 4096
static int end_server = 0;

/*
 * Data structure to keep track of active client connections (not the for main socket).
 */
typedef struct conn_pool {
    /* Largest file descriptor in this pool. */
    int maxfd;
    /* Number of ready descriptors returned by select. */
    int nready;
    /* Set of all active descriptors for reading. */
    fd_set read_set;
    /* Subset of descriptors ready for reading. */
    fd_set ready_read_set;
    /* Set of all active descriptors for writing. */
    fd_set write_set;
    /* Subset of descriptors ready for writing.  */
    fd_set ready_write_set;
    /* Doubly-linked list of active client connection objects. */
    struct conn *conn_head;
    /* Number of active client connections. */
    unsigned int nr_conns;

}conn_pool_t;

/*
 * Data structure to keep track of messages. Each message object holds one
 * complete line of message from a client.
 *
 * The message objects are maintained per connection in a doubly-linked list.
 * When a message is read from one connection, it is added to the list of all other connections.
 *
 * A message is added to the list only when a complete line has been read from
 * the client.
 */
typedef struct msg {
    /* Points to the previous message object in the doubly-linked list. */
    struct msg *prev;
    /* Points to the next message object in the doubly-linked list. */
    struct msg *next;
    /* Points to a dynamically allocated buffer holding the message. */
    char *message;
    /* Size of the message. */
    int size;
}msg_t;


/*
 * Data structure to keep track of client connection state.
 *
 * The connection objects are also maintained in a global doubly-linked list.
 * There is a dummy connection head at the beginning of the list.
 */
typedef struct conn {
    /* Points to the previous connection object in the doubly-linked list. */
    struct conn *prev;
    /* Points to the next connection object in the doubly-linked list. */
    struct conn *next;
    /* File descriptor associated with this connection. */
    int fd;
    /*
     * Pointers for the doubly-linked list of messages that
     * have to be written out on this connection.
     */
    struct msg *write_msg_head;
    struct msg *write_msg_tail;
}conn_t;

/**
 * Allocates and initializes a new msg_t structure to hold a copy of a given message.
 * This function dynamically allocates memory for both the msg_t structure and its message
 * content. It then copies the given message into the newly allocated buffer, sets the message size,
 * and initializes the next and prev pointers to NULL, indicating that the message is not yet linked
 * into a message queue.
 *
 * @param buffer: A pointer to the character array containing the message to be copied.
 * @param len: The length of the message in bytes. This length is used to allocate the
 *             appropriate amount of memory for the message content and determines how much
 *             data will be copied from the buffer into the new message structure.
 * @return
 *   - A pointer to the newly created msg_t structure if the function succeeds.
 *   - NULL if memory allocation for either the msg_t structure or its message content fails.
 */
msg_t* createMessage(const char* buffer, int len);

/**
 * Initializes a connection pool structure. This function sets up the initial state
 * of the conn_pool_t structure by setting the maxfd to -1 (indicating that no file descriptors
 * are currently in the pool), initializing the number of ready descriptors (nready) to 0,
 * and clearing all file descriptor sets used for select() operations. It also initializes
 * the head of the linked list of connections (conn_head) to NULL and sets the number of active
 * connections (nr_conns) to 0. This ensures that the connection pool is in a valid state
 * before being used to manage client connections.
 *
 * @param pool: A pointer to a conn_pool_t structure that will be initialized.
 * @return
 *   - 0 if the pool is successfully initialized.
 *   - -1 if the provided pointer to the pool is NULL, indicating a failure to initialize.
 */
int initPool(conn_pool_t* pool);

// Signal handler to gracefully terminate the server
void intHandler(int SIG_INT);

/**
 * Frees all messages in the write queue of a given connection. This function iterates through
 * the doubly-linked list of message structures, freeing the memory allocated for both the message
 * content and the message structure itself. After all messages have been freed, it resets the
 * connection's pointers to the head and tail of the write queue to NULL, indicating that the queue
 * is empty.
 *
 * @param conn: A pointer to a conn_t structure representing a single client connection. The
 *              function will operate on the write queue of this connection.
 */
void freeMessagesInQueue(conn_t* conn);

/**
 * Updates the maximum file descriptor (maxfd) value in the connection pool. This function
 * iterates through all active connections in the pool to find the highest socket descriptor
 * value and sets the pool's maxfd to the maximum of these values or the welcome socket's
 * descriptor, whichever is higher. This is necessary to ensure the select() call in the main
 * loop correctly monitors all active file descriptors, including the welcome socket for
 * accepting new connections and all client sockets for reading and writing.
 *
 * @param pool: A pointer to the conn_pool_t structure representing the current state of active
 *              connections and their management.
 * @param welcome_socket: The socket descriptor of the server's welcome socket. This descriptor
 *                        is used as a baseline for the maximum descriptor value since it must
 *                        always be included in the set of descriptors monitored by select().
 */
void updateMaxFd(conn_pool_t* pool, int welcome_socket);

/**
 * Initializes the server by creating a welcome socket, setting it to non-blocking mode,
 * and binding it to the specified port. This function sets up the server's listening
 * environment, making it ready to accept incoming connections.
 *
 * @param port: The port number on which the server will listen for incoming connections.
 * @return: The socket descriptor of the welcome socket if successful, or -1 on failure.
 */
int initializeServer(in_port_t port);

/**
 * Capitalizes all alphabetic characters in a given string. This function iterates
 * through each character of the string, converting it to its uppercase equivalent
 * if it is an alphabetic character.
 *
 * @param message: The string to be capitalized. This string is modified in place.
 * @param length: The length of the string.
 */
void capitalizeMessage(char* message, int length);

/**
 * Accepts a new connection on the welcome socket and adds it to the connection pool.
 * If a new connection is successfully established, it also updates the maximum file
 * descriptor value in the pool.
 *
 * @param welcome_socket: The socket descriptor of the server's welcome socket.
 * @param pool: A pointer to the conn_pool_t structure representing the current state of active connections.
 */
int acceptNewConnection(int welcome_socket, conn_pool_t* pool);

/**
 * Reads data from an active connection, capitalizes it, and then broadcasts
 * the message to other connections. If the connection is closed, it removes
 * the connection from the pool and updates the maxfd accordingly.
 *
 * @param sd: The socket descriptor of the connection to read from.
 * @param pool: A pointer to the conn_pool_t structure for managing active connections.
 * @param welcome_socket: The socket descriptor of the server's welcome socket for maxfd calculation.
 */
void processDataFromConnection(int sd, conn_pool_t* pool, int welcome_socket);

/**
 * Adds a new client connection to the connection pool. This function dynamically allocates memory
 * for a new conn_t structure to represent the client connection identified by the socket descriptor 'sd'.
 * It initializes this structure, sets it as the new head of the doubly linked list of connections within
 * the pool, and updates the file descriptor sets and maximum file descriptor value as necessary.
 * The connection pool is used to manage active client connections and facilitate select-based multiplexing
 * for handling I/O operations.
 *
 * @param sd: The socket descriptor of the new client connection to be added to the pool.
 * @param pool: A pointer to the conn_pool_t structure representing the connection pool.
 * @return
 *   - 0 if the new connection is successfully added to the pool.
 *   - -1 if the provided pool pointer is NULL, memory allocation for the new connection fails,
 *     or any other error occurs during the process.
 */
int addConn(int sd, conn_pool_t* pool);

/**
 * Removes a client connection from the connection pool. This function finds the conn_t
 * structure associated with the given socket descriptor (sd) in the pool's linked list of
 * connections, frees all queued messages, removes the connection from the list, and updates
 * the pool's file descriptor sets and maxfd value as necessary. It also closes the socket
 * descriptor and frees the conn_t structure.
 *
 * @param sd: The socket descriptor of the connection to remove.
 * @param pool: A pointer to the connection pool (conn_pool_t structure) from which the
 *              connection will be removed.
 * @return
 *   - 0 on success, indicating the connection was successfully removed from the pool.
 *   - -1 on failure, indicating an invalid pool pointer was provided or the specified
 *     connection was not found in the pool.
 */
int removeConn(int sd, conn_pool_t* pool);

/**
 * Distributes a message to all active connections in the connection pool, except for the sender.
 * For each connection, this function creates a new msg_t structure containing a copy of the given
 * message, then appends this message to the end of the connection's write queue. It ensures that
 * the message will be sent to each client by marking the connection as ready for writing in the
 * connection pool's write_set.
 *
 * @param sd: The socket descriptor of the sender. The message will not be added to the sender's
 *            write queue to avoid echoing the message back to the sender.
 * @param buffer: A pointer to the character array containing the message to be distributed.
 * @param len: The length of the message in bytes, indicating how much data from the buffer should
 *             be copied into each new message structure.
 * @param pool: A pointer to the conn_pool_t structure representing the current state of active
 *              connections and their write queues.
 * @return
 *   - 0 on successful distribution of the message to all connections except the sender.
 *   - -1 if the provided pool pointer is NULL, indicating an error.
 */
int addMsg(int sd,char* buffer,int len,conn_pool_t* pool);


/**
 * Writes all queued messages for a specific client connection to the client. This function
 * iterates through the write queue of messages for the connection identified by the socket
 * descriptor (sd), writing each message to the client. After a message has been successfully
 * written, it is removed from the queue and its memory is freed. If all messages are successfully
 * written and the queue becomes empty, the connection's descriptor is removed from the write set
 * to indicate that there is no more data pending to be sent to this client.
 *
 * @param sd: The socket descriptor of the connection for which messages are to be written.
 * @param pool: A pointer to the conn_pool_t structure representing the current state of active
 *              connections and their write queues.
 * @return
 *   - 0 on success, indicating that messages were written to the client or there were no messages
 *     to write.
 *   - -1 on failure, indicating an invalid pool pointer was provided, the connection for the given
 *     socket descriptor was not found, or an error occurred during writing.
 */
int writeToClient(int sd,conn_pool_t* pool);

#endif
