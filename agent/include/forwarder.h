#ifndef FORWARDER_H
#define FORWARDER_H

#include "queue.h"
#include "config.h"

typedef struct forwarder_args {
    event_queue_t *queue;
    const agent_config_t *config;
} forwarder_args_t;

void *forwarder_worker(void *arg);

#endif