#ifndef PROTO_H
#define PROTO_H

typedef enum { STATE_NEW, STATE_CONNECTED, STATE_DISCONNECTED } state_e;

typedef struct {
    int fd;
    state_e state;
    char buffer[4096];
} clientstate_t;

#endif