#ifndef EVENT_H
#define EVENT_H

#include <stddef.h>

#include "profile.h"

typedef struct event {
    char *data;
    size_t size;
} event_t;

char *event_enrich(
    const char *data,
    size_t size,
    const container_profile_t *profile,
    size_t *output_size
);
void event_destroy(event_t *event);

#endif