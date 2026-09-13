#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "log.h"
#include "cJSON.h"
#include "profile.h"
#include "docker.h"

container_profile_t *profile_build(const struct sockaddr_storage *peer_addr, socklen_t peer_addr_len) {
    container_profile_t *profile = NULL;
    char *docker_info = NULL;
    cJSON *root = NULL;

    profile = calloc(1, sizeof(*profile)); // all NULL    
    
    if (!profile) {
        log_error("Failed to create container profile. Memory allocation failed.");
        return NULL;
    }

    profile->ip_addr = *peer_addr;
    profile->ip_addr_len = peer_addr_len;

    docker_info = get_docker_info();

    if (!docker_info) {
        log_error("Could not retrieve Docker info");
        goto fail;
    }

    root = cJSON_Parse(docker_info);

    free(docker_info);
    docker_info = NULL;

    if (!root) {
        log_error("Failed to parse Docker response");
        goto fail;
    }

    if (!cJSON_IsArray(root)) {
        log_error("Docker response is not an array");
        goto fail;
    }

    // ip string representation
    char ip_string[INET6_ADDRSTRLEN];

    if (!inet_ntop(
            AF_INET,
            &((const struct sockaddr_in *)&profile->ip_addr)->sin_addr,
            ip_string,
            sizeof(ip_string)
        )) {
        log_error("Could not convert peer IP address.");
        goto fail;
    }

    // find the correct container
    cJSON *container;
    cJSON *matched_container = NULL;

    cJSON_ArrayForEach(container, root) {
        cJSON *network_settings =
            cJSON_GetObjectItem(container, "NetworkSettings");

        cJSON *networks =
            cJSON_GetObjectItem(network_settings, "Networks");

        cJSON *ctfwatch_network =
            cJSON_GetObjectItem(networks, "ctfwatch-net");

        cJSON *ip =
            cJSON_GetObjectItem(ctfwatch_network, "IPAddress");
        
        if (cJSON_IsString(ip) && strcmp(ip->valuestring, ip_string) == 0) {
            matched_container = container;
            break;
        }
    }

    if (!matched_container) {
        log_warn("Could not map %s to a container", ip_string);
        goto fail;
    }

    // get important stuff
    cJSON *id = cJSON_GetObjectItem(matched_container, "Id");
    cJSON *names = cJSON_GetObjectItem(matched_container, "Names");
    cJSON *name = cJSON_GetArrayItem(names, 0);
    cJSON *image = cJSON_GetObjectItem(matched_container, "Image");

    cJSON *labels = cJSON_GetObjectItem(matched_container, "Labels");
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

    cJSON_Delete(root);
    return profile;

fail:
    free(docker_info);
    cJSON_Delete(root);
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