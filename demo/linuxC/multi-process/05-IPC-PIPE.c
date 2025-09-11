/*
demo for IPC. using pipe.
*/

#ifndef ANONYMOUS
#define ANONYMOUS 0
#endif

#if ANONYMOUS == 1
// 匿名管道代码
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    int pipefd[2];
    pid_t pid;
    char write_msg[] = "Hello from par!";
    char read_msg[100];

    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        exit(1);
    }

    pid = fork();

    if (pid == -1) {
        perror("fork failed");
        exit(1);
    } else if (pid == 0) {
        close(pipefd[1]);
        read(pipefd[0], read_msg, sizeof(read_msg));
        printf("sub-process read out: %s\n", read_msg);
        close(pipefd[0]);
    } else {
        close(pipefd[0]);
        write(pipefd[1], write_msg, strlen(write_msg) + 1);
        printf("par-process write in: %s\n", write_msg);
        close(pipefd[1]);
        wait(NULL);
    }
    return 0;
}

#else
// 命名管道（FIFO）代码
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main() {
    const char *fifo_name = "/tmp/my_fifo123";
    char message[] = "Hello through named pipe!";
    char buffer[100];

    if (mkfifo(fifo_name, 0666) == -1) {
        perror("mkfifo failed");
        exit(1);
    }

    if (fork() == 0) {
        int fd = open(fifo_name, O_WRONLY);
        write(fd, message, strlen(message) + 1);
        printf("sub-process write in: %s\n", message);
        close(fd);
    } else {
        int fd = open(fifo_name, O_RDONLY);
        read(fd, buffer, sizeof(buffer));
        printf("par-process read out: %s\n", buffer);
        close(fd);

        unlink(fifo_name);
    }

    return 0;
}

#endif