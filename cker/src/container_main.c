#include "inc/container_utils.h"
#include "inc/container_ns.h"
#include "inc/container_config.h"

#include <stdlib.h>
#include <sys/wait.h>
#include <sched.h>

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <rootfs_path> <command> [args...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    container_config_t config;
    config.rootfs_path = argv[1];
    config.argv = &argv[2];
    config.envp = (char *[]){"PATH=/bin:/usr/bin", NULL};
    config.hostname = "my-linux-container";

    config.clone_flags = CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWNS | SIGCHLD;

    printf("Parent PID: %d\n", getpid());
    printf("Rootfs Path: %s\n", config.rootfs_path);
    printf("Command: %s\n", config.argv[0]);
    printf("Clone Flags: 0x%x\n", config.clone_flags);

    char *stack = malloc(1024 * 1024);
    if (!stack) {
        die("malloc failed");
    }

    pid_t container_pid = clone(container_entrypoint, stack + (1024 * 1024), config.clone_flags, &config);

    if (container_pid == -1) {
        die("clone failed");
    }

    printf("Parent PID %d: Container process created with PID: %d\n", getpid(), container_pid);

    int status;
    if (waitpid(container_pid, &status, 0) == -1) {
        die("waitpid failed");
    }

    printf("Parent PID %d: Container finished with status %d.\n", getpid(), WEXITSTATUS(status));
    free(stack);
    return WEXITSTATUS(status);
}
