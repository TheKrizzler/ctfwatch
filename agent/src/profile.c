#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "log.h"
#include "cJSON.h"
#include "profile.h"
#include "docker.h"

container_profile_t *profile_build_from_container(const cJSON *container) {
    container_profile_t *profile = NULL;

    profile = calloc(1, sizeof(*profile)); // all NULL    
    
    if (!profile) {
        log_error("Failed to create container profile. Memory allocation failed.");
        return NULL;
    }

    cJSON *id = cJSON_GetObjectItem(container, "Id");
    cJSON *names = cJSON_GetObjectItem(container, "Names");
    cJSON *name = cJSON_GetArrayItem(names, 0);
    cJSON *image = cJSON_GetObjectItem(container, "Image");

    cJSON *labels = cJSON_GetObjectItem(container, "Labels");
    cJSON *compose_project = cJSON_GetObjectItem(labels, "com.docker.compose.project");
    cJSON *compose_service = cJSON_GetObjectItem(labels, "com.docker.compose.service");

    // populate
    if (cJSON_IsString(id)) {
        profile->container_id = strdup(id->valuestring);
    }

    if (cJSON_IsString(name)) {
        profile->container_name = strdup(name->valuestring);
    }

    if (cJSON_IsString(image)) {
        profile->image_name = strdup(image->valuestring);
    }

    if (cJSON_IsString(compose_service)) {
        profile->compose_service = strdup(compose_service->valuestring);
    }

    if (cJSON_IsString(compose_project)) {
        profile->compose_project = strdup(compose_project->valuestring);
    }

    if (!profile->container_id || !profile->container_name) {
        log_error("Could not associate IP with Docker ID or name");
        goto fail;
    }

    return profile;

fail:
    profile_destroy(profile);
    return NULL;
}

container_profile_t *profile_build(const struct sockaddr_storage *peer_addr, socklen_t peer_addr_len) {
    char *docker_info = get_docker_info();
    cJSON *root = NULL;
    container_profile_t *profile = NULL;

    if (!docker_info) {
        log_error("Could not retrieve Docker info");
        return NULL;
    }

    root = cJSON_Parse(docker_info);
    free(docker_info);

    if (!root || !cJSON_IsArray(root)) {
        log_error("Docker response is not an array");
        cJSON_Delete(root);
        return NULL;
    }

    char ip_string[INET6_ADDRSTRLEN];
    if (!inet_ntop(
            AF_INET,
            &((const struct sockaddr_in *)peer_addr)->sin_addr,
            ip_string,
            sizeof(ip_string)
        )) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *container;
    cJSON *matched_container = NULL;
    cJSON_ArrayForEach(container, root) {
        cJSON *network_settings = cJSON_GetObjectItem(container, "NetworkSettings");
        cJSON *networks = cJSON_GetObjectItem(network_settings, "Networks");
        cJSON *ctfwatch_network = cJSON_GetObjectItem(networks, CTFWATCH_NETWORK);
        cJSON *ip = cJSON_GetObjectItem(ctfwatch_network, "IPAddress");

        if (cJSON_IsString(ip) && strcmp(ip->valuestring, ip_string) == 0) {
            matched_container = container;
            break;
        }
    }

    if (matched_container)
        profile = profile_build_from_container(matched_container);
    else
        log_warn("Could not map %s to a container", ip_string);

    if (profile) {
        profile->ip_addr = *peer_addr;
        profile->ip_addr_len = peer_addr_len;
    }

    cJSON_Delete(root);
    return profile;
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