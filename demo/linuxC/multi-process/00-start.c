/*
demo for multi process.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

int main() {
    pid_t pid;
    int status;

    // fork()
    pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(1);
    }
    else if (pid == 0) {
        printf("sub-process: PID = %d\n", getpid());
        execl("/bin/ls", "ls", "-l", NULL);
        perror("exec failed");
        exit(1);
    }
    else {
        printf("par-process: PID = %d, sub-process: PID = %d\n", getpid(), pid);
        wait(&status);
        printf("sub-process is end.\n");
    }

    return 0;
}