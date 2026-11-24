#include "json.h"

#include <assert.h>
#include <stdlib.h>

static _Thread_local JsonSerializerError error;

int32_t read_node(JsonContext *context, JsonReader *reader, JsonNode *node);
int32_t read_object(JsonContext *context, JsonReader *reader, JsonNode *node);
int32_t read_array(JsonContext *context, JsonReader *reader, JsonNode *node);
int32_t read_string(JsonContext *context, JsonReader *reader, JsonNode *node);
int32_t read_number(JsonContext *context, JsonReader *reader, JsonNode *node);
void set_error(const JsonReader *reader, JsonErrorCode code, const char *message);
void reset_error(void);

JsonNode * json_deserialize(JsonContext *context, JsonStream *stream) {
    (void)context;

    reset_error();

    JsonReader reader;
    if (json_reader_init(&reader, stream) != JSON_RESULT_OK) {
        return NULL;
    }

    if (!json_reader_move_next(&reader)) {
        set_error(&reader, JSON_ERROR_CODE_UNEXPECTED_END_OF_INPUT, "Input contains no valid JSON token");
        return NULL;
    }

    JsonNode *node = malloc(sizeof(JsonNode));
    if (!node) {
        set_error(&reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
        return NULL;
    }

    node->root = node;
    node->parent = NULL;

    if (read_node(context, &reader, node) != JSON_RESULT_OK) {
        json_node_dispose(node);
        return NULL;
    }

    return node;
}

int32_t json_serialize(JsonContext *context, JsonNode *node, JsonStream *stream) {
    (void)context;
    (void)node;
    (void)stream;

    return JSON_RESULT_ERROR;
}

JsonSerializerError json_error(void) {
    return error;
}

bool reader_move_next_expect(JsonReader *reader, const JsonTokenKind expected) {
    return json_reader_move_next(reader) && reader->token.kind == expected;
}

int32_t read_node(JsonContext *context, JsonReader *reader, JsonNode *node) {
    switch (reader->token.kind) {
        case JSON_TOKEN_KIND_BEGIN_ARRAY:
            if (read_array(context, reader, node) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
            break;
        case JSON_TOKEN_KIND_BEGIN_OBJECT:
            if (read_object(context, reader, node) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
            break;
        case JSON_TOKEN_KIND_STRING:
            if (read_string(context, reader, node) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
            break;
        case JSON_TOKEN_KIND_NUMBER:
            if (read_number(context, reader, node) != JSON_RESULT_OK) {
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
            set_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Unknown token kind");
            return JSON_RESULT_ERROR;
    }

    return JSON_RESULT_OK;
}

int32_t read_member(JsonContext *context, JsonReader *reader, JsonMember *member) {
    assert(reader->token.kind == JSON_TOKEN_KIND_STRING);

    member->name = reader->token.value.string.value;

    if (!reader_move_next_expect(reader, JSON_TOKEN_KIND_NAME_SEPARATOR)) {
        set_error(reader, JSON_ERROR_CODE_EXPECTED_NAME_SEPARATOR, "Expected ':'");
        return JSON_RESULT_ERROR;
    }

    if (!json_reader_move_next(reader)) {
        set_error(reader, JSON_ERROR_CODE_EXPECTED_VALUE, "Expected a JSON value");
        return JSON_RESULT_ERROR;
    }

    member->value = malloc(sizeof(JsonNode));

    if (read_node(context, reader, member->value) != JSON_RESULT_OK) {
        free(member->value);
        member->value = NULL;
        return JSON_RESULT_ERROR;
    }

    return JSON_RESULT_OK;
}

int32_t read_object(JsonContext *context, JsonReader *reader, JsonNode *node) {
    assert(reader->token.kind == JSON_TOKEN_KIND_BEGIN_OBJECT);

    if (!json_reader_move_next(reader)) {
        if (reader->can_read) {
            set_error(reader, JSON_ERROR_CODE_EXPECTED_END_OBJECT, "Unclosed object, expected '}'");
        } else {
            set_error(reader, JSON_ERROR_CODE_UNEXPECTED_END_OF_INPUT, "Input ended before parsing completed");
        }
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
        set_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
        return JSON_RESULT_ERROR;
    }

    do {
        if (reader->token.kind != JSON_TOKEN_KIND_STRING
            && !reader_move_next_expect(reader, JSON_TOKEN_KIND_STRING)) {
            set_error(reader, JSON_ERROR_CODE_EXPECTED_PROPERTY_NAME, "Expected a quoted object key");
            goto fail_cleanup_members;
        }

        if (i + 1 >= allocated) {
            allocated += allocated;
            JsonMember *new_members = realloc(members, sizeof(JsonMember) * allocated);
            if (!new_members) {
                set_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
                goto fail_cleanup_members;
            }
            members = new_members;
        }

        if (read_member(context, reader, &members[i]) != JSON_RESULT_OK) {
            goto fail_cleanup_members;
        }

        members[i].value->root = node->root;
        members[i].value->parent = node;

        i++;
    } while (reader_move_next_expect(reader, JSON_TOKEN_KIND_VALUE_SEPARATOR));

    if (reader->token.kind != JSON_TOKEN_KIND_END_OBJECT) {
        set_error(reader, JSON_ERROR_CODE_EXPECTED_END_OBJECT, "Unclosed object, expected '}'");
        goto fail_cleanup_members;
    }

    actual_members = realloc(members, sizeof(JsonMember) * i);
    if (!actual_members) {
        set_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
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

int32_t read_array(JsonContext *context, JsonReader *reader, JsonNode *node) {
    assert(reader->token.kind == JSON_TOKEN_KIND_BEGIN_ARRAY);

    if (!json_reader_move_next(reader)) {
        if (reader->can_read) {
            set_error(reader, JSON_ERROR_CODE_EXPECTED_END_ARRAY, "Unclosed array, expected ']'");
        } else {
            set_error(reader, JSON_ERROR_CODE_UNEXPECTED_END_OF_INPUT, "Input ended before parsing completed");
        }
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
        set_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
        return JSON_RESULT_ERROR;
    }

    do {
        if (reader->token.kind == JSON_TOKEN_KIND_VALUE_SEPARATOR
            && !json_reader_move_next(reader)) {
            set_error(reader, JSON_ERROR_CODE_EXPECTED_VALUE, "Expected JSON value");
            goto fail_cleanup_items;
        }

        if (i + 1 >= allocated) {
            allocated += allocated;
            JsonNode *new_items = realloc(items, sizeof(JsonNode) * allocated);
            if (!new_items) {
                set_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "out of memory");
                goto fail_cleanup_items;
            }
            items = new_items;
        }

        if (read_node(context, reader, &items[i]) != JSON_RESULT_OK) {
            goto fail_cleanup_items;
        }

        items[i].root = node->root;
        items[i].parent = node;

        i++;
    } while (reader_move_next_expect(reader, JSON_TOKEN_KIND_VALUE_SEPARATOR));

    if (reader->token.kind != JSON_TOKEN_KIND_END_ARRAY) {
        set_error(reader, JSON_ERROR_CODE_EXPECTED_END_ARRAY, "Unclosed array, expected ']'");
        goto fail_cleanup_items;
    }

    actual_items = realloc(items, sizeof(JsonNode) * i);
    if (!actual_items) {
        set_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
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

int32_t read_string(JsonContext *context, JsonReader *reader, JsonNode *node) {
    assert(reader->token.kind == JSON_TOKEN_KIND_STRING);

    (void)context;

    node->kind = JSON_NODE_KIND_VALUE;
    node->value = (JsonValue) {
        .kind = JSON_VALUE_KIND_STRING,
        .string = reader->token.value.string
    };

    return JSON_RESULT_OK;
}

int32_t read_number(JsonContext *context, JsonReader *reader, JsonNode *node) {
    assert(reader->token.kind == JSON_TOKEN_KIND_NUMBER);

    (void)context;

    node->kind = JSON_NODE_KIND_VALUE;
    node->value = (JsonValue) {
        .kind = JSON_VALUE_KIND_NUMBER,
        .number = reader->token.value.number
    };

    return JSON_RESULT_OK;
}

void set_error(const JsonReader *reader, const JsonErrorCode code, const char *message) {
    error.code = code;
    error.code_display_name = JSON_ERROR_CODE_NAMES[code];
    error.message = message;
    error.line = reader->current_line;
    error.column = reader->current_column;
    error.current_char = json_reader_current_char(reader);
}

void reset_error(void) {
    error.code = JSON_ERROR_CODE_NONE;
    error.code_display_name = NULL;
    error.message = NULL;
    error.line = 0;
    error.column = 0;
    error.current_char = '\0';
}
