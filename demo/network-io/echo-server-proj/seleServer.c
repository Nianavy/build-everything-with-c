/*
demo for IO Multiplexing: select.
*/

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    int sock;
    struct sockaddr_in addr;
} connection_t;

typedef struct {
    int (*make_listener)(const char *, int);
    connection_t (*accept_connection)(int);
    int (*echo)(int, void (*)(int, void *), void *);
    void (*sele)(int);
} server_t;

int make_listener(const char *ip, int port) {
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == -1) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);

    if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(listen_sock);
        return -1;
    }

    if (listen(listen_sock, 5) == -1) {
        perror("listen");
        close(listen_sock);
        return -1;
    }
    return listen_sock;
}

connection_t accept_connection(int listen_sock) {
    connection_t conn;
    conn.sock = -1;
    socklen_t addr_len = sizeof(conn.addr);
    int sock= accept(listen_sock, (struct sockaddr *)&conn.addr, &addr_len);
    if (sock == -1) {
        perror("accept");
        return conn;
    }
    conn.sock = sock;
    printf("Client connected: %s:%d\n", inet_ntoa(conn.addr.sin_addr),
                            ntohs(conn.addr.sin_port));
    return conn;
}

int echo(int sock, void (*__callback)(int, void *args), void *args) {
    char buffer[1024];
    ssize_t bytes_received = recv(sock, buffer, sizeof(buffer), 0);
    if (bytes_received == -1) {
        perror("recv");
        close(sock);
        return -1;
    }

    if (bytes_received == 0) {
        if (__callback != NULL) __callback(sock, args);
        else close(sock);
        return 0;
    }

    buffer[bytes_received] = '\0';

    printf("Received from client: %s\n", buffer);
    int bytes_sended = send(sock, buffer, bytes_received, 0);
    if (bytes_sended == -1) {
        perror("send");
        close(sock);
        return -1;
    }

    if (bytes_sended == 0) {
        if (__callback != NULL) __callback(sock, args);
        else close(sock);
        return 0;
    }

    return 1;
}

typedef struct {
    fd_set *read_fds;
    int *maxfd;
} args_t;

void callback(int sock, void *args) {
    args_t *cbargs = (args_t *)args;
    close(sock);
    FD_CLR(sock, cbargs->read_fds);
    if (sock == *cbargs->maxfd) {
        for (int i = *cbargs->maxfd - 1; i > 0; --i) {
            if (FD_ISSET(i, cbargs->read_fds)) {
                *cbargs->maxfd = i;
                break;
            }
        }
    }
}

void sele(int listen_sock) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(listen_sock, &read_fds);

    int maxfd = listen_sock;
    while (true) {
        fd_set result_fds;
        result_fds = read_fds;
        int flag = select(maxfd + 1, &result_fds, NULL, NULL, NULL);
        if (flag < 0) {
            perror("select");
            close(listen_sock);
            break;
        }
        if (flag == 0) continue;
        for (int fd = 0; fd <= maxfd; ++fd) {
            if (!FD_ISSET(fd, &result_fds)) continue;
            if (fd == listen_sock) {
                connection_t conn = accept_connection(listen_sock);
                FD_SET(conn.sock, &read_fds);
                if (conn.sock > maxfd) maxfd = conn.sock;
                continue;
            }
            else {
                args_t cbargs = { .maxfd = &maxfd, .read_fds = &read_fds };
                int res = echo(fd, callback, &cbargs);
                if (res <= 0) break;
            }
        }
    }
}

server_t make_server() {
    server_t server = {
        .make_listener = make_listener,
        .accept_connection = accept_connection,
        .echo = echo,
        .sele = sele,
    };
    return server;
};

int main() {
    server_t server = make_server();
    int listen_sock = server.make_listener("127.0.0.1", 6666);
    if (listen_sock == -1) return -1;
    server.sele(listen_sock);
    close(listen_sock);
    return 0;
}