#ifndef PROTO_H
#define PROTO_H

typedef enum {
    PROTO_HELLO,
} proto_type_e;

typedef struct {
    proto_type_e type;
    unsigned short len;
    unsigned char payload[];
} proto_hdr_t;

#endif