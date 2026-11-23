#include "json.h"

#include <stdlib.h>

#include "json_reader.h"

int32_t read_node(JsonReader *reader, JsonNode *node);
int32_t read_object(JsonReader *reader, JsonNode *node);
int32_t read_array(JsonReader *reader, JsonNode *node);
int32_t read_string(JsonReader *reader, JsonNode *node);
int32_t read_number(JsonReader *reader, JsonNode *node);

JsonNode * json_deserialize(JsonStream *stream, JsonSerializerOptions *options) {
    (void)options;

    JsonReader reader;
    if (json_reader_init(&reader, stream) != JSON_RESULT_OK) {
        return NULL;
    }

    if (!json_reader_move_next(&reader)) {
        return NULL;
    }

    JsonNode *node = malloc(sizeof(JsonNode));
    if (!node) {
        return NULL;
    }

    node->root = node;
    node->parent = NULL;

    if (read_node(&reader, node) != JSON_RESULT_OK) {
        json_node_dispose(node);
        return NULL;
    }

    return node;
}

int32_t json_serialize(JsonNode *node, JsonStream *stream, JsonSerializerOptions *options) {
    (void)node;
    (void)stream;
    (void)options;

    return JSON_RESULT_ERROR;
}

bool reader_move_next_expect(JsonReader *reader, const JsonTokenKind expected) {
    return json_reader_move_next(reader) && reader->token.kind == expected;
}

