/*
demo for IPC. using message queue.
*/

#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define QUEUE_NAME "/my_message_queue"
#define MAX_SIZE 1024
#define MSG_STOP "exit"

int main() {
    mqd_t mq;
    struct mq_attr attr;
    char buffer[MAX_SIZE + 1];
    char message[MAX_SIZE];

    // 可选：清理可能残留的消息队列
    mq_unlink(QUEUE_NAME);

    // 设置消息队列属性
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = MAX_SIZE;
    attr.mq_curmsgs = 0;

    // 创建消息队列
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDWR, 0644, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open failed");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(1);
    } else if (pid == 0) {
        // 子进程：消费者
        while (1) {
            ssize_t bytes_read = mq_receive(mq, buffer, MAX_SIZE, NULL);
            if (bytes_read == -1) {
                perror("mq_receive failed");
                exit(1);
            }
            buffer[bytes_read] = '\0';
            printf("sub-process received: %s\n", buffer);

            if (strcmp(buffer, MSG_STOP) == 0) { break; }
        }

        if (mq_close(mq) == -1) { perror("sub-process mq_close failed"); }
        exit(0);
    } else {
        // 父进程：生产者
        for (int i = 0; i < 3; ++i) {
            if (i == 2) {
                strcpy(message, MSG_STOP);
            } else {
                strcpy(message, "hello from parent");
            }

            if (mq_send(mq, message, strlen(message), 0) == -1) {
                perror("mq_send failed");
                exit(1);
            }
            printf("par-process sended: %s\n", message);
        }

        wait(NULL);  // 等待子进程结束

        if (mq_close(mq) == -1) { perror("par-process mq_close failed"); }
        if (mq_unlink(QUEUE_NAME) == -1) { perror("mq_unlink failed"); }
    }

    return 0;
}