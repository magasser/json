#include "json.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define JSON_BEGIN_OBJECT       '{'
#define JSON_END_OBJECT         '}'
#define JSON_BEGIN_ARRAY        '['
#define JSON_END_ARRAY          ']'
#define JSON_NAME_SEPARATOR     ':'
#define JSON_VALUE_SEPARATOR    ','

#define JSON_LITERAL_FALSE      "false"
#define JSON_LITERAL_TRUE       "true"
#define JSON_LITERAL_NULL        "null"

#define QUOTATION_MARK          '"'
#define ESCAPE                  '\\'
#define REVERSE_SOLIDUS         '\\'
#define SOLIDUS                 '/'
#define BACKSPACE               '\b'
#define FORM_FEED               '\f'
#define LINE_FEED               '\n'
#define CARRIAGE_RETURN         '\r'
#define TAB                     '\t'
#define U_XXXX                  'u'

static _Thread_local JsonSerializerError error;

typedef struct {
    JsonSerializerOptions *options;
    uint32_t depth;
} JsonContext;

const JsonSerializerOptions DEFAULT_SERIALIZER_OPTIONS = {
    .max_depth = 64,
    .use_pretty_printing = false,
#ifdef _WIN32
    .new_line = "\r\n",
#else
    .new_line = "\n",
#endif
    .indent = "  "
};

int32_t read_node(JsonContext *context, JsonReader *reader, JsonNode *node);
int32_t read_object(JsonContext *context, JsonReader *reader, JsonNode *node);
int32_t read_array(JsonContext *context, JsonReader *reader, JsonNode *node);
int32_t read_string(JsonContext *context, JsonReader *reader, JsonNode *node);
int32_t read_number(JsonContext *context, JsonReader *reader, JsonNode *node);
int32_t write_node(JsonContext *context, JsonWriter *writer, JsonNode *node);
int32_t write_object(JsonContext *context, JsonWriter *writer, JsonNode *node);
int32_t write_array(JsonContext *context, JsonWriter *writer, JsonNode *node);
int32_t write_string(JsonContext *context, JsonWriter *writer, JsonNode *node);
int32_t write_number(JsonContext *context, JsonWriter *writer, JsonNode *node);
void set_reader_error(const JsonReader *reader, JsonErrorCode code, const char *message);
void set_writer_error(const JsonWriter *writer, JsonErrorCode code, const char *message);
void reset_error(void);

void context_init(JsonContext *context, JsonSerializerOptions *options) {
    if (!options) {
        options = (JsonSerializerOptions *)&DEFAULT_SERIALIZER_OPTIONS;
    }

    context->options = options;
    context->depth = 0;

    reset_error();
}

void reader_init(JsonReader *reader, JsonStream *stream) {
    reader->token = (JsonToken){0};
    reader->stream = stream;
    reader->global_position = 0;
    reader->current_position = 0;
    reader->buffer_end = 0;
    reader->current_line = 1;
    reader->current_column = 1;
    reader->can_read = true;
}

void writer_init(JsonWriter *writer, JsonStream *stream) {
    writer->stream = stream;
    writer->global_position = 0;
    writer->current_position = 0;
    writer->current_node = NULL;
    writer->can_write = true;
}

JsonNode * json_deserialize(JsonStream *stream, JsonSerializerOptions *options) {
    if (!stream) {
        set_reader_error(NULL, JSON_ERROR_CODE_INVALID_ARGUMENT, "The 'stream' cannot be null");
        return NULL;
    }

    JsonContext context;
    context_init(&context, options);

    JsonReader reader;
    reader_init(&reader, stream);

    if (!json_reader_move_next(&reader)) {
        set_reader_error(&reader, JSON_ERROR_CODE_UNEXPECTED_END_OF_INPUT, "Input contains no valid JSON token");
        return NULL;
    }

    JsonNode *node = malloc(sizeof(JsonNode));
    if (!node) {
        set_reader_error(&reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
        return NULL;
    }

    node->root = node;
    node->parent = NULL;

    if (read_node(&context, &reader, node) != JSON_RESULT_OK) {
        json_node_dispose(node);
        return NULL;
    }

    return node;
}

int32_t json_serialize(JsonNode *node, JsonStream *stream, JsonSerializerOptions *options) {
    if (!node) {
        set_writer_error(NULL, JSON_ERROR_CODE_INVALID_VALUE, "The 'node' cannot be null");
        return JSON_RESULT_ERROR;
    }

    JsonContext context;
    context_init(&context, options);

    JsonWriter writer;
    writer_init(&writer, stream);

    if (write_node(&context, &writer, node) != JSON_RESULT_OK) {
        return JSON_RESULT_ERROR;
    }

    return JSON_RESULT_OK;
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
            set_reader_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Unknown token kind");
            return JSON_RESULT_ERROR;
    }

    return JSON_RESULT_OK;
}

