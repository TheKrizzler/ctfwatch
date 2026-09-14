#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "log.h"
#include "protocol.h"
#include "profile.h"
#include "event.h"

// receive exactly len bytes 
static int recv_bytes(int fd, void *buf, size_t len) {
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

static void log_connection(const container_profile_t* profile) {
    char ip[INET6_ADDRSTRLEN];

    inet_ntop(
        AF_INET,
        &((struct sockaddr_in *)&profile->ip_addr)->sin_addr,
        ip,
        sizeof(ip)
    );

    log_info("Established incoming connection from %s (id: %s, name: %s)", ip, profile->container_id, profile->container_name);
}

int new_protocol_socket(void) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        log_fatal("Could not establish socket! Exiting...\n");
        exit(EXIT_FAILURE);
    }

    int yes = 1;

    if (setsockopt(
            socket_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &yes,
            sizeof(yes)
        ) < 0) {
        log_warn("Could not set SO_REUSEADDR");
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
    container_profile_t *profile = profile_build(
        &w_args.peer_addr,
        w_args.peer_addr_len
    );

    if (!profile) {
        log_warn("Could not build container profile. Killing connection...");
        close(w_args.connection_fd);
        return NULL;
    }

    log_connection(profile);

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

        // aggregation
        size_t enriched_size;

        char *enriched = event_enrich(
            event_content,
            event_size,
            profile,
            &enriched_size
        );

        free(event_content);

        if (!enriched) {
            log_warn("Failed to parse/enrich event");
            break;
        }

        event_t *event = malloc(sizeof(*event));

        if (!event) {
            log_warn("Failed to allocate event");
            free(enriched);
            break;
        }

        event->data = enriched;
        event->size = enriched_size;

        if (queue_push(w_args.queue, event) < 0) {
            log_warn("Failed to queue event");
            event_destroy(event);
            break;
        }

        send(w_args.connection_fd, PROTOCOL_RES_OK, PROTOCOL_RES_LEN, 0);        
    }

    profile_destroy(profile);
    close(w_args.connection_fd);

    return NULL;
}

