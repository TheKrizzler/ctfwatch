#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "log.h"
#include "protocol.h"
#include "profile.h"

// receive exactly len bytes 
static int recv_bytes(int fd, void *buf, size_t len)
{
    size_t received = 0;

    while (received < len) {
        ssize_t n = recv(
            fd,
            (char *)buf + received,
            len - received,
            0
        );

        if (n <= 0)
            return -1;

        received += n;
    }

    return 0;
}

int new_protocol_socket(void) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        log_fatal("Could not establish socket! Exiting...\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(CTFWATCH_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_fatal("Could not bind socket! Exiting...\n");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(socket_fd, SOMAXCONN) < 0) {
        log_fatal("Could not listen()! Exiting...\n");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    log_info("ctfwatch agent listening on port %i...", CTFWATCH_PORT);

    return socket_fd;
}

void *protocol_worker(void *args) {
    worker_args_t w_args = *(worker_args_t *)args;

    // ownership of args transferred
    free(args);

    // build profile
    // code will go here

    log_info("Established new incoming connection with 127.0.0.1...");

    // communication
    for (;;) {
        // get size
        uint32_t event_size_net;
        if(recv_bytes(w_args.connection_fd, &event_size_net, sizeof(event_size_net)) < 0) {
            log_warn("Error while receiving size! Killing connection...");
            break;
        }

        uint32_t event_size = ntohl(event_size_net);        

        if (event_size > MAX_EVENT_SIZE) {
            log_warn("Received an event too large! Killing connection...");
            break;
        }

        if (event_size == 0) {
            send(w_args.connection_fd, PROTOCOL_RES_OK, PROTOCOL_RES_LEN, 0);
            continue;
        }

        // get content
        char *event_content = malloc(event_size);
        if (!event_content) {
            log_warn("Failed to allocate memory for event! Killing connection...");
            break;
        }

        if (recv_bytes(w_args.connection_fd, event_content, (size_t)event_size) < 0) {
            log_warn("Error while receiving content! Killing connection...");
            free(event_content);
            break;
        }

        // aggregation code (jsonify, enrich, compress)
        // will transfer ownership of event_content and be responsible for sending to queue
    }

    close(w_args.connection_fd);

    return NULL;
}