int32_t read_member(JsonContext *context, JsonReader *reader, JsonMember *member) {
    assert(reader->token.kind == JSON_TOKEN_KIND_STRING);

    member->name = reader->token.value.string.value;

    if (!reader_move_next_expect(reader, JSON_TOKEN_KIND_NAME_SEPARATOR)) {
        set_reader_error(reader, JSON_ERROR_CODE_EXPECTED_NAME_SEPARATOR, "Expected ':'");
        return JSON_RESULT_ERROR;
    }

    if (!json_reader_move_next(reader)) {
        set_reader_error(reader, JSON_ERROR_CODE_EXPECTED_VALUE, "Expected a JSON value");
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
            set_reader_error(reader, JSON_ERROR_CODE_EXPECTED_END_OBJECT, "Unclosed object, expected '}'");
        } else {
            set_reader_error(reader, JSON_ERROR_CODE_UNEXPECTED_END_OF_INPUT, "Input ended before parsing completed");
        }
        return JSON_RESULT_ERROR;
    }

    context->depth++;

    if (context->depth > context->options->max_depth) {
        set_reader_error(reader, JSON_ERROR_CODE_MAX_DEPTH_EXCEEDED, "Maximum nesting depth exceeded");
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
        set_reader_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
        return JSON_RESULT_ERROR;
    }

    do {
        if (reader->token.kind != JSON_TOKEN_KIND_STRING
            && !reader_move_next_expect(reader, JSON_TOKEN_KIND_STRING)) {
            set_reader_error(reader, JSON_ERROR_CODE_EXPECTED_PROPERTY_NAME, "Expected a quoted object key");
            goto fail_cleanup_members;
        }

        if (i + 1 >= allocated) {
            allocated += allocated;
            JsonMember *new_members = realloc(members, sizeof(JsonMember) * allocated);
            if (!new_members) {
                set_reader_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
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
        set_reader_error(reader, JSON_ERROR_CODE_EXPECTED_END_OBJECT, "Unclosed object, expected '}'");
        goto fail_cleanup_members;
    }

    actual_members = realloc(members, sizeof(JsonMember) * i);
    if (!actual_members) {
        set_reader_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
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
            set_reader_error(reader, JSON_ERROR_CODE_EXPECTED_END_ARRAY, "Unclosed array, expected ']'");
        } else {
            set_reader_error(reader, JSON_ERROR_CODE_UNEXPECTED_END_OF_INPUT, "Input ended before parsing completed");
        }
        return JSON_RESULT_ERROR;
    }

    context->depth++;

    if (context->depth > context->options->max_depth) {
        set_reader_error(reader, JSON_ERROR_CODE_MAX_DEPTH_EXCEEDED, "Maximum nesting depth exceeded");
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
        set_reader_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
        return JSON_RESULT_ERROR;
    }

    do {
        if (reader->token.kind == JSON_TOKEN_KIND_VALUE_SEPARATOR
            && !json_reader_move_next(reader)) {
            set_reader_error(reader, JSON_ERROR_CODE_EXPECTED_VALUE, "Expected JSON value");
            goto fail_cleanup_items;
        }

        if (i + 1 >= allocated) {
            allocated += allocated;
            JsonNode *new_items = realloc(items, sizeof(JsonNode) * allocated);
            if (!new_items) {
                set_reader_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "out of memory");
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
        set_reader_error(reader, JSON_ERROR_CODE_EXPECTED_END_ARRAY, "Unclosed array, expected ']'");
        goto fail_cleanup_items;
    }

    actual_items = realloc(items, sizeof(JsonNode) * i);
    if (!actual_items) {
        set_reader_error(reader, JSON_ERROR_CODE_INTERNAL_ERROR, "Out of memory");
        goto fail_cleanup_items;
    }

success:
    context->depth--;
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

int32_t writer_write(JsonWriter *writer, const char *value, const uint32_t length) {
    const size_t written = writer->stream->write(writer->stream, (const uint8_t*)value, length);

    if (written != length) {
        set_writer_error(writer, JSON_ERROR_CODE_INTERNAL_ERROR, "Writing to stream failed");
        return JSON_RESULT_ERROR;
    }

    writer->global_position += length;
    writer->current_position += length;

    return JSON_RESULT_OK;
}

int32_t writer_write_char(JsonWriter *writer, const char value) {
    const size_t written = writer->stream->write(writer->stream, (const uint8_t*)&value, 1);

    if (written != 1) {
        set_writer_error(writer, JSON_ERROR_CODE_INTERNAL_ERROR, "Writing to stream failed");
        return JSON_RESULT_ERROR;
    }

    writer->global_position += 1;
    writer->current_position += 1;

    return JSON_RESULT_OK;
}

int32_t write_node(JsonContext *context, JsonWriter *writer, JsonNode *node) {
    (void)context;
    (void)writer;
    (void)node;

    if (!node) {
        writer_write(writer, "null", 4);
        return JSON_RESULT_OK;
    }

    switch (node->kind) {
        case JSON_NODE_KIND_OBJECT:
            if (write_object(context, writer, node) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
            break;
        case JSON_NODE_KIND_ARRAY:
            if (write_array(context, writer, node) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
            break;
        case JSON_NODE_KIND_VALUE:
            switch (node->value.kind) {
                case JSON_VALUE_KIND_OBJECT:
                    if (write_object(context, writer, node) != JSON_RESULT_OK) {
                        return JSON_RESULT_ERROR;
                    }
                    break;
                case JSON_VALUE_KIND_ARRAY:
                    if (write_array(context, writer, node) != JSON_RESULT_OK) {
                        return JSON_RESULT_ERROR;
                    }
                    break;
                case JSON_VALUE_KIND_STRING:
                    if (write_string(context, writer, node) != JSON_RESULT_OK) {
                        return JSON_RESULT_ERROR;
                    }
                    break;
                case JSON_VALUE_KIND_NUMBER:
                    if (write_number(context, writer, node) != JSON_RESULT_OK) {
                        return JSON_RESULT_ERROR;
                    }
                    break;
                case JSON_VALUE_KIND_TRUE:
                    if (writer_write(writer, "true", 4) != JSON_RESULT_OK) {
                        return JSON_RESULT_ERROR;
                    }
                    break;
                case JSON_VALUE_KIND_FALSE:
                    if (writer_write(writer, "false", 5) != JSON_RESULT_OK) {
                        return JSON_RESULT_ERROR;
                    }
                    break;
                case JSON_VALUE_KIND_NULL:
                    if (writer_write(writer, "null", 4) != JSON_RESULT_OK) {
                        return JSON_RESULT_ERROR;
                    }
                    break;
                default:
                    set_writer_error(writer, JSON_ERROR_CODE_INVALID_ARGUMENT, "Unknown value kind");
                    return JSON_RESULT_ERROR;
            }
            break;
        default:
            set_writer_error(writer, JSON_ERROR_CODE_INVALID_ARGUMENT, "Unknown node kind");
            return JSON_RESULT_ERROR;
    }

    return JSON_RESULT_OK;
}

bool must_escape(char c) {
    return c == QUOTATION_MARK
            || c == REVERSE_SOLIDUS
            || c == SOLIDUS
            || c == BACKSPACE
            || c == FORM_FEED
            || c == LINE_FEED
            || c == CARRIAGE_RETURN
            || c == TAB;
}

const char * escape(const char *c, uint8_t *size) {
    switch (*c) {
        case QUOTATION_MARK:
            *size = 2;
            return "\\\"";
        case REVERSE_SOLIDUS:
            *size = 2;
            return "\\\\";
        case SOLIDUS:
            *size = 2;
            return "\\/";
        case BACKSPACE:
            *size = 2;
            return "\\b";
        case FORM_FEED:
            *size = 2;
            return "\\f";
        case LINE_FEED:
            *size = 2;
            return "\\n";
        case CARRIAGE_RETURN:
            *size = 2;
            return "\\r";
        case TAB:
            *size = 2;
            return "\\t";
        default:
            *size = 1;
            return c;
    }
}

int32_t write_string_value(JsonWriter *writer, const char *value) {
    const size_t value_length = strlen(value);
    size_t length = value_length + 2;

    for (size_t i = 0; i < value_length; i++) {
        if (must_escape(value[i])) {
            length++;
        }
    }

    char temp[length];

    temp[0] = QUOTATION_MARK;

    for (size_t i = 1, j = 0; i < length - 1; j++) {
        uint8_t size;
        const char *v = escape(&value[j], &size);

        for (uint8_t k = 0; k < size; k++) {
            temp[i++] = v[k];
        }
    }

    temp[length - 1] = QUOTATION_MARK;

    if (writer_write(writer, temp, length) != JSON_RESULT_OK) {
        return JSON_RESULT_ERROR;
    }

    return JSON_RESULT_OK;
}

int32_t writer_next_line(JsonContext *context, JsonWriter *writer) {
    if (!context->options->use_pretty_printing) {
        return JSON_RESULT_OK;
    }

    if (writer_write(writer, context->options->new_line, strlen(context->options->new_line) != JSON_RESULT_OK)) {
        return JSON_RESULT_ERROR;
    }

    for (uint32_t i = 0; i < context->depth; i++) {
        if (writer_write(writer, context->options->indent, strlen(context->options->indent) != JSON_RESULT_OK)) {
            return JSON_RESULT_ERROR;
        }
    }

    return JSON_RESULT_OK;
}

int32_t write_object(JsonContext *context, JsonWriter *writer, JsonNode *node) {
    if (writer_write_char(writer, JSON_BEGIN_OBJECT) != JSON_RESULT_OK) {
        return JSON_RESULT_ERROR;
    }

    if (node->object.member_count <= 0) {
        goto end_object;
    }

    context->depth++;

    if (context->depth > context->options->max_depth) {
        set_writer_error(writer, JSON_ERROR_CODE_MAX_DEPTH_EXCEEDED, "Maximum nesting depth exceeded");
        return JSON_RESULT_ERROR;
    }

    if (writer_next_line(context, writer) != JSON_RESULT_OK) {
        return JSON_RESULT_ERROR;
    }

    if (!node->object.members) {
        set_writer_error(writer, JSON_ERROR_CODE_INVALID_ARGUMENT, "Members cannot be null with non-zero member count");
        return JSON_RESULT_ERROR;
    }

    for (size_t i = 0; i < node->object.member_count; i++) {
        JsonMember *member = &node->object.members[i];

        if (!member) {
            set_writer_error(writer, JSON_ERROR_CODE_INVALID_ARGUMENT, "Member cannot be null");
            return JSON_RESULT_ERROR;
        }

        if (!member->name) {
            set_writer_error(writer, JSON_ERROR_CODE_INVALID_ARGUMENT, "Member name cannot be null");
            return JSON_RESULT_ERROR;
        }

        write_string_value(writer, member->name);

        if (writer_write_char(writer, JSON_NAME_SEPARATOR) != JSON_RESULT_OK) {
            return JSON_RESULT_ERROR;
        }

        if (context->options->use_pretty_printing) {
            if (writer_write_char(writer, ' ') != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
        }

        if (write_node(context, writer, member->value) != JSON_RESULT_OK) {
            return JSON_RESULT_ERROR;
        }

        if (i < node->object.member_count - 1) {
            if (writer_write_char(writer, JSON_VALUE_SEPARATOR) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }

            if (writer_next_line(context, writer) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
        }
    }

    context->depth--;

    if (writer_next_line(context, writer) != JSON_RESULT_OK) {
        return JSON_RESULT_ERROR;
    }

end_object:
    if (writer_write_char(writer, JSON_END_OBJECT) != JSON_RESULT_OK) {
        return JSON_RESULT_ERROR;
    }

    return JSON_RESULT_OK;
}

int32_t write_array(JsonContext *context, JsonWriter *writer, JsonNode *node) {
    if (writer_write_char(writer, JSON_BEGIN_ARRAY) != JSON_RESULT_OK) {
        return JSON_RESULT_ERROR;
    }

    if (node->array.item_count <= 0) {
        goto end_array;
    }

    context->depth++;

    if (context->depth > context->options->max_depth) {
        set_writer_error(writer, JSON_ERROR_CODE_MAX_DEPTH_EXCEEDED, "Maximum nesting depth exceeded");
        return JSON_RESULT_ERROR;
    }

    if (writer_next_line(context, writer) != JSON_RESULT_OK) {
        return JSON_RESULT_ERROR;
    }

    if (!node->array.items) {
        set_writer_error(writer, JSON_ERROR_CODE_INVALID_ARGUMENT, "Members cannot be null with non-zero member count");
        return JSON_RESULT_ERROR;
    }

    for (size_t i = 0; i < node->array.item_count; i++) {
        JsonNode *item = &node->array.items[i];

        if (!item) {
            set_writer_error(writer, JSON_ERROR_CODE_INVALID_ARGUMENT, "Member cannot be null");
            return JSON_RESULT_ERROR;
        }

        if (write_node(context, writer, item) != JSON_RESULT_OK) {
            return JSON_RESULT_ERROR;
        }

        if (i < node->array.item_count - 1) {
            if (writer_write_char(writer, JSON_VALUE_SEPARATOR) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }

            if (writer_next_line(context, writer) != JSON_RESULT_OK) {
                return JSON_RESULT_ERROR;
            }
        }
    }
    
    context->depth--;

    if (writer_next_line(context, writer) != JSON_RESULT_OK) {
        return JSON_RESULT_ERROR;
    }

end_array:
    if (writer_write_char(writer, JSON_END_ARRAY) != JSON_RESULT_OK) {
        return JSON_RESULT_ERROR;
    }

    return JSON_RESULT_OK;
}

int32_t write_string(JsonContext *context, JsonWriter *writer, JsonNode *node) {
    assert(node->value.kind == JSON_VALUE_KIND_STRING);

    (void)context;

    if (!node->value.string.value) {
        return writer_write(writer, "null", 4);
    }

    return write_string_value(writer, node->value.string.value);
}

int32_t write_number(JsonContext *context, JsonWriter *writer, JsonNode *node) {
    assert(node->value.kind == JSON_VALUE_KIND_NUMBER);

    (void)context;
    (void)writer;
    (void)node;

    switch (node->value.number.kind) {
        case JSON_NUMBER_KIND_INTEGER: {
            char temp[32];

            const size_t written = snprintf(temp, sizeof(temp), "%lld", node->value.number.value.i);

            if (written >= sizeof(temp)) {
                set_writer_error(writer,
                    JSON_ERROR_CODE_OUTPUT_LIMIT_EXCEEDED,
                    "Maximum number of integer digits (32) has been exceeded");
                return JSON_RESULT_ERROR;
            }

            return write_string_value(writer, temp);
        }
        case JSON_NUMBER_KIND_DOUBLE: {
            char temp[64];

            const size_t written = snprintf(temp, sizeof(temp), "%.17g", node->value.number.value.d);

            if (written >= sizeof(temp)) {
                set_writer_error(writer,
                    JSON_ERROR_CODE_OUTPUT_LIMIT_EXCEEDED,
                    "Maximum number of double digits (64) has been exceeded");
                return JSON_RESULT_ERROR;
            }

            return write_string_value(writer, temp);
        }
        default:
            set_writer_error(writer, JSON_ERROR_CODE_INVALID_ARGUMENT, "Unknown number kind");
            return JSON_RESULT_ERROR;
    }
}

void set_reader_error(const JsonReader *reader, const JsonErrorCode code, const char *message) {
    error.code = code;
    error.code_display_name = JSON_ERROR_CODE_NAMES[code];
    error.message = message;
    error.line = reader ? reader->current_line : 0;
    error.column = reader ? reader->current_column : 0;
}

void set_writer_error(const JsonWriter *writer, const JsonErrorCode code, const char *message) {
    error.code = code;
    error.code_display_name = JSON_ERROR_CODE_NAMES[code];
    error.message = message;
    error.node = writer ? writer->current_node : NULL;
}

void reset_error(void) {
    error.code = JSON_ERROR_CODE_NONE;
    error.code_display_name = NULL;
    error.message = NULL;
    error.line = 0;
    error.column = 0;
    error.node = NULL;
}
