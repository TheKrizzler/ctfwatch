#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sys/socket.h>

#include "queue.h"

#define CTFWATCH_PORT 9494
#define MAX_EVENT_SIZE (1024 * 1024) // 1MiB
#define PROTOCOL_RES_LEN 2
#define PROTOCOL_RES_OK "OK"
#define PROTOCOL_RES_FAIL "XX"

typedef struct worker_args {
    int connection_fd;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;

    event_queue_t *queue;
} worker_args_t;

int new_protocol_socket(void); // creates and binds a socket, returns socket fd
void *protocol_worker(void *args); // handles an incoming connection

#endif