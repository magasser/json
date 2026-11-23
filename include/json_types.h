#ifndef JSON_JSON_TYPES_H
#define JSON_JSON_TYPES_H

#define JSON_RESULT_OK 0
#ifdef JSON_DEBUG
    #define JSON_RESULT_ERROR (__debugbreak(), -1)
#else
    #define JSON_RESULT_ERROR (-1)
#endif
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

typedef enum {
    JSON_VALUE_KIND_UNKNOWN = 0,
    JSON_VALUE_KIND_OBJECT,
    JSON_VALUE_KIND_ARRAY,
    JSON_VALUE_KIND_STRING,
    JSON_VALUE_KIND_NUMBER,
    JSON_VALUE_KIND_TRUE,
    JSON_VALUE_KIND_FALSE,
    JSON_VALUE_KIND_NULL,
} JsonValueKind;

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

typedef enum {
    JSON_NODE_KIND_UNKNOWN = 0,
    JSON_NODE_KIND_OBJECT,
    JSON_NODE_KIND_ARRAY,
    JSON_NODE_KIND_VALUE,
} JsonNodeKind;

typedef struct JsonNodeCollection JsonNodeCollection;

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
    uint8_t _dummy;
} JsonSerializerOptions;

JsonNode *json_node_get_item(JsonNode *node, const char *name);

JsonNode *json_node_get_item_by_path(JsonNode *node, JsonPath path);

void json_node_dispose(JsonNode *node);

#endif //JSON_JSON_TYPES_H
