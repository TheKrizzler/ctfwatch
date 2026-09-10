#include <stdlib.h>
#include <sys/socket.h>

#include "log.h"
#include "profile.h"

container_profile_t *profile_build(const struct sockaddr_storage *peer_addr, socklen_t peer_addr_len) {
    container_profile_t *profile = calloc(1, sizeof(*profile)); // all NULL
    if (!profile) {
        log_error("Failed to create container profile. Memory allocation failed.");
        return NULL;
    }

    // code

    return profile;

fail:
    profile_destroy(profile);
    return NULL;
}

void profile_destroy(container_profile_t *profile) {
    if (!profile) {
        return;
    }

    free(profile->container_id);
    free(profile->container_name);
    free(profile->image_name);
    free(profile->compose_project);
    free(profile->compose_service);

    free(profile);
}