int32_t read_node(JsonReader *reader, JsonNode *node) {
    switch (reader->token.kind) {
        case JSON_TOKEN_KIND_BEGIN_ARRAY:
            if (read_array(reader, node) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
            break;
        case JSON_TOKEN_KIND_BEGIN_OBJECT:
            if (read_object(reader, node) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
            break;
        case JSON_TOKEN_KIND_STRING:
            if (read_string(reader, node) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
            break;
        case JSON_TOKEN_KIND_NUMBER:
            if (read_number(reader, node) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
            break;
        case JSON_TOKEN_KIND_FALSE:
            node->kind = JSON_NODE_KIND_VALUE;
            node->value = (JsonValue) {
                .kind = JSON_VALUE_KIND_TRUE,
                .boolean = (JsonBoolean) {
                    .value = false
                }
            };
            break;
        case JSON_TOKEN_KIND_TRUE:
            node->kind = JSON_NODE_KIND_VALUE;
            node->value = (JsonValue) {
                .kind = JSON_VALUE_KIND_TRUE,
                .boolean = (JsonBoolean) {
                    .value = true
                }
            };
            break;
        case JSON_TOKEN_KIND_NULL:
            node->kind = JSON_NODE_KIND_VALUE;
            node->value = (JsonValue) {
                .kind = JSON_VALUE_KIND_NULL,
                .null = {0}
            };
            break;
        default:
            return JSON_RESULT_ERROR;
    }

    return JSON_RESULT_OK;
}

int32_t read_member(JsonReader *reader, JsonMember *member) {
    if (reader->token.kind != JSON_TOKEN_KIND_STRING) {
        return JSON_RESULT_ERROR;
    }

    member->name = reader->token.value.string.value;

    if (!reader_move_next_expect(reader, JSON_TOKEN_KIND_NAME_SEPARATOR)) {
        return JSON_RESULT_ERROR;
    }

    if (!json_reader_move_next(reader)) {
        return JSON_RESULT_ERROR;
    }

    member->value = malloc(sizeof(JsonNode));

    if (read_node(reader, member->value) != JSON_RESULT_OK) {
        free(member->value);
        member->value = NULL;
        return JSON_RESULT_ERROR;
    }

    return JSON_RESULT_OK;
}

int32_t read_object(JsonReader *reader, JsonNode *node) {
    if (reader->token.kind != JSON_TOKEN_KIND_BEGIN_OBJECT) {
        return JSON_RESULT_ERROR;
    }

    if (!json_reader_move_next(reader)) {
        return JSON_RESULT_ERROR;
    }

    uint32_t i = 0;
    JsonMember *actual_members = NULL;

    if (reader->token.kind == JSON_TOKEN_KIND_END_OBJECT) {
        goto success;
    }

    uint32_t allocated = 10;
    JsonMember *members = malloc(sizeof(JsonMember) * allocated);

    if (!members) {
        return JSON_RESULT_ERROR;
    }

    do {
        if (reader->token.kind != JSON_TOKEN_KIND_STRING
            && !reader_move_next_expect(reader, JSON_TOKEN_KIND_STRING)) {
            goto fail_cleanup_members;
        }

        if (i + 1 >= allocated) {
            allocated += allocated;
            JsonMember *new_members = realloc(members, sizeof(JsonMember) * allocated);
            if (!new_members) {
                goto fail_cleanup_members;
            }
            members = new_members;
        }

        if (read_member(reader, &members[i]) != JSON_RESULT_OK) {
            goto fail_cleanup_members;
        }

        members[i].value->root = node->root;
        members[i].value->parent = node;

        i++;
    } while (reader_move_next_expect(reader, JSON_TOKEN_KIND_VALUE_SEPARATOR));

    if (reader->token.kind != JSON_TOKEN_KIND_END_OBJECT) {
        goto fail_cleanup_members;
    }

    actual_members = realloc(members, sizeof(JsonMember) * i);
    if (!actual_members) {
        goto fail_cleanup_members;
    }

success:
    node->kind = JSON_NODE_KIND_OBJECT;
    node->object.member_count = i;
    node->object.members = actual_members;
    return JSON_RESULT_OK;

fail_cleanup_members:
    free(members);
    return JSON_RESULT_ERROR;
}

int32_t read_array(JsonReader *reader, JsonNode *node) {
    if (reader->token.kind != JSON_TOKEN_KIND_BEGIN_ARRAY) {
        return JSON_RESULT_ERROR;
    }

    if (!json_reader_move_next(reader)) {
        return JSON_RESULT_ERROR;
    }

    uint32_t i = 0;
    JsonNode *actual_items = NULL;

    if (reader->token.kind == JSON_TOKEN_KIND_END_ARRAY) {
        goto success;
    }

    uint32_t allocated = 10;
    JsonNode *items = malloc(sizeof(JsonNode) * allocated);

    if (!items) {
        return JSON_RESULT_ERROR;
    }

    do {
        if (reader->token.kind == JSON_TOKEN_KIND_VALUE_SEPARATOR
            && !json_reader_move_next(reader)) {
            goto fail_cleanup_items;
        }

        if (i + 1 >= allocated) {
            allocated += allocated;
            JsonNode *new_items = realloc(items, sizeof(JsonNode) * allocated);
            if (!new_items) {
                goto fail_cleanup_items;
            }
            items = new_items;
        }

        if (read_node(reader, &items[i]) != JSON_RESULT_OK) {
            goto fail_cleanup_items;
        }

        items[i].root = node->root;
        items[i].parent = node;

        i++;
    } while (reader_move_next_expect(reader, JSON_TOKEN_KIND_VALUE_SEPARATOR));

    if (reader->token.kind != JSON_TOKEN_KIND_END_ARRAY) {
        goto fail_cleanup_items;
    }

    actual_items = realloc(items, sizeof(JsonNode) * i);
    if (!actual_items) {
        goto fail_cleanup_items;
    }

success:
    node->kind = JSON_NODE_KIND_ARRAY;
    node->array.item_count = i;
    node->array.items = actual_items;
    return JSON_RESULT_OK;

fail_cleanup_items:
    free(items);
    return JSON_RESULT_ERROR;
}

int32_t read_string(JsonReader *reader, JsonNode *node) {
    if (reader->token.kind != JSON_TOKEN_KIND_STRING) {
        return JSON_RESULT_ERROR;
    }

    node->kind = JSON_NODE_KIND_VALUE;
    node->value = (JsonValue) {
        .kind = JSON_VALUE_KIND_STRING,
        .string = reader->token.value.string
    };

    return JSON_RESULT_OK;
}

int32_t read_number(JsonReader *reader, JsonNode *node) {
    if (reader->token.kind != JSON_TOKEN_KIND_NUMBER) {
        return JSON_RESULT_ERROR;
    }

    node->kind = JSON_NODE_KIND_VALUE;
    node->value = (JsonValue) {
        .kind = JSON_VALUE_KIND_NUMBER,
        .number = reader->token.value.number
    };

    return JSON_RESULT_OK;
}



