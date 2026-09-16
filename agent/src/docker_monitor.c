#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <curl/curl.h>

#include "cJSON.h"
#include "docker.h"
#include "docker_monitor.h"
#include "event.h"
#include "log.h"
#include "profile.h"
#include "protocol.h"

#define MONITOR_INTERVAL_SEC 5
#define LOGS_URL_SIZE 256

typedef struct monitored_container {
    char *id;
    struct monitored_container *next;
} monitored_container_t;

typedef struct docker_monitor {
    event_queue_t *queue;
    monitored_container_t *containers;
    pthread_mutex_t lock;
} docker_monitor_t;

typedef struct log_stream {
    unsigned char *data;
    size_t size;
    size_t capacity;
    size_t discard;
    int tty;
    event_queue_t *queue;
    container_profile_t *profile;
} log_stream_t;

typedef struct log_reader_args {
    docker_monitor_t *monitor;
    container_profile_t *profile;
    char *container_id;
} log_reader_args_t;

static int monitor_contains(docker_monitor_t *monitor, const char *id)
{
    int found = 0;

    pthread_mutex_lock(&monitor->lock);
    for (monitored_container_t *container = monitor->containers;
         container != NULL;
         container = container->next) {
        if (strcmp(container->id, id) == 0) {
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&monitor->lock);
    return found;
}

static void monitor_remove(docker_monitor_t *monitor, const char *id)
{
    pthread_mutex_lock(&monitor->lock);

    monitored_container_t **current = &monitor->containers;
    while (*current != NULL) {
        monitored_container_t *container = *current;
        if (strcmp(container->id, id) == 0) {
            *current = container->next;
            free(container->id);
            free(container);
            break;
        }
        current = &container->next;
    }

    pthread_mutex_unlock(&monitor->lock);
}

static int stream_reserve(log_stream_t *stream, size_t additional)
{
    if (additional > SIZE_MAX - stream->size)
        return -1;

    size_t required = stream->size + additional;
    if (required <= stream->capacity)
        return 0;

    size_t capacity = stream->capacity ? stream->capacity * 2 : 4096;
    while (capacity < required) {
        if (capacity > SIZE_MAX / 2)
            capacity = required;
        else
            capacity *= 2;
    }

    unsigned char *data = realloc(stream->data, capacity);
    if (!data)
        return -1;

    stream->data = data;
    stream->capacity = capacity;
    return 0;
}

static void stream_consume(log_stream_t *stream, size_t amount)
{
    if (amount >= stream->size) {
        stream->size = 0;
        return;
    }

    memmove(stream->data, stream->data + amount, stream->size - amount);
    stream->size -= amount;
}

static void stream_emit(log_stream_t *stream, const unsigned char *message,
                        size_t message_size, const char *stream_name)
{
    cJSON *root = cJSON_CreateObject();
    cJSON *log = NULL;
    char *serialized = NULL;
    event_t *event = NULL;
    size_t enriched_size = 0;
    char *enriched = NULL;

    if (!root)
        return;

    char *message_copy = malloc(message_size + 1);
    if (!message_copy)
        goto done;

    memcpy(message_copy, message, message_size);
    message_copy[message_size] = '\0';

    if (!cJSON_AddStringToObject(root, "message", message_copy)) {
        free(message_copy);
        goto done;
    }
    free(message_copy);

    log = cJSON_AddObjectToObject(root, "log");
    if (!log || !cJSON_AddStringToObject(log, "stream", stream_name))
        goto done;

    serialized = cJSON_PrintUnformatted(root);
    if (!serialized)
        goto done;

    enriched = event_enrich(
        serialized,
        strlen(serialized),
        stream->profile,
        &enriched_size
    );
    if (!enriched)
        goto done;

    event = malloc(sizeof(*event));
    if (!event)
        goto done;

    event->data = enriched;
    event->size = enriched_size;
    if (queue_push(stream->queue, event) < 0) {
        event_destroy(event);
        enriched = NULL;
        event = NULL;
    }

done:
    free(serialized);
    if (!event)
        free(enriched);
    cJSON_Delete(root);
}

static void stream_feed_tty(log_stream_t *stream,
                            const unsigned char *data, size_t length)
{
    if (stream_reserve(stream, length) < 0)
        return;

    memcpy(stream->data + stream->size, data, length);
    stream->size += length;

    size_t start = 0;
    for (size_t index = 0; index < stream->size; index++) {
        if (stream->data[index] != '\n')
            continue;

        stream_emit(stream, stream->data + start, index - start, "stdout");
        start = index + 1;
    }

    if (start > 0)
        stream_consume(stream, start);
}

static void stream_feed_multiplexed(log_stream_t *stream,
                                    const unsigned char *data, size_t length)
{
    if (stream_reserve(stream, length) < 0)
        return;

    memcpy(stream->data + stream->size, data, length);
    stream->size += length;

    for (;;) {
        if (stream->discard > 0) {
            size_t discarded = stream->discard < stream->size
                ? stream->discard
                : stream->size;
            stream_consume(stream, discarded);
            stream->discard -= discarded;
            if (stream->discard > 0)
                return;
        }

        if (stream->size < 8)
            return;

        uint32_t message_size =
            ((uint32_t)stream->data[4] << 24) |
            ((uint32_t)stream->data[5] << 16) |
            ((uint32_t)stream->data[6] << 8) |
            (uint32_t)stream->data[7];

        if (message_size > MAX_EVENT_SIZE) {
            stream_consume(stream, 8);
            stream->discard = message_size;
            continue;
        }

        if (stream->size < 8 + message_size)
            return;

        const char *stream_name = NULL;
        if (stream->data[0] == 1)
            stream_name = "stdout";
        else if (stream->data[0] == 2)
            stream_name = "stderr";

        if (stream_name)
            stream_emit(stream, stream->data + 8, message_size, stream_name);

        stream_consume(stream, 8 + message_size);
    }
}

static size_t log_write_callback(char *data, size_t size, size_t count,
                                 void *userdata)
{
    log_stream_t *stream = userdata;
    size_t length = size * count;

    if (stream->tty)
        stream_feed_tty(stream, (unsigned char *)data, length);
    else
        stream_feed_multiplexed(stream, (unsigned char *)data, length);

    return length;
}

static void *log_reader_worker(void *arg)
{
    log_reader_args_t *args = arg;
    log_stream_t stream = {
        .tty = docker_container_is_tty(args->container_id),
        .queue = args->monitor->queue,
        .profile = args->profile
    };
    CURL *curl = curl_easy_init();

    if (curl) {
        char url[LOGS_URL_SIZE];
        snprintf(
            url,
            sizeof(url),
            "http://localhost/containers/%s/logs?stdout=1&stderr=1&follow=1&timestamps=0&tail=0",
            args->container_id
        );

        curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, DOCKER_SOCK);
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, log_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &stream);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    if (stream.tty && stream.size > 0)
        stream_emit(&stream, stream.data, stream.size, "stdout");

    free(stream.data);
    profile_destroy(args->profile);
    monitor_remove(args->monitor, args->container_id);
    free(args->container_id);
    free(args);
    return NULL;
}

