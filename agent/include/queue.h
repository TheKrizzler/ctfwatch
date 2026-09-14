#ifndef QUEUE_H
#define QUEUE_H

#include <stddef.h>
#include <pthread.h>

#include "event.h"

#define BATCH_TARGET (1024 * 1024)

typedef struct queue_node {
    event_t *event;
    struct queue_node *next;
} queue_node_t;

typedef struct event_queue {
    queue_node_t *head;
    queue_node_t *tail;

    size_t count;
    size_t bytes;

    pthread_mutex_t mutex;
    pthread_cond_t flush_cond;
} event_queue_t;

typedef struct event_batch {
    queue_node_t *head;
    queue_node_t *tail;

    size_t count;
    size_t bytes;
} event_batch_t;

int queue_push(event_queue_t *queue, event_t *event);
event_t *queue_pop(event_queue_t *queue);
int queue_init(event_queue_t *queue);
void queue_destroy(event_queue_t *queue);
event_batch_t queue_detach(event_queue_t *queue);
void batch_destroy(event_batch_t *batch);
char *batch_serialize(const event_batch_t *batch, size_t *size);

#endif