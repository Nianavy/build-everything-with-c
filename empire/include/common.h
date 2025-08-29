#ifndef COMMON_H
#define COMMON_H

#define STATUS_ERROR    -1
#define STATUS_SUCCESS  0

typedef enum {
    STATE_NEW,
    STATE_CONNECTED,
    STATE_DISCONNECTED
} state_e;

typedef struct {
    int fd;
    state_e state;
    char buffer[4096];
} clientstate_t;

#endif