#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>

#include "log.h"
#include "protocol.h"

int main(int argc, char *argv[]) {
    int socket_fd = new_protocol_socket();
    pthread_t worker_id;

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
    
        if (pthread_create(&worker_id, NULL, protocol_worker, worker_args) != 0) {
            log_error("Worker thread could not be created. Closing connection %i...", connection_fd);
            close(connection_fd);
            free(worker_args);
            continue;
        }
        
        pthread_detach(worker_id);
    }

    return 0;
}