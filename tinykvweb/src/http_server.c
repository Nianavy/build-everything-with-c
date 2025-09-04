#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include "../inc/http_server.h"
#include "../inc/parser.h"
#include "../inc/engine.h"

#define PROT_DEFAULT 8080
#define BUFFER_SIZE 4096

void handle_client(int client_sock, Storage *store) {
    char buffer[BUFFER_SIZE];
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) return;
    buffer[bytes] = '\0';

    if (strncmp(buffer, "POST /api/query", 15) != 0) {
        const char *err = "HTTP/1.0 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, err, strlen(err), 0);
        return;
    }

    char *body = strstr(buffer, "\r\n\r\n");
    if (!body) return;
    body += 4;

    KvCommand cmd;
    char response[512];

    if (parse_input(body, &cmd) == 0) 
        engine_execute(store, &cmd /* , response, sizeof(response) */);
    else snprintf(response, sizeof(response), "{\"error\":\"invalid command\"}");

    char http_response[1024];
    snprintf(http_response, sizeof(http_response),
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s", strlen(response), response);
    send(client_sock, http_response, strlen(http_response), 0);
}

void http_server_start(Storage *store, int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        exit(1);
    }

    printf("HTTP server running on port %d...\n", port);
    while (true) {
        int client_sock = accept(server_fd, NULL, NULL);
        if (client_sock >= 0) {
            handle_client(client_sock, store);
            close(client_sock);
        }
    }
    close(server_fd);
}