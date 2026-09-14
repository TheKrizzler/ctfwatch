#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>

#include "config.h"

int server_post_batch(const agent_config_t *config, const unsigned char *data, size_t size);

#endif