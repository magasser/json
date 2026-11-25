#ifndef JSON_LIBRARY_H
#define JSON_LIBRARY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define JSON_RESULT_OK 0
#include <stdio.h>

#ifdef JSON_DEBUG
    #define JSON_RESULT_ERROR (__debugbreak(), -1)
#else
    #define JSON_RESULT_ERROR (-1)
#endif

#define GENERATE_ENUM_VALUES(value, name) value,
#define GENERATE_ENUM_NAME_MAPPING(value, name) [value] = name, \

#define GENERATE_ENUM_WITH_NAME_MAPPINGS(type, mappings, X) \
    typedef enum { \
        X(GENERATE_ENUM_VALUES) \
    } type; \
    \
    static const char *const mappings[] = { \
        X(GENERATE_ENUM_NAME_MAPPING) \
    }

#define JSON_ERROR_CODES(X) \
    /* General errors */ \
    X(JSON_ERROR_CODE_NONE, "NONE") \
    X(JSON_ERROR_CODE_INTERNAL_ERROR, "INTERNAL_ERROR") \
    X(JSON_ERROR_CODE_INVALID_ARGUMENT, "INVALID_ARGUMENT") \
    \
    /* Deserialization errors */ \
    X(JSON_ERROR_CODE_UNEXPECTED_END_OF_INPUT, "UNEXPECTED_END_OF_INPUT") \
    X(JSON_ERROR_CODE_UNEXPECTED_CHARACTER, "UNEXPECTED_CHARACTER") \
    X(JSON_ERROR_CODE_EXPECTED_VALUE, "EXPECTED_VALUE") \
    X(JSON_ERROR_CODE_EXPECTED_PROPERTY_NAME, "EXPECTED_PROPERTY_NAME") \
    X(JSON_ERROR_CODE_EXPECTED_NAME_SEPARATOR, "EXPECTED_NAME_SEPARATOR") \
    X(JSON_ERROR_CODE_EXPECTED_VALUE_SEPARATOR, "EXPECTED_VALUE_SEPARATOR") \
    X(JSON_ERROR_CODE_EXPECTED_END_ARRAY, "EXPECTED_END_ARRAY") \
    X(JSON_ERROR_CODE_EXPECTED_END_OBJECT, "EXPECTED_END_OBJECT") \
    X(JSON_ERROR_CODE_INVALID_NUMBER, "INVALID_NUMBER") \
    X(JSON_ERROR_CODE_INVALID_STRING, "INVALID_STRING") \
    X(JSON_ERROR_CODE_INVALID_ESCAPE_SEQUENCE, "INVALID_ESCAPE_SEQUENCE") \
    X(JSON_ERROR_CODE_INVALID_UNICODE_ESCAPE, "INVALID_UNICODE_ESCAPE") \
    X(JSON_ERROR_CODE_TRAILING_CHARACTERS, "TRAILING_CHARACTERS") \
    X(JSON_ERROR_CODE_MAX_DEPTH_EXCEEDED, "MAX_DEPTH_EXCEEDED") \
    \
    /* Serialization errors */ \
    X(JSON_ERROR_CODE_INVALID_VALUE, "INVALID_VALUE") \
    X(JSON_ERROR_CODE_UNSUPPORTED_VALUE, "UNSUPPORTED_VALUE") \
    X(JSON_ERROR_CODE_NON_FINITE_NUMBER, "NON_FINITE_NUMBER") \
    X(JSON_ERROR_CODE_INVALID_STRING_ENCODING, "INVALID_STRING_ENCODING") \
    X(JSON_ERROR_CODE_CIRCULAR_REFERENCE, "CIRCULAR_REFERENCE") \
    X(JSON_ERROR_CODE_OUTPUT_LIMIT_EXCEEDED, "OUTPUT_LIMIT_EXCEEDED") \

#define JSON_TOKEN_KINDS(X) \
    X(JSON_TOKEN_KIND_NONE, "NONE") \
    X(JSON_TOKEN_KIND_BEGIN_ARRAY, "BEGIN_ARRAY") \
    X(JSON_TOKEN_KIND_BEGIN_OBJECT, "BEGIN_OBJECT") \
    X(JSON_TOKEN_KIND_END_ARRAY, "END_ARRAY") \
    X(JSON_TOKEN_KIND_END_OBJECT, "END_OBJECT") \
    X(JSON_TOKEN_KIND_NAME_SEPARATOR, "NAME_SEPARATOR") \
    X(JSON_TOKEN_KIND_VALUE_SEPARATOR, "VALUE_SEPARATOR") \
    X(JSON_TOKEN_KIND_NUMBER, "NUMBER") \
    X(JSON_TOKEN_KIND_STRING, "STRING") \
    X(JSON_TOKEN_KIND_FALSE, "FALSE") \
    X(JSON_TOKEN_KIND_TRUE, "TRUE") \
    X(JSON_TOKEN_KIND_NULL, "NULL")

#define JSON_NODE_KINDS(X) \
    X(JSON_NODE_KIND_UNKNOWN, "UNKNOWN") \
    X(JSON_NODE_KIND_OBJECT, "OBJECT") \
    X(JSON_NODE_KIND_ARRAY, "ARRAY") \
    X(JSON_NODE_KIND_VALUE, "VALUE") \

#define JSON_VALUE_KINDS(X) \
    X(JSON_VALUE_KIND_UNKNOWN, "UNKNOWN") \
    X(JSON_VALUE_KIND_OBJECT, "OBJECT") \
    X(JSON_VALUE_KIND_ARRAY, "ARRAY") \
    X(JSON_VALUE_KIND_STRING, "STRING") \
    X(JSON_VALUE_KIND_NUMBER, "NUMBER") \
    X(JSON_VALUE_KIND_TRUE, "TRUE") \
    X(JSON_VALUE_KIND_FALSE, "FALSE") \
    X(JSON_VALUE_KIND_NULL, "NULL") \

GENERATE_ENUM_WITH_NAME_MAPPINGS(JsonErrorCode, JSON_ERROR_CODE_NAMES, JSON_ERROR_CODES);
GENERATE_ENUM_WITH_NAME_MAPPINGS(JsonTokenKind, JSON_TOKEN_KIND_NAMES, JSON_TOKEN_KINDS);
GENERATE_ENUM_WITH_NAME_MAPPINGS(JsonNodeKind, JSON_NODE_KIND_NAMES, JSON_NODE_KINDS);
GENERATE_ENUM_WITH_NAME_MAPPINGS(JsonValueKind, JSON_VALUE_KIND_NAMES, JSON_VALUE_KINDS);

#undef GENERATE_ENUM_VALUES
#undef GENERATE_ENUM_NAME_MAPPING
#undef GENERATE_ENUM_WITH_NAME_MAPPINGS
#undef JSON_ERROR_CODES
#undef JSON_TOKEN_KINDS
#undef JSON_NODE_KINDS
#undef JSON_VALUE_KINDS

#define JSON_BUFFER_SIZE 4096

// TODO: Add Doxygen documentation

typedef struct JsonNode JsonNode;

typedef struct {
    const char *name;
    JsonNode *value;
} JsonMember;

typedef struct {
    JsonMember *members;
    size_t member_count;
} JsonObject;

typedef struct {
    JsonNode *items;
    size_t item_count;
} JsonArray;

typedef struct {
    const char *value;
} JsonString;

typedef enum {
    JSON_NUMBER_KIND_INTEGER = 0,
    JSON_NUMBER_KIND_DOUBLE,
} JsonNumberKind;

typedef struct {
    JsonNumberKind kind;

    union {
        int64_t i;
        double d;
    } value;
} JsonNumber;

typedef struct {
    bool value;
} JsonBoolean;

typedef struct {
    uint8_t _dummy;
} JsonNull;

typedef struct {
    JsonValueKind kind;

    union {
        JsonObject object;
        JsonArray array;
        JsonString string;
        JsonNumber number;
        JsonBoolean boolean;
        JsonNull null;
    };
} JsonValue;

struct JsonNode {
    JsonNodeKind kind;
    JsonNode *root;
    JsonNode *parent;

    union {
        JsonObject object;
        JsonArray array;
        JsonValue value;
    };
};

typedef struct {
    const char **value;
    size_t length;
} JsonPath;

struct JsonNodeCollection {
    JsonNode *items;
    size_t count;
};

typedef struct {
    uint8_t *buffer;
    size_t size;
} JsonMemoryStreamContext;

typedef struct {
    FILE *file;
} JsonFileStreamContext;

typedef struct JsonStream JsonStream;

typedef size_t (*JsonStreamRead)(JsonStream *stream, uint8_t *data, size_t count);
typedef size_t (*JsonStreamWrite)(JsonStream *stream, const uint8_t *data, size_t count);
typedef void (*JsonStreamClose)(JsonStream *stream);

struct JsonStream {
    void *context;
    JsonStreamRead read;
    JsonStreamWrite write;
    JsonStreamClose close;
};

typedef struct {
    JsonTokenKind kind;

    union {
        JsonNumber number;
        JsonString string;
    } value;
} JsonToken;

// TODO: Use some kind of ring buffer to handle tokens at the end of the buffer
typedef struct  {
    JsonStream *stream;
    JsonToken token;
    uint8_t buffer[JSON_BUFFER_SIZE];
    uint32_t global_position;
    uint32_t current_position;
    uint32_t buffer_end;
    uint32_t current_column;
    uint32_t current_line;
    uint8_t can_read;
} JsonReader;

typedef struct  {
    JsonStream *stream;
    JsonNode *current_node;
    uint32_t global_position;
    uint32_t current_position;
    uint8_t can_write;
} JsonWriter;

typedef struct {
    uint32_t max_depth;
    bool use_pretty_printing;
    const char *new_line;
    const char *indent;
} JsonSerializerOptions;

extern const JsonSerializerOptions DEFAULT_SERIALIZER_OPTIONS;

typedef struct {
    JsonErrorCode code;
    const char *code_display_name;
    const char *message;
    uint32_t line;
    uint32_t column;
    JsonNode *node;
} JsonSerializerError;

JsonNode * json_deserialize(JsonStream *stream, JsonSerializerOptions *options);

int32_t json_serialize(JsonNode *node, JsonStream *stream, JsonSerializerOptions *options);

JsonSerializerOptions json_options_default(void);

JsonSerializerError json_error(void);

JsonNode *json_node_get_item(JsonNode *node, const char *name);

JsonNode *json_node_get_item_by_path(JsonNode *node, JsonPath path);

char *json_node_to_string(JsonNode *node);

void json_node_dispose(JsonNode *node);

bool json_reader_move_next(JsonReader *reader);

char json_reader_current_char(const JsonReader *reader);

JsonNode * json_writer_current_node(const JsonWriter *writer);

int32_t json_open_file_stream(JsonStream *stream, const char *file_name, const char *mode);

void json_stream_dispose(JsonStream *stream);

#endif // JSON_LIBRARY_H
