#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "proto.h"

void handle_client(const int fd);

int main() {
    // socket 获取socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }
    printf("%d\n", fd);

    // bind 设置服务器地址参数
    struct sockaddr_in serverInfo = {0};
    serverInfo.sin_family = AF_INET;
    serverInfo.sin_port = htons(5555);
    serverInfo.sin_addr.s_addr = htonl(INADDR_ANY);
    //memset(serverInfo.sin_zero, 0, 8);

    if (bind(fd, (struct sockaddr *)&serverInfo,
    sizeof serverInfo) == -1) {
        perror("bind");
        close(fd);
        return -1;
    }

    // listen  设置为待连接的状态
    if (listen(fd, 1024) == -1) {
        perror("listen");
        close(fd);
        return -1;
    }

    // accept  接受客户端连接
    struct sockaddr_in clientInfo = {0};
    socklen_t clientSize = sizeof clientInfo;

    while (true) {
        int cfd = accept(fd, (struct sockaddr *)&clientInfo, &clientSize);
        if (cfd == -1) {
            perror("accept");
            close(fd);
            return -1;
        }

        handle_client(cfd);

        // close
        close(cfd);
    }

    close(fd);

    return 0;
}

// 处理客户端发送的数据
void handle_client(const int fd) {
    char buf[4096] = {0};
    proto_hdr_t *hdr = (proto_hdr_t *)buf;
    hdr->type = htonl(PROTO_HELLO);
    hdr->len = sizeof(int);
    int real_len = hdr->len;
    hdr->len = htons(real_len);
    int *data = (int *)hdr->payload;
    *data = htonl(1);

    if (write(fd, hdr, sizeof(proto_hdr_t) + real_len) == -1) {
        perror("write");
        return;
    }
}