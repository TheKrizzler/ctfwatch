#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "event.h"

char *event_enrich(
    const char *data,
    size_t size,
    const container_profile_t *profile,
    size_t *output_size
) {
    cJSON *root = cJSON_ParseWithLength(data, size);

    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return NULL;
    }

    /*
     * Never trust a logger-supplied ctfwatch namespace.
     */
    cJSON_DeleteItemFromObject(root, "ctfwatch");

    cJSON *ctfwatch = cJSON_AddObjectToObject(
        root,
        "ctfwatch"
    );

    if (!ctfwatch)
        goto fail;

    cJSON *container = cJSON_AddObjectToObject(
        ctfwatch,
        "container"
    );

    if (!container)
        goto fail;

    if (!cJSON_AddStringToObject(
            container,
            "id",
            profile->container_id
        ))
        goto fail;

    if (!cJSON_AddStringToObject(
            container,
            "name",
            profile->container_name
        ))
        goto fail;

    if (profile->image_name) {
        if (!cJSON_AddStringToObject(
                container,
                "image",
                profile->image_name
            ))
            goto fail;
    }

    if (profile->compose_project) {
        if (!cJSON_AddStringToObject(
                container,
                "compose_project",
                profile->compose_project
            ))
            goto fail;
    }

    if (profile->compose_service) {
        if (!cJSON_AddStringToObject(
                container,
                "compose_service",
                profile->compose_service
            ))
            goto fail;
    }

    char *serialized = cJSON_PrintUnformatted(root);

    if (!serialized)
        goto fail;

    *output_size = strlen(serialized);

    cJSON_Delete(root);
    return serialized;

fail:
    cJSON_Delete(root);
    return NULL;
}

void event_destroy(event_t *event)
{
    if (!event)
        return;

    free(event->data);
    free(event);
}