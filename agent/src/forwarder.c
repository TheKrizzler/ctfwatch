#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>

#include "queue.h"
#include "forwarder.h"
#include "log.h"
#include "compress.h"
#include "server.h"

#define FLUSH_INTERVAL_SEC 5

void *forwarder_worker(void *arg)
{
    forwarder_args_t *args = arg;

    event_queue_t *queue = args->queue;
    const agent_config_t *config = args->config;

    for (;;) {
        pthread_mutex_lock(&queue->mutex);

        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += FLUSH_INTERVAL_SEC;

        while (queue->bytes < BATCH_TARGET) {
            int result = pthread_cond_timedwait(
                &queue->flush_cond,
                &queue->mutex,
                &deadline
            );

            if (result == ETIMEDOUT)
                break;
        }

        if (queue->count == 0) {
            pthread_mutex_unlock(&queue->mutex);
            continue;
        }

        // batch
        event_batch_t batch = queue_detach(queue);

        pthread_mutex_unlock(&queue->mutex);

        log_debug(
            "Detached batch: %zu events, %zu bytes",
            batch.count,
            batch.bytes
        );

        // serialize batch
        size_t json_size;
        char *json = batch_serialize(&batch, &json_size);

        if (!json) {
            log_error("Failed to serialize event batch");
            batch_destroy(&batch);
            continue;
        }

        log_debug(
            "Serialized batch to %zu bytes of JSON",
            json_size
        );

        // compress
        size_t compressed_size;
        unsigned char *compressed;

        if (gzip_compress(json, json_size, &compressed, &compressed_size) < 0) {
            log_error("Failed to gzip event batch");

            free(json);
            batch_destroy(&batch);
            continue;
        }

        log_debug(
            "Compressed batch: %zu -> %zu bytes",
            json_size,
            compressed_size
        );

        if (server_post_batch(config, compressed, compressed_size) < 0) {
            log_error(
                "Failed to forward batch of %zu events",
                batch.count
            );
        } else {
            log_debug(
                "Forwarded batch of %zu events",
                batch.count
            );
        }

        free(compressed);
        free(json);
        batch_destroy(&batch);
    }

    return NULL;
}