/*
demo test for IO Multiplexing: epoll.
*/

#include <arpa/inet.h>
#include <errno.h> // For errno and EAGAIN
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // For strlen, memset, strncpy
#include <sys/socket.h>
#include <time.h>   // For nanosleep
#include <unistd.h> // For close, sleep

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 3366
#define CLIENT_BUFFER_SIZE 4096 // 客户端缓冲区可以大一些

// Utility for sleeping a bit
void micro_sleep(long us) {
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

int main(int argc, char *argv[]) {
    // 检查命令行参数
    // Usage: ./client [send_size_bytes] [send_chunks] [chunk_delay_us]
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s <send_size_bytes> [send_chunks=1] [chunk_delay_us=0]\n", argv[0]);
        fprintf(stderr, "  <send_size_bytes>: Total number of bytes to send (e.g., 2000, 10000).\n");
        fprintf(stderr, "  [send_chunks]: How many chunks to split the total size into (default: 1, i.e., send all at once).\n");
        fprintf(stderr, "  [chunk_delay_us]: Delay in microseconds between sending chunks (default: 0).\n");
        exit(EXIT_FAILURE);
    }

    long total_send_size = atol(argv[1]);
    int send_chunks = (argc > 2) ? atoi(argv[2]) : 1;
    long chunk_delay_us = (argc > 3) ? atol(argv[3]) : 0;

    if (total_send_size <= 0) {
        fprintf(stderr, "Error: send_size_bytes must be positive.\n");
        exit(EXIT_FAILURE);
    }
    if (send_chunks <= 0) {
        fprintf(stderr, "Error: send_chunks must be positive.\n");
        exit(EXIT_FAILURE);
    }

    size_t bytes_per_chunk = total_send_size / send_chunks;
    if (bytes_per_chunk == 0 && total_send_size > 0) { // If total_send_size is smaller than send_chunks
        bytes_per_chunk = 1;
        send_chunks = total_send_size; // Each chunk is 1 byte
    }
    
    // 确保每个块的大小不超过缓冲区
    if (bytes_per_chunk > CLIENT_BUFFER_SIZE) {
        bytes_per_chunk = CLIENT_BUFFER_SIZE;
        fprintf(stderr, "Warning: bytes_per_chunk adjusted to %zu (max %d) to fit buffer.\n", bytes_per_chunk, CLIENT_BUFFER_SIZE);
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    if (inet_pton(AF_INET, SERVER_IP, &addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    addr.sin_port = htons(SERVER_PORT);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("connect failed");
        close(sock);
        exit(EXIT_FAILURE);
    }
    printf("Client connected to %s:%d\n", SERVER_IP, SERVER_PORT);

    char send_buffer[CLIENT_BUFFER_SIZE];
    char recv_buffer[CLIENT_BUFFER_SIZE];

    // Prepare send buffer with repeating pattern for easy verification
    for (size_t i = 0; i < CLIENT_BUFFER_SIZE; ++i) {
        send_buffer[i] = (i % 26) + 'A'; // A, B, C...
    }

    ssize_t total_bytes_sent = 0;
    ssize_t total_bytes_received = 0;

    // Send loop
    printf("Sending %ld bytes in %d chunks (each %zu bytes, delay %ld us)...\n",
           total_send_size, send_chunks, bytes_per_chunk, chunk_delay_us);

    for (int i = 0; i < send_chunks && total_bytes_sent < total_send_size; ++i) {
        size_t current_chunk_size = bytes_per_chunk;
        if (total_bytes_sent + current_chunk_size > total_send_size) {
            current_chunk_size = total_send_size - total_bytes_sent;
        }

        ssize_t bytes_sent_this_chunk = send(sock, send_buffer, current_chunk_size, 0);
        if (bytes_sent_this_chunk == -1) {
            perror("send failed");
            break;
        }
        if (bytes_sent_this_chunk == 0) {
            printf("Server closed connection during send.\n");
            break;
        }
        total_bytes_sent += bytes_sent_this_chunk;
        printf("  Chunk %d sent: %zd bytes (total: %zd)\n", i + 1, bytes_sent_this_chunk, total_bytes_sent);

        if (total_bytes_sent < total_send_size && chunk_delay_us > 0) {
            micro_sleep(chunk_delay_us);
        }
    }

    printf("Finished sending. Total sent: %zd bytes.\n", total_bytes_sent);

    // Receive loop - try to receive everything sent
    printf("Attempting to receive echoed data...\n");
    while (total_bytes_received < total_bytes_sent) {
        memset(recv_buffer, 0, CLIENT_BUFFER_SIZE);
        ssize_t bytes_received_this_round = recv(sock, recv_buffer, CLIENT_BUFFER_SIZE - 1, 0);

        if (bytes_received_this_round == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // For a blocking client, this usually means no more data.
                // But let's give server some time if it's lagging.
                printf("  recv returned EAGAIN/EWOULDBLOCK. Waiting for more data...\n");
                micro_sleep(10000); // Wait 10ms
                continue;
            }
            perror("recv failed");
            break;
        }
        if (bytes_received_this_round == 0) {
            printf("Server closed the connection gracefully (total received: %zd/%zd).\n", total_bytes_received, total_bytes_sent);
            break;
        }
        
        total_bytes_received += bytes_received_this_round;
        recv_buffer[bytes_received_this_round] = '\0'; // Null-terminate received chunk
        printf("  Received: %zd bytes (total: %zd/%zd). Content starts with: \"%.*s\"...\n",
               bytes_received_this_round, total_bytes_received, total_bytes_sent,
               (int)(bytes_received_this_round > 50 ? 50 : bytes_received_this_round), // Print first 50 chars
               recv_buffer);
        
        // Optional: If you expect to receive more, sleep briefly to let server catch up
        // micro_sleep(1000); 
    }
    printf("Finished receiving. Total received: %zd bytes.\n", total_bytes_received);

    close(sock);
    printf("Client disconnected.\n");

    return 0;
}