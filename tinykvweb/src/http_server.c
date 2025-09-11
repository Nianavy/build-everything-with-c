#include "../inc/http_server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../inc/api_handler.h"

#define BUFFER_SIZE 2048

void handle_client(int client_sock, Storage *store) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    bytes_read = read(client_sock, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        perror("read");
        close(client_sock);
        return;
    }
    buffer[bytes_read] = '\0';

    char method[8], path[128];
    if (sscanf(buffer, "%7s %63s", method, path) != 2) {
        const char *msg = "{\"error\":\"Malformed request line\"}";
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response),
                 "HTTP/1.0 400 Bad Request\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n"
                 "%s",
                 strlen(msg), msg);
        write(client_sock, response, strlen(response));
        close(client_sock);
        return;
    }

    char *body_pos = strstr(buffer, "\r\n\r\n");
    const char *body = body_pos ? body_pos + 4 : "";

    char msg[BUFFER_SIZE * 2];
    memset(msg, 0, sizeof(msg));

    int is_full_http_resonse = 0;
    handle_api_request(store, path, body, msg, sizeof(msg));

    if (strncmp(msg, "HTTP/", 5) == 0) is_full_http_resonse = 1;

    if (is_full_http_resonse)
        write(client_sock, msg, strlen(msg));
    else {
        char response[BUFFER_SIZE * 2];
        snprintf(response, sizeof(response),
                 "HTTP/1.0 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n"
                 "%s",
                 strlen(msg), msg);
        write(client_sock, response, strlen(response));
    }
    close(client_sock);
}

void http_server_start(Storage *store, int port) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int reuse = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_sock, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 5) < 0) {
        perror("listen");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("HTTP server started on port %d...\n", port);

    while (true) {
        client_sock =
            accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        handle_client(client_sock, store);
    }
    close(server_sock);
}