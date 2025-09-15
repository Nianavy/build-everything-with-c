#include "inc/container_ns.h"
#include "inc/container_config.h"
#include "inc/container_utils.h"
#include "inc/container_rootfs.h"

#include <sys/types.h>
#include <unistd.h>
#include <sched.h>

int container_entrypoint(void *arg) {
    container_config_t *config = (container_config_t *)arg;
    printf("Container PID %d: Starting in new namespace...\n", getpid());

    if (sethostname(config->hostname, strlen(config->hostname)) == -1) {
        die("sethostname failed");
    }
    printf("Container PID %d: Hostname set to %s\n", getpid(), config->hostname);

    if (setup_rootfs(config) != 0) {
        die("setup_rootfs failed");
    }

    printf("Container PID %d: Executing: %s\n", getpid(), config->argv[0]);
    execve(config->argv[0], config->argv, config->envp);
    die("execve failed");
    return 1;
}