#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "proto.h"  // 包含协议头文件

#define SERVER_IP "127.0.0.1"  // 服务器 IP 地址 (本地主机)
#define SERVER_PORT 5555       // 服务器端口

void send_hello(const int fd);
void receive_and_print_response(const int fd);

int main() {
    // 1. 创建套接字
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }
    printf("Client socket FD: %d\n", fd);

    // 2. 准备服务器地址信息
    struct sockaddr_in serverAddr = {0};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);  // 转换为网络字节序

    // 将 IP 地址从点分十进制转换为网络字节序
    if (inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr) <= 0) {
        perror("inet_pton");
        close(fd);
        return -1;
    }

    // 3. 连接到服务器
    if (connect(fd, (struct sockaddr *)&serverAddr, sizeof serverAddr) == -1) {
        perror("connect");
        close(fd);
        return -1;
    }
    printf("Connected to server %s:%d\n", SERVER_IP, SERVER_PORT);

    // 4. 发送 PROTO_HELLO 消息给服务器
    send_hello(fd);

    // 5. 接收服务器的响应
    receive_and_print_response(fd);

    // 6. 关闭套接字
    printf("Closing client socket.\n");
    close(fd);

    return 0;
}

// 发送一个 PROTO_HELLO 消息给服务器
void send_hello(const int fd) {
    char buf[4096] = {0};  // 准备一个缓冲区来构建协议消息
    proto_hdr_t *hdr = (proto_hdr_t *)buf;  // 将缓冲区起始地址转换为协议头指针

    // 填充协议头
    hdr->type = htonl(PROTO_HELLO);  // 消息类型，转换为网络字节序
    hdr->len = sizeof(int);          // 载荷长度，这里是 int 的大小
    int real_len = hdr->len;         // 保存实际载荷长度
    hdr->len = htons(real_len);  // 载荷长度字段转换为网络字节序

    // 填充载荷数据
    int *data = (int *)hdr->payload;  // 获取载荷的起始地址
    *data = htonl(100);  // 载荷数据，这里发送数字 100，转换为网络字节序

    // 发送整个消息：协议头 + 载荷
    ssize_t bytes_sent = write(fd, hdr, sizeof(proto_hdr_t) + real_len);
    if (bytes_sent == -1) {
        perror("send_hello: write");
        return;
    }
    printf("Sent PROTO_HELLO message (type %d, len %d, data %d), %zd bytes.\n",
           (int)ntohl(hdr->type), (int)ntohs(hdr->len), (int)ntohl(*data),
           bytes_sent);
}

// 接收服务器的响应并打印
void receive_and_print_response(const int fd) {
    char buf[4096] = {0};  // 缓冲区用于接收响应
    proto_hdr_t *hdr = (proto_hdr_t *)buf;

    // 尝试接收协议头
    ssize_t bytes_received = read(fd, hdr, sizeof(proto_hdr_t));
    if (bytes_received == -1) {
        perror("receive_response: read header");
        return;
    }
    if (bytes_received == 0) {
        printf("Server closed connection.\n");
        return;
    }
    if (bytes_received < (ssize_t)sizeof(proto_hdr_t)) {
        fprintf(stderr,
                "receive_response: Incomplete header received (%zd bytes).\n",
                bytes_received);
        return;
    }

    // 解析协议头
    proto_type_e type =
        (proto_type_e)ntohl(hdr->type);  // 消息类型从网络字节序转换
    unsigned short len = ntohs(hdr->len);  // 载荷长度从网络字节序转换

    printf("Received response: Type %d, Payload Length %hu\n", (int)type, len);

    // 如果有载荷，继续接收载荷数据
    if (len > 0) {
        if (len > (unsigned short)(sizeof(buf) - sizeof(proto_hdr_t))) {
            fprintf(
                stderr,
                "receive_response: Payload too large for buffer (len %hu).\n",
                len);
            return;  // 载荷太大无法放入缓冲区
        }

        // 接收载荷数据
        bytes_received = read(fd, hdr->payload, len);
        if (bytes_received == -1) {
            perror("receive_response: read payload");
            return;
        }
        if (bytes_received < len) {
            fprintf(stderr,
                    "receive_response: Incomplete payload received (%zd of %hu "
                    "bytes).\n",
                    bytes_received, len);
            return;
        }

        // 处理载荷数据 (根据协议类型)
        if (type == PROTO_HELLO) {
            if (len == sizeof(int)) {
                int *data = (int *)hdr->payload;
                int received_data = ntohl(*data);  // 从网络字节序转换
                printf("  PROTO_HELLO data: %d\n", received_data);
            } else {
                printf("  PROTO_HELLO with unexpected payload length %hu.\n",
                       len);
            }
        } else {
            printf("  Unknown protocol type received.\n");
        }
    }
}