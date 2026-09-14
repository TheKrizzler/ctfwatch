#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "log.h"
#include "config.h"

agent_config_t *config_load(void)
{
    agent_config_t *config = NULL;
    char *content = NULL;
    cJSON *root = NULL;

    FILE *file = fopen(CONFIG_PATH, "rb");
    if (!file) {
        log_error("Could not open config file: %s", CONFIG_PATH);
        goto fail;
    }

    if (fseek(file, 0, SEEK_END) != 0)
        goto fail_file;

    long file_size = ftell(file);
    if (file_size < 0)
        goto fail_file;

    rewind(file);

    content = malloc((size_t)file_size + 1);
    if (!content)
        goto fail_file;

    size_t bytes_read = fread(
        content,
        1,
        (size_t)file_size,
        file
    );

    fclose(file);
    file = NULL;

    if (bytes_read != (size_t)file_size) {
        log_error("Could not read complete config file");
        goto fail;
    }

    content[bytes_read] = '\0';

    root = cJSON_Parse(content);
    free(content);
    content = NULL;

    if (!root || !cJSON_IsObject(root)) {
        log_error("Invalid JSON in config file");
        goto fail;
    }

    cJSON *server_url =
        cJSON_GetObjectItem(root, "server_url");

    cJSON *token =
        cJSON_GetObjectItem(root, "token");

    if (!cJSON_IsString(server_url) ||
        !cJSON_IsString(token)) {
        log_error(
            "Config requires string fields 'server_url' and 'token'"
        );
        goto fail;
    }

    config = calloc(1, sizeof(*config));
    if (!config)
        goto fail;

    config->server_url = strdup(server_url->valuestring);
    config->token = strdup(token->valuestring);

    if (!config->server_url || !config->token) {
        log_error("Could not allocate config");
        goto fail;
    }

    cJSON_Delete(root);
    return config;

fail_file:
    fclose(file);

fail:
    free(content);
    cJSON_Delete(root);
    config_destroy(config);
    return NULL;
}

void config_destroy(agent_config_t *config)
{
    if (!config)
        return;

    free(config->server_url);
    free(config->token);
    free(config);
}