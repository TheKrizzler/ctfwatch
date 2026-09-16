#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <curl/curl.h>

#include "log.h"
#include "protocol.h"
#include "queue.h"
#include "forwarder.h"
#include "config.h"
#include "docker_monitor.h"

int main(int argc, char *argv[]) {
    agent_config_t *config = config_load();

    if (!config) {
        log_fatal("Could not load agent configuration");
        return EXIT_FAILURE;
    }
    
    pthread_t worker_id;

    if (argc > 2) {
        log_fatal("Usage: %s <config>", argv[0]);
        return EXIT_FAILURE;
    }

    // libcurl init
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        log_fatal("Could not initialize libcurl");
        return EXIT_FAILURE;
    }

    // initialize event queue
    event_queue_t queue;
    if (queue_init(&queue) < 0) {
        log_fatal("Could not initialize event queue");
        return EXIT_FAILURE;
    }

    forwarder_args_t forwarder_args = {
        .queue = &queue,
        .config = config
    };

    // initialize event forwarder
    pthread_t forwarder_thread;

    if (pthread_create(
            &forwarder_thread,
            NULL,
            forwarder_worker,
            &forwarder_args
        ) != 0) {
        log_fatal("Could not start forwarder thread");
        queue_destroy(&queue);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    pthread_t docker_monitor_thread;
    if (docker_monitor_start(&queue, &docker_monitor_thread) != 0) {
        log_fatal("Could not start Docker log monitor");
        queue_destroy(&queue);
        config_destroy(config);
        return EXIT_FAILURE;
    }

    int socket_fd = new_protocol_socket();

    for (;;) {
        worker_args_t *worker_args = malloc(sizeof(*worker_args));
        if (!worker_args) {
            log_error("Unable to allocate worker thread arguments!");
            continue;
        }

        worker_args->peer_addr_len = sizeof(worker_args->peer_addr);

        int connection_fd = accept(
            socket_fd, 
            (struct sockaddr *)&worker_args->peer_addr, 
            &worker_args->peer_addr_len
        );

        if (connection_fd < 0) {
            log_error("Unable to accept incoming connection!");
            free(worker_args);
            continue;
        }

        worker_args->connection_fd = connection_fd;
        worker_args->queue = &queue;
    
        if (pthread_create(&worker_id, NULL, protocol_worker, worker_args) != 0) {
            log_error("Worker thread could not be created. Closing connection %i...", connection_fd);
            close(connection_fd);
            free(worker_args);
            continue;
        }
        
        pthread_detach(worker_id);
    }

    curl_global_cleanup();
    return 0;
}