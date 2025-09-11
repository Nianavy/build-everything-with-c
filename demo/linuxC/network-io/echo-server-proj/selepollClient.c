/*
demo test for IO Multiplexing: select | poll.
*/

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>  // For strlen, memset
#include <sys/socket.h>
#include <unistd.h>  // For close, sleep

#define BUFFER_SIZE 1024  // 定义一个合理的缓冲区大小

int main(int argc, char *argv[]) {
    // 检查命令行参数
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <message_to_send>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));  // 清零结构体
    addr.sin_family = AF_INET;

    // 使用 inet_pton 代替 inet_addr，更现代，支持 IPv6
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    addr.sin_port = htons(6666);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Connected to server at 127.0.0.1:6666\n");

    char send_buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE];
    const char *message_to_send = argv[1];
    size_t message_len = strlen(message_to_send);

    if (message_len >= BUFFER_SIZE) {
        fprintf(stderr, "Error: Message to send is too long (max %d bytes).\n",
                BUFFER_SIZE - 1);
        close(sock);
        exit(EXIT_FAILURE);
    }

    // 将要发送的消息复制到发送缓冲区
    strncpy(send_buffer, message_to_send, BUFFER_SIZE - 1);
    send_buffer[BUFFER_SIZE - 1] = '\0';  // 确保字符串以 null 结尾

    ssize_t bytes_sent;
    ssize_t bytes_received;

    while (1) {
        // 发送数据
        bytes_sent = send(sock, send_buffer, message_len, 0);  // 发送实际长度
        if (bytes_sent == -1) {
            perror("send failed");
            break;  // 跳出循环，进行清理
        }
        if (bytes_sent == 0) {
            printf("Server closed the connection unexpectedly during send.\n");
            break;
        }
        printf("Sent to server: \"%s\" (%zd bytes)\n", send_buffer, bytes_sent);

        // 接收数据
        // 接收前清零缓冲区，避免打印旧数据或垃圾
        memset(recv_buffer, 0, BUFFER_SIZE);
        bytes_received = recv(sock, recv_buffer, BUFFER_SIZE - 1,
                              0);  // 接收到缓冲区，留一个字节给 '\0'
        if (bytes_received == -1) {
            perror("recv failed");
            break;  // 跳出循环，进行清理
        }
        if (bytes_received == 0) {
            printf("Server closed the connection gracefully.\n");
            break;  // 服务器关闭连接
        }
        recv_buffer[bytes_received] = '\0';  // 确保接收到的数据以 null 结尾
        printf("Received from server: \"%s\" (%zd bytes)\n", recv_buffer,
               bytes_received);

        sleep(1);
    }

    close(sock);
    printf("Client disconnected.\n");

    return 0;
}