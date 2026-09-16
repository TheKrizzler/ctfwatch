#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <curl/curl.h>

#include "cJSON.h"
#include "log.h"
#include "docker.h"

typedef struct response {
    char *data;
    size_t size;
} response_t;

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t bytes = size * nmemb;
    response_t *res = userdata;

    char *new_data = realloc(
        res->data,
        res->size + bytes + 1
    );

    if (!new_data)
        return 0;

    res->data = new_data;

    memcpy(
        res->data + res->size,
        ptr,
        bytes
    );

    res->size += bytes;
    res->data[res->size] = '\0';

    return bytes;
}

char *get_docker_info(void) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        return NULL;
    }

    response_t res = {0};

    curl_easy_setopt(
        curl,
        CURLOPT_UNIX_SOCKET_PATH,
        DOCKER_SOCK
    );

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        "http://localhost/containers/json"
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        write_callback
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &res
    );

    curl_easy_setopt(
        curl,
        CURLOPT_FAILONERROR,
        1L
    );

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        log_error("Could not reach Docker API: %s", curl_easy_strerror(result));
        free(res.data);
        res.data = NULL;
    }

    curl_easy_cleanup(curl);
    
    return res.data;
}

char *get_docker_container_info(const char *container_id) {
    CURL *curl = curl_easy_init();
    if (!curl)
        return NULL;

    response_t res = {0};
    char url[256];
    snprintf(url, sizeof(url), "http://localhost/containers/%s/json", container_id);

    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, DOCKER_SOCK);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    if (curl_easy_perform(curl) != CURLE_OK) {
        free(res.data);
        res.data = NULL;
    }

    curl_easy_cleanup(curl);
    return res.data;
}

int docker_container_is_tty(const char *container_id) {
    char *info = get_docker_container_info(container_id);
    if (!info)
        return 0;

    cJSON *root = cJSON_Parse(info);
    free(info);
    if (!root)
        return 0;

    cJSON *config = cJSON_GetObjectItem(root, "Config");
    cJSON *tty = cJSON_GetObjectItem(config, "Tty");
    int result = cJSON_IsTrue(tty);
    cJSON_Delete(root);
    return result;
}