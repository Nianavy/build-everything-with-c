#include <arpa/inet.h>   // For inet_addr
#include <netinet/in.h>  // For sockaddr_in
#include <stdio.h>       // For printf, perror
#include <stdlib.h>      // For exit, EXIT_FAILURE
#include <string.h>      // For memset, strlen
#include <sys/socket.h>  // For socket, connect, send, recv
#include <unistd.h>      // For close, sleep

#include "proto.h"

#define SERVER_IP "127.0.0.1"  // 服务器 IP 地址，这里是本地回环地址
#define PORT 3333
#define BUFF_SIZE 4096  // 缓冲区大小，与服务器保持一致

int main() {
    int sock_fd;
    struct sockaddr_in server_addr;
    char buffer[BUFF_SIZE];
    const char *message = "Hello from client!";  // 要发送的消息

    // 1. 创建套接字
    // AF_INET: IPv4 协议
    // SOCK_STREAM: TCP 连接 (流式套接字)
    // 0: 默认协议 (TCP)
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 2. 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));  // 清零结构体
    server_addr.sin_family = AF_INET;              // IPv4
    server_addr.sin_port = htons(PORT);  // 服务器端口号，转换为网络字节序
    // 将服务器 IP 地址字符串转换为网络地址结构
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connecting to %s:%d...\n", SERVER_IP, PORT);

    // 3. 连接到服务器
    if (connect(sock_fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == -1) {
        perror("connect failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server.\n");

    // 4. 发送数据到服务器
    // message: 要发送的数据
    // strlen(message): 数据长度
    // 0: 标志位 (通常为 0)
    if (send(sock_fd, message, strlen(message), 0) == -1) {
        perror("send failed");
        close(sock_fd);
        exit(EXIT_FAILURE);
    }

    printf("Sent message to server: '%s'\n", message);

    // 5. (可选) 接收服务器的响应
    // 尽管当前服务器代码没有发送响应，但作为客户端，通常会做好接收准备。
    // 这里我们等待一小段时间，模拟服务器处理或发送。
    // ssize_t bytes_received = recv(sock_fd, buffer, BUFF_SIZE - 1, 0);
    // if (bytes_received > 0) {
    //     buffer[bytes_received] = '\0'; // 确保字符串以空终止符结束
    //     printf("Received response from server: '%s'\n", buffer);
    // } else if (bytes_received == 0) {
    //     printf("Server closed connection.\n");
    // } else {
    //     perror("recv failed");
    // }

    // 由于服务器当前不发送数据，我们简单地等待几秒，然后关闭。
    sleep(2);  // 等待 2 秒，让服务器有时间处理接收到的数据

    // 6. 关闭套接字
    close(sock_fd);
    printf("Connection closed.\n");

    return 0;
}