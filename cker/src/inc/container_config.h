#ifndef CONTAINER_CONFIG_H
#define CONTAINER_CONFIG_H

typedef struct {
    char *rootfs_path;
    char **argv;
    char **envp;
    char *hostname;
    int clone_flags;
} container_config_t;

#endif