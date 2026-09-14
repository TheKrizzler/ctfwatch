#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "log.h"
#include "queue.h"

int queue_push(event_queue_t *queue, event_t *event)
{
    queue_node_t *node = malloc(sizeof(*node));

    if (!node)
        return -1;

    node->event = event;
    node->next = NULL;

    pthread_mutex_lock(&queue->mutex);

    if (!queue->tail) {
        queue->head = node;
        queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }

    queue->count++;
    queue->bytes += event->size;

    if (queue->bytes >= BATCH_TARGET) {
        pthread_cond_signal(&queue->flush_cond);
    }

    pthread_mutex_unlock(&queue->mutex);

    return 0;
}

event_t *queue_pop(event_queue_t *queue)
{
    pthread_mutex_lock(&queue->mutex);

    if (!queue->head) {
        pthread_mutex_unlock(&queue->mutex);
        return NULL;
    }

    queue_node_t *node = queue->head;
    event_t *event = node->event;

    queue->head = node->next;

    if (!queue->head)
        queue->tail = NULL;

    queue->count--;
    queue->bytes -= event->size;

    pthread_mutex_unlock(&queue->mutex);

    free(node);

    return event;
}

int queue_init(event_queue_t *queue)
{
    memset(queue, 0, sizeof(*queue));

    if (pthread_mutex_init(&queue->mutex, NULL) != 0)
        return -1;

    if (pthread_cond_init(&queue->flush_cond, NULL) != 0) {
        pthread_mutex_destroy(&queue->mutex);
        return -1;
    }

    return 0;
}

void queue_destroy(event_queue_t *queue)
{
    event_t *event;

    while ((event = queue_pop(queue)) != NULL)
        event_destroy(event);

    pthread_cond_destroy(&queue->flush_cond);
    pthread_mutex_destroy(&queue->mutex);
}

event_batch_t queue_detach(event_queue_t *queue)
{
    event_batch_t batch = {0};

    /*
     * Caller must hold queue->mutex.
     */

    batch.head = queue->head;
    batch.tail = queue->tail;
    batch.count = queue->count;
    batch.bytes = queue->bytes;

    queue->head = NULL;
    queue->tail = NULL;
    queue->count = 0;
    queue->bytes = 0;

    return batch;
}

void batch_destroy(event_batch_t *batch)
{
    queue_node_t *node = batch->head;

    while (node) {
        queue_node_t *next = node->next;

        event_destroy(node->event);
        free(node);

        node = next;
    }

    batch->head = NULL;
    batch->tail = NULL;
    batch->count = 0;
    batch->bytes = 0;
}

char *batch_serialize(const event_batch_t *batch, size_t *size)
{
    cJSON *root = cJSON_CreateObject();
    if (!root)
        return NULL;

    cJSON *events = cJSON_AddArrayToObject(root, "events");
    if (!events) {
        cJSON_Delete(root);
        return NULL;
    }

    for (queue_node_t *node = batch->head;
         node != NULL;
         node = node->next) {

        event_t *event = node->event;

        cJSON *json = cJSON_ParseWithLength(
            event->data,
            event->size
        );

        if (!json) {
            log_warn("Skipping invalid JSON event");
            continue;
        }

        cJSON_AddItemToArray(events, json);
    }

    char *serialized = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!serialized)
        return NULL;

    *size = strlen(serialized);

    return serialized;
}