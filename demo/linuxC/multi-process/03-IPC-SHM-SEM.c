/*
demo for IPC. using semaphore + share memory.
*/

#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define SHM_NAME "/my_shard_memory"
#define SHM_SIZE 1024
#define SEM_NAME "/my_semaphore"

int main() {
    int shm_fd;
    char *shm_ptr;
    sem_t *sem;

    sem = sem_open(SEM_NAME, O_CREAT, 0644, 0);
    if (sem == SEM_FAILED) {
        perror("sem_open failed");
        exit(EXIT_FAILURE);
    }

    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        sem_close(sem);
        sem_unlink(SEM_NAME);
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate failed");
        sem_close(sem);
        sem_unlink(SEM_NAME);
        close(shm_fd);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    shm_ptr = (char *)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                           shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("mmap failed");
        sem_close(sem);
        sem_unlink(SEM_NAME);
        close(shm_fd);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        strcpy(shm_ptr, "Ciao from sub-process!");
        printf("sub-process write in: %s\n", shm_ptr);

        sem_post(sem);

        munmap(shm_ptr, SHM_SIZE);
        close(shm_fd);
        sem_close(sem);
        printf("sub-process end\n");
        exit(0);
    } else {
        sem_wait(sem);

        printf("par-process read out: %s\n", shm_ptr);

        wait(NULL);

        munmap(shm_ptr, SHM_SIZE);
        close(shm_fd);
        shm_unlink(SHM_NAME);

        sem_close(sem);
        sem_unlink(SEM_NAME);
        printf("par-process end\n");
    }

    return 0;
}