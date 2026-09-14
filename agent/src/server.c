#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include "log.h"
#include "server.h"

int server_post_batch(const agent_config_t *config, const unsigned char *data, size_t size) {
    CURL *curl = curl_easy_init();
    if (!curl)
        return -1;

    size_t auth_size =
        strlen("Authorization: Bearer ") +
        strlen(config->token) +
        1;

    char *auth_header = malloc(auth_size);

    if (!auth_header) {
        curl_easy_cleanup(curl);
        return -1;
    }

    snprintf(
        auth_header,
        auth_size,
        "Authorization: Bearer %s",
        config->token
    );

    struct curl_slist *headers = NULL;

    headers = curl_slist_append(
        headers,
        "Content-Type: application/json"
    );

    headers = curl_slist_append(
        headers,
        "Content-Encoding: gzip"
    );

    headers = curl_slist_append(
        headers,
        auth_header
    );

    free(auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, config->server_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDSIZE_LARGE,
        (curl_off_t)size
    );

    CURLcode result = curl_easy_perform(curl);

    long status = 0;
    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &status
    );

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        log_error(
            "Failed to send batch: %s",
            curl_easy_strerror(result)
        );
        return -1;
    }

    if (status < 200 || status >= 300) {
        log_error(
            "Server rejected batch with HTTP %ld",
            status
        );
        return -1;
    }

    return 0;
}