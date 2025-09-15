#include "inc/container_utils.h"

void die(const char *msg) {
    fprintf(stderr, "[!] %s: %s\n", msg, strerror(errno));
    exit(EXIT_FAILURE);
}

