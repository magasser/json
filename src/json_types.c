#include "json_types.h"

#include <stdlib.h>
#include <string.h>

JsonNode * json_node_get_item(JsonNode *node, const char *name) {
    if (node->kind != JSON_NODE_KIND_OBJECT) {
        return NULL;
    }

    for (uint32_t i = 0; i < node->object.member_count; i++) {
        JsonMember member = node->object.members[i];
        if (strcmp(member.name, name) == 0) {
            return member.value;
        }
    }

    return NULL;
}

JsonNode * json_node_get_item_by_path(JsonNode *node, JsonPath path) {
    if (path.length <= 0 || node->kind != JSON_NODE_KIND_OBJECT) {
        return NULL;
    }

    JsonNode *child = json_node_get_item(node, path.value[0]);

    if (path.length == 1) {
        return child;
    }

    const JsonPath new_path = (JsonPath) {
        .length = path.length - 1,
        .value = &path.value[1]
    };

    return json_node_get_item_by_path(child, new_path);
}

void dispose_object(JsonObject object) {
    for (uint32_t i = 0; i < object.member_count; i++) {
        json_node_dispose(object.members[i].value);
        free((char *)object.members[i].name);
    }

    free(object.members);
}

void dispose_array(JsonArray array) {
    for (uint32_t i = 0; i < array.item_count; i++) {
        json_node_dispose(&array.items[i]);
    }

    free(array.items);
}

void dispose_value(JsonValue value) {
    switch (value.kind) {
        case JSON_VALUE_KIND_OBJECT:
            dispose_object(value.object);
            break;
        case JSON_VALUE_KIND_ARRAY:
            dispose_array(value.array);
            break;
        case JSON_VALUE_KIND_STRING:
            free((char *)value.string.value);
            break;
        default:
            break;
    }
}

void json_node_dispose(JsonNode *node) {
    switch (node->kind) {
        case JSON_NODE_KIND_OBJECT:
            dispose_object(node->object);
            break;
        case JSON_NODE_KIND_ARRAY:
            dispose_array(node->array);
            break;
        case JSON_NODE_KIND_VALUE:
            dispose_value(node->value);
            break;
        default:
            break;
    }

    free(node);
}
