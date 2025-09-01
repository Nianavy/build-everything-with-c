/*
demo for IO Multiplexing: epoll.
*/
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For memset
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

// Define server modes
enum server_mode {
    MODE_LT, // Level Triggered
    MODE_ET  // Edge Triggered
};

// Client specific data to pass through epoll_event.data.ptr
struct client_data {
    int sock_fd;
    struct sockaddr_in client_addr;
    // Add more state here if needed, e.g., read buffer, write buffer, etc.
};

// Main server structure
struct server {
    int listen_fd;
    int epoll_fd;
    enum server_mode mode;
    char *ip_address;
    int port;
};

// Utility function to set socket non-blocking
void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL)");
        exit(EXIT_FAILURE);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl(F_SETFL)");
        exit(EXIT_FAILURE);
    }
}

// Function to safely close a client connection and free its resources
void close_client_connection(struct server *srv, struct client_data *client) {
    if (client->sock_fd != -1) {
        // Remove from epoll first
        if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_DEL, client->sock_fd, NULL) == -1) {
            perror("epoll_ctl(EPOLL_CTL_DEL) for client");
            // Don't exit, just log and continue to close socket
        }
        close(client->sock_fd);
        printf("Client disconnected: %s:%d (FD: %d)\n",
               inet_ntoa(client->client_addr.sin_addr),
               ntohs(client->client_addr.sin_port),
               client->sock_fd);
    }
    free(client); // Free the client_data structure
}

// Handles reading from a client socket
// Returns 1 if data was processed, 0 if client disconnected, -1 on error
int handle_client_read(struct server *srv, struct client_data *client) {
    char buffer[1024];
    ssize_t bytes_received;
    int client_fd = client->sock_fd;

    // ET mode must loop until EAGAIN/EWOULDBLOCK
    // LT mode can read once (though looping is safer for large data)
    // For simplicity, we'll loop in ET and do single read in LT for this demo.
    // In real LT, you might still want to loop for large data.
    do {
        bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No more data to read for now. This is expected in non-blocking.
                return 1; // Still active
            } else {
                perror("recv error");
                return -1; // Actual error
            }
        }

        if (bytes_received == 0) {
            // Client closed connection
            printf("Client sent EOF.\n");
            return 0;
        }

        buffer[bytes_received] = '\0'; // Null-terminate for printing
        printf("Received from client %d: %s\n", client_fd, buffer);

        // Echo back
        ssize_t bytes_sent = send(client_fd, buffer, bytes_received, 0);
        if (bytes_sent == -1) {
            perror("send error");
            return -1; // Actual error
        }
        if (bytes_sent == 0) {
            // Should not happen for non-zero bytes_received, but good to check.
            printf("Send returned 0 bytes, connection might be closing.\n");
            return 0;
        }

    } while (srv->mode == MODE_ET); // Loop only in ET mode

    return 1; // Still active
}

// Initializes the server's listening socket
int setup_listener(struct server *srv) {
    srv->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->listen_fd == -1) {
        perror("socket");
        return -1;
    }

    // Allow immediate reuse of the address
    int optval = 1;
    if (setsockopt(srv->listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt(SO_REUSEADDR)");
        close(srv->listen_fd);
        return -1;
    }

    set_nonblocking(srv->listen_fd);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr)); // Clear the structure
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(srv->ip_address);
    addr.sin_port = htons(srv->port);

    if (bind(srv->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(srv->listen_fd);
        return -1;
    }

    if (listen(srv->listen_fd, 128) == -1) { // Increased backlog for robustness
        perror("listen");
        close(srv->listen_fd);
        return -1;
    }

    printf("Server listening on %s:%d (FD: %d)\n", srv->ip_address, srv->port, srv->listen_fd);
    return 0;
}

// Main epoll event loop
void run_server(struct server *srv) {
    srv->epoll_fd = epoll_create1(0);
    if (srv->epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event;
    event.data.fd = srv->listen_fd;
    event.events = EPOLLIN; // Listen socket always LT for new connections

    if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, srv->listen_fd, &event) == -1) {
        perror("epoll_ctl(ADD listen_fd)");
        close(srv->epoll_fd);
        close(srv->listen_fd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[1024]; // Max events per wait call

    while (1) {
        int num_events = epoll_wait(srv->epoll_fd, events, 1024, -1); // Wait indefinitely
        if (num_events == -1) {
            if (errno == EINTR) { // Interrupted by signal
                continue;
            }
            perror("epoll_wait");
            break; // Fatal epoll error, exit loop
        }

        for (int i = 0; i < num_events; ++i) {
            // Handle listen socket for new connections
            if (events[i].data.fd == srv->listen_fd) {
                while (1) { // Loop for accept, especially in ET mode
                    struct client_data *new_client = (struct client_data *)malloc(sizeof(struct client_data));
                    if (!new_client) {
                        perror("malloc for new client");
                        // Log and continue, or handle out of memory
                        break; // Can't accept more if no memory
                    }
                    memset(new_client, 0, sizeof(struct client_data));
                    socklen_t client_addr_len = sizeof(new_client->client_addr);
                    new_client->sock_fd = accept(srv->listen_fd, (struct sockaddr *)&new_client->client_addr, &client_addr_len);

                    if (new_client->sock_fd == -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            // No more incoming connections for now
                            free(new_client);
                            break; // Exit accept loop
                        } else {
                            perror("accept error");
                            free(new_client);
                            // Log and continue, don't break main loop for one failed accept
                            break; // Exit accept loop for this error
                        }
                    }

                    set_nonblocking(new_client->sock_fd);
                    printf("Client connected: %s:%d (FD: %d)\n",
                           inet_ntoa(new_client->client_addr.sin_addr),
                           ntohs(new_client->client_addr.sin_port),
                           new_client->sock_fd);

                    struct epoll_event client_event;
                    client_event.data.ptr = new_client; // Pass client_data struct
                    client_event.events = EPOLLIN | EPOLLRDHUP; // Always watch for read and client hangup

                    if (srv->mode == MODE_ET) {
                        client_event.events |= EPOLLET; // Add ET flag if server is in ET mode
                    }

                    if (epoll_ctl(srv->epoll_fd, EPOLL_CTL_ADD, new_client->sock_fd, &client_event) == -1) {
                        perror("epoll_ctl(ADD client_fd)");
                        close_client_connection(srv, new_client); // Clean up
                        // Log and continue, don't break main loop
                        break; // Exit accept loop for this error
                    }
                } // End while(1) accept loop
            } else { // Handle client connection events
                struct client_data *client = (struct client_data *)events[i].data.ptr;

                // Handle client disconnection (EPOLLRDHUP or EPOLLERR/EPOLLHUP)
                if ((events[i].events & EPOLLRDHUP) || (events[i].events & EPOLLHUP) || (events[i].events & EPOLLERR)) {
                    printf("Client FD %d hangup or error.\n", client->sock_fd);
                    close_client_connection(srv, client);
                    continue; // Go to next event
                }

                // Handle read event
                if (events[i].events & EPOLLIN) {
                    int res = handle_client_read(srv, client);
                    if (res == -1 || res == 0) { // Error or client disconnected
                        close_client_connection(srv, client);
                        // IMPORTANT: continue to the next event, DO NOT break the for loop
                        continue;
                    }
                }
                // Add EPOLLOUT handling here if needed
            }
        } // End for loop over events
    } // End while(1) main event loop

    // Cleanup resources
    close(srv->epoll_fd);
    close(srv->listen_fd);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <ip_address> <port> [lt|et]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct server srv;
    memset(&srv, 0, sizeof(srv)); // Clear server structure

    srv.ip_address = argv[1];
    srv.port = atoi(argv[2]);

    if (argc > 3 && strcmp(argv[3], "et") == 0) {
        srv.mode = MODE_ET;
        printf("Starting server in ET (Edge Triggered) mode.\n");
    } else {
        srv.mode = MODE_LT;
        printf("Starting server in LT (Level Triggered) mode.\n");
    }

    if (setup_listener(&srv) == -1) {
        exit(EXIT_FAILURE);
    }

    run_server(&srv);

    return 0;
}