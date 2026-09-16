#ifndef PROFILE_H
#define PROFILE_H

#include <sys/socket.h>

typedef struct cJSON cJSON;

typedef struct container_profile {
    // essential fields
    struct sockaddr_storage ip_addr;
    socklen_t ip_addr_len;

    char *container_id;
    char *container_name;
    
    // optional fields. may be NULL
    char *image_name;
    char *compose_project; // NULL if not Compose
    char *compose_service; // NULL if not Compose
} container_profile_t;

container_profile_t *profile_build(
    const struct sockaddr_storage *peer_addr,
    socklen_t peer_addr_len
);

container_profile_t *profile_build_from_container(const cJSON *container);

void profile_destroy(container_profile_t *profile);

#endif