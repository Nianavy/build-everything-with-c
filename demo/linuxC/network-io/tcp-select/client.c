#include <arpa/inet.h>   // IP 地址转换，用于 inet_addr
#include <stdio.h>       // 标准输入输出，用于 printf, perror
#include <stdlib.h>      // 标准库，用于 exit
#include <string.h>      // 字符串操作，用于 memset, strlen
#include <sys/socket.h>  // Socket API，用于 socket, connect, send
#include <unistd.h>      // POSIX 操作系统 API，用于 close

// 定义服务器的端口号和 IP 地址
#define SERVER_PORT 3333
#define SERVER_IP "127.0.0.1"  // 本地主机IP地址
#define BUFF_SIZE 4096  // 定义发送缓冲区大小，与服务器保持一致

int main() {
    int client_fd;                   // 客户端套接字文件描述符
    struct sockaddr_in server_addr;  // 服务器地址结构体
    char buffer[BUFF_SIZE];          // 发送数据的缓冲区
    const char *message = "Hello from client!";  // 要发送的消息

    // 1. 创建套接字 (Socket)
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket error");  // 打印错误信息
        exit(EXIT_FAILURE);      // 退出程序
    }
    printf("Client socket created.\n");

    // 2. 准备服务器地址信息
    memset(&server_addr, 0, sizeof(server_addr));  // 清零结构体
    server_addr.sin_family = AF_INET;              // IPv4
    server_addr.sin_port =
        htons(SERVER_PORT);  // 服务器端口号 (转换为网络字节序)

    // 将 IP 地址字符串转换为网络字节序的二进制形式
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("invalid server IP address");
        close(client_fd);  // 关闭套接字
        exit(EXIT_FAILURE);
    }
    printf("Server address prepared: %s:%d\n", SERVER_IP, SERVER_PORT);

    // 3. 连接到服务器
    if (connect(client_fd, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) == -1) {
        perror("connect error");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    printf("Successfully connected to server.\n");

    // 4. 向服务器发送数据
    if (send(client_fd, message, strlen(message), 0) == -1) {
        perror("send error");
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    printf("Sent message to server: \"%s\"\n", message);

    // 5. 关闭套接字
    close(client_fd);
    printf("Client socket closed. Exiting.\n");

    return 0;
}