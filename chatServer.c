#include "chatServer.h"

void intHandler(int SIG_INT)
{
    (void)SIG_INT; // Explicitly mark the parameter as unused
    end_server = 1;
}

int main(int argc, char* argv[])
{
    // Check command line arguments
    if (argc != 2)
    {
        printf("Usage: server <port>\n");
        exit(EXIT_FAILURE);
    }

    // Convert port number from string to integer, ensuring it's in a valid range
    long temp_port = (long) strtoul(argv[1], NULL, 10);
    if (temp_port < 1 || temp_port > 65535)
    {
        printf( "Usage: server <port>\n");
        exit(EXIT_FAILURE);
    }

    in_port_t port = (in_port_t)temp_port;

    // Register signal handler for graceful shutdown
    signal(SIGINT, intHandler);

    // Initialize server and get the welcome socket
    int welcome_socket = initializeServer(port);
    if (welcome_socket == -1)
        exit(EXIT_FAILURE); // Server initialization failed

    // Initialize connection pool
    conn_pool_t pool;
    initPool(&pool);

    // Add the listening socket to the set of file descriptors to monitor
    FD_SET(welcome_socket, &pool.read_set);
    pool.maxfd = welcome_socket; // Initially, the listening socket has the highest file descriptor number

    // Main server loop
    do
    {
        // Copy file descriptor sets to avoid modifying the original sets
        pool.ready_read_set = pool.read_set;
        pool.ready_write_set = pool.write_set;

        // Block until input arrives at one or more active sockets
        printf("Waiting on select()...\nMaxFd %d\n", pool.maxfd);
        pool.nready = select(pool.maxfd + 1, &pool.ready_read_set, &pool.ready_write_set, NULL, NULL);
        if (pool.nready < 0)
            continue;

        // Check each file descriptor in the set
        for (int sd = 0; sd <= pool.maxfd && pool.nready > 0; sd++)
        {
            // Accept new connections
            if (FD_ISSET(sd, &pool.ready_read_set))
            {
                if (sd == welcome_socket)
                {
                    if (acceptNewConnection(welcome_socket, &pool) != 0) // Accept the new connection and add it to the connections list
                        continue;
                }

                else
                {
                    // Read data and add it to the clients' queues (or remove the client is disconnected)
                    processDataFromConnection(sd, &pool, welcome_socket);
                }

                pool.nready--;
            }

            // Send queued messages to ready connections
            if (FD_ISSET(sd, &pool.ready_write_set))
            {
                writeToClient(sd, &pool);
                pool.nready--;
            }
        }
    } while (!end_server);


    /* Cleanup connections on server shutdown */
    conn_t* current = pool.conn_head;
    while (current != NULL)
    {
        int current_fd = current->fd; // Store the current FD
        conn_t* next = current->next; // Save the next connection
        // Remove and cleanup the current connection
        if (removeConn(current_fd, &pool) == 0)
            printf("removing connection with sd %d \n", current_fd);

        current = next; // Move to the next connection
    }

    // Finally, close the listening socket
    close(welcome_socket);
}

void processDataFromConnection(int sd, conn_pool_t* pool, int welcome_socket)
{
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    printf("Descriptor %d is readable\n", sd);
    ssize_t bytes_read = read(sd, buffer, BUFFER_SIZE - 1);
    if (bytes_read > 0)
    {
        printf("%zd bytes received from sd %d\n", bytes_read, sd);
        capitalizeMessage(buffer, (int)bytes_read);
        addMsg(sd, buffer, (int)bytes_read, pool);
    }
    else if (bytes_read == 0)
    {
        printf("Connection closed for sd %d\n", sd);
        printf("removing connection with sd %d \n", sd);
        removeConn(sd, pool);
        updateMaxFd(pool, welcome_socket); // Recalculate maxfd
    }
    else
        perror("Error reading from socket");
}

int acceptNewConnection(int welcome_socket, conn_pool_t* pool)
{
    int new_socket = accept(welcome_socket, NULL, NULL);
    if (new_socket < 0)
    {
        perror("accept failed");
        return -1;
    }

    if (addConn(new_socket, pool) < 0)
    {
        fprintf(stderr, "Failed to add new connection to pool\n");
        close(new_socket);
        return -1;
    }

    printf("New incoming connection on sd %d\n", new_socket);
    updateMaxFd(pool, welcome_socket); // Recalculate maxfd
    return 0;
}

void capitalizeMessage(char* message, int length)
{
    for (int i = 0; i < length; ++i)
        message[i] = (char)toupper((unsigned char)message[i]);
}

void updateMaxFd(conn_pool_t* pool, int welcome_socket)
{
    int max_fd = welcome_socket; // Start with the welcome_socket's descriptor as the minimum

    // Iterate through all connections to find the highest file descriptor
    for (conn_t* conn = pool->conn_head ; conn != NULL ; conn = conn->next)
        if (conn->fd > max_fd)
            max_fd = conn->fd;

    // Update the pool's maxfd
    pool->maxfd = max_fd;
}


int initializeServer(in_port_t port)
{
    int welcome_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (welcome_socket < 0)
    {
        perror("Error creating socket");
        return -1;
    }

    // Make the socket non-blocking
    int on = 1;
    if (ioctl(welcome_socket, FIONBIO, (char*)&on) < 0)
    {
        perror("ioctl failed");
        close(welcome_socket);
        return -1;
    }

    // Bind the socket to the specified port on any network interface
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);
    if (bind(welcome_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error binding socket");
        close(welcome_socket);
        return -1;
    }

    // Set connections queue size
    if (listen(welcome_socket, 5) < 0)
    {
        perror("Listen failed");
        close(welcome_socket);
        return -1;
    }

    return welcome_socket;
}


void freeMessagesInQueue(conn_t* conn)
{
    if (!conn) // Safety check to ensure the connection pointer is not NULL.
        return;

    msg_t* msg = conn->write_msg_head; // Start with the head of the message queue.
    while (msg != NULL)
    {
        msg_t* next_msg = msg->next; // Save the next message before freeing the current one.
        free(msg->message); // Free the message content.
        free(msg); // Free the message structure itself.
        msg = next_msg; // Move to the next message in the queue.
    }
    // After freeing all messages, reset the head and tail pointers of the queue.
    conn->write_msg_head = conn->write_msg_tail = NULL;
}


int initPool(conn_pool_t* pool)
{
    if (!pool)
    {
        printf("Invalid pool pointer provided\n");
        return -1; // Return -1 on failure, indicating the provided pointer is NULL.
    }

    // Set initial values for the connection pool structure.
    pool->maxfd = -1; // Indicate no file descriptors are present.
    pool->nready = 0; // No file descriptors are initially ready.
    // Clear file descriptor sets for reading and writing.
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->ready_read_set);
    FD_ZERO(&pool->write_set);
    FD_ZERO(&pool->ready_write_set);
    // Initialize the connection list to empty.
    pool->conn_head = NULL;
    pool->nr_conns = 0; // Initially, there are no connections in the pool.

    return 0; // Return 0 on successful initialization.
}


int addConn(int sd, conn_pool_t* pool)
{
    if (!pool)
    {
        fprintf(stderr, "Invalid pool pointer provided\n");
        return -1; // Return -1 on failure, indicating the provided pool pointer is NULL.
    }

    // Allocate memory for the new connection structure.
    conn_t* new_conn = (conn_t*) malloc(sizeof(conn_t));
    if (new_conn == NULL)
    {
        fprintf(stderr, "malloc failed\n");
        close(sd); // Close the socket descriptor to prevent resource leak.
        return -1; // Return -1 on failure, indicating memory allocation failed.
    }

    // Initialize the newly allocated connection structure.
    new_conn->fd = sd; // Assign the provided socket descriptor.
    new_conn->write_msg_head = NULL; // Initialize the message queue as empty.
    new_conn->write_msg_tail = NULL;
    new_conn->prev = NULL; // New connection will be the new head, so no previous connection.
    new_conn->next = pool->conn_head; // The current head becomes the next connection.

    // If the connection pool is not empty, link the current head back to the new connection.
    if (pool->conn_head != NULL)
        pool->conn_head->prev = new_conn;

    // Set the new connection as the head of the doubly linked list in the pool.
    pool->conn_head = new_conn;

    // Add the new connection's socket descriptor to the read set for monitoring.
    FD_SET(sd, &pool->read_set);

    // Increment the total number of active connections in the pool.
    pool->nr_conns++;

    return 0; // Return 0 on successful addition of the new connection.
}


int removeConn(int sd, conn_pool_t* pool)
{
    if (!pool)
    {
        fprintf(stderr, "Invalid pool pointer provided\n");
        return -1;
    }

    // Find the connection in the pool.
    conn_t* temp = pool->conn_head;
    conn_t* prev = NULL;
    while (temp != NULL && temp->fd != sd)
    {
        prev = temp;
        temp = temp->next;
    }

    // Check if the connection was found.
    if (temp == NULL)
    {
        fprintf(stderr, "Connection with sd %d not found\n", sd);
        return -1;
    }

    // Free all messages in the connection's queue before removing it.
    freeMessagesInQueue(temp);

    // Remove the connection from the doubly linked list.
    if (prev == NULL) // If removing the head of the list.
        pool->conn_head = temp->next;

    else // If removing a connection in the middle or at the end.
        prev->next = temp->next;

    if (temp->next != NULL)
        temp->next->prev = prev;

    // Update the file descriptor sets.
    FD_CLR(sd, &pool->read_set);
    FD_CLR(sd, &pool->write_set);

    // Close the socket descriptor and free the connection structure.
    close(sd);
    free(temp);

    // Decrement the number of connections.
    pool->nr_conns--;

    return 0; // Return 0 on success.
}


msg_t* createMessage(const char* buffer, int len)
{
    // Allocate memory for the msg_t structure.
    msg_t* message = (msg_t*) malloc(sizeof(msg_t));
    if (!message)
    {
        fprintf(stderr, "malloc failed\n");
        return NULL; // Allocation failed; return NULL.
    }

    // Allocate memory for the message content.
    message->message = (char*)malloc(len);
    if (!message->message)
    {
        fprintf(stderr, "malloc failed\n");
        free(message); // Free the previously allocated msg_t structure before returning NULL.
        return NULL;
    }

    // Copy the provided message content into the newly allocated buffer.
    memcpy(message->message, buffer, len);
    message->size = len; // Set the message size.
    message->next = message->prev = NULL; // Initialize next and prev pointers to NULL.

    return message; // Return the pointer to the newly created message structure.
}

int addMsg(int sd, char* buffer, int len, conn_pool_t* pool)
{
    if (!pool)
    {
        fprintf(stderr, "Invalid pool pointer provided\n");
        return -1; // Return -1 on failure, indicating an invalid pool pointer.
    }

    // Iterate over all connections, excluding the sender.
    for (conn_t* conn = pool->conn_head; conn != NULL; conn = conn->next)
        if (conn->fd != sd) // Check if the current connection is not the sender.
        {
            // Create a new message for each connection.
            msg_t* newMsg = createMessage(buffer, len);
            if (!newMsg)
            {
                // If message creation fails, log the error and continue to the next connection.
                fprintf(stderr, "Failed to create a new message for connection %d\n", conn->fd);
                continue; // Continue to the next connection without halting the loop.
            }

            // Add the message to the write queue of the connection.
            if (!conn->write_msg_tail) // If the queue is empty.
                // Set both head and tail to the new message for an empty queue.
                conn->write_msg_head = conn->write_msg_tail = newMsg;

            else // For a non-empty queue.
            {
                // Append the new message at the end of the queue.
                conn->write_msg_tail->next = newMsg;
                newMsg->prev = conn->write_msg_tail;
                conn->write_msg_tail = newMsg;
            }

            // Mark the connection as ready for writing.
            FD_SET(conn->fd, &pool->write_set);
        }

    return 0; // Return 0 on success.
}


int writeToClient(int sd, conn_pool_t* pool)
{
    if (!pool)
    {
        fprintf(stderr, "Invalid pool pointer provided\n");
        return -1; // Return -1 on failure, indicating an invalid pool pointer.
    }

    // Find the connection in the pool matching the provided socket descriptor.
    conn_t* conn = pool->conn_head;
    while (conn && conn->fd != sd)
        conn = conn->next;

    if (!conn)
    {
        fprintf(stderr, "No connection found for sd %d\n", sd);
        return -1; // Return -1 if the connection is not found in the pool.
    }

    // Iterate through the write queue of the connection and write each message.
    msg_t* msg = conn->write_msg_head;
    msg_t* next_msg = NULL;
    int is_error = -1;

    while (msg)
    {
        ssize_t written = write(sd, msg->message, msg->size);
        if (written <= 0)
        {
            // Handle errors or connection closure during write operation.
            perror("Error writing to client");
            is_error = 1;
            break; // Exit the loop if a write error occurs.
        }

        // Proceed to the next message and free the current one.
        next_msg = msg->next;
        free(msg->message); // Free the message content.
        free(msg); // Free the message structure.

        msg = next_msg; // Move to the next message in the queue.
    }

    if (is_error != 1)
    {
        // Reset the connection's write queue indicators to show it's empty.
        conn->write_msg_head = NULL;
        conn->write_msg_tail = NULL;
    }

    // Clear the socket descriptor from the write set if no more messages are pending.
    FD_CLR(sd, &pool->write_set);

    return 0; // Return 0 on success, indicating messages were written or no action was needed.
}