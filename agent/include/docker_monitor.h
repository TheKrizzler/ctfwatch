#ifndef DOCKER_MONITOR_H
#define DOCKER_MONITOR_H

#include <pthread.h>

#include "queue.h"

int docker_monitor_start(event_queue_t *queue, pthread_t *thread);

#endif