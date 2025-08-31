/*
demo for IPC. using semaphore.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>

#define NAME "/my_demo_semaphore"

int main() {
    sem_t *sem;
    sem = sem_open(NAME, O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        sem_unlink(NAME);  // 清理可能残留的信号量
        perror("sem_open failed");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(1);
    }
    else if (pid == 0) {
        // 子进程
        puts("sub-process waiting the semaphore...");
        sem_wait(sem);

        puts("sub-process get the semaphore");
        sleep(2);  // 模拟临界区操作
        sem_post(sem);

        puts("sub-process post the semaphore");
        sem_close(sem);  // 显式关闭
        exit(0);  // 子进程结束
    }
    else {
        // 父进程
        puts("par-process waiting the semaphore...");
        sem_wait(sem);

        puts("par-process get the semaphore");
        sleep(2);  // 模拟临界区操作
        sem_post(sem);

        puts("par-process post the semaphore");

        wait(NULL);  // 等待子进程

        sem_close(sem);
        if (sem_unlink(NAME) == -1) {
            perror("sem_unlink failed");
        }
    }

    return 0;
}