static void monitor_start_reader(docker_monitor_t *monitor,
                                 container_profile_t *profile)
{
    if (monitor_contains(monitor, profile->container_id)) {
        profile_destroy(profile);
        return;
    }

    monitored_container_t *container = calloc(1, sizeof(*container));
    log_reader_args_t *args = calloc(1, sizeof(*args));
    if (!container || !args) {
        free(container);
        free(args);
        profile_destroy(profile);
        return;
    }

    container->id = strdup(profile->container_id);
    args->container_id = strdup(profile->container_id);
    args->monitor = monitor;
    args->profile = profile;
    if (!container->id || !args->container_id) {
        free(container->id);
        free(container);
        free(args->container_id);
        free(args);
        profile_destroy(profile);
        return;
    }

    pthread_mutex_lock(&monitor->lock);
    container->next = monitor->containers;
    monitor->containers = container;
    pthread_mutex_unlock(&monitor->lock);

    pthread_t thread;
    if (pthread_create(&thread, NULL, log_reader_worker, args) != 0) {
        monitor_remove(monitor, container->id);
        profile_destroy(profile);
        free(args->container_id);
        free(args);
        return;
    }
    pthread_detach(thread);
}

static void monitor_scan(docker_monitor_t *monitor)
{
    char *docker_info = get_docker_info();
    if (!docker_info)
        return;

    cJSON *root = cJSON_Parse(docker_info);
    free(docker_info);
    if (!root || !cJSON_IsArray(root)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *container;
    cJSON_ArrayForEach(container, root) {
        cJSON *labels = cJSON_GetObjectItem(container, "Labels");
        cJSON *enabled = cJSON_GetObjectItem(labels, "ctfwatch.logs");
        cJSON *id = cJSON_GetObjectItem(container, "Id");

        if (!cJSON_IsString(enabled) || strcmp(enabled->valuestring, "true") != 0)
            continue;
        if (!cJSON_IsString(id) || monitor_contains(monitor, id->valuestring))
            continue;

        container_profile_t *profile = profile_build_from_container(container);
        if (profile)
            monitor_start_reader(monitor, profile);
    }

    cJSON_Delete(root);
}

static void *monitor_worker(void *arg)
{
    docker_monitor_t *monitor = arg;

    for (;;) {
        monitor_scan(monitor);
        sleep(MONITOR_INTERVAL_SEC);
    }

    return NULL;
}

int docker_monitor_start(event_queue_t *queue, pthread_t *thread)
{
    docker_monitor_t *monitor = calloc(1, sizeof(*monitor));
    if (!monitor)
        return -1;

    monitor->queue = queue;
    if (pthread_mutex_init(&monitor->lock, NULL) != 0) {
        free(monitor);
        return -1;
    }

    if (pthread_create(thread, NULL, monitor_worker, monitor) != 0) {
        pthread_mutex_destroy(&monitor->lock);
        free(monitor);
        return -1;
    }

    pthread_detach(*thread);
    return 0;
}