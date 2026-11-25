#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "json.h"

/* Forward declarations. */
static JsonNode sample_root;

static JsonNode n_message;
static JsonNode n_int64_max;
static JsonNode n_int64_min;
static JsonNode n_unsafe_integer;
static JsonNode n_pi;
static JsonNode n_scientific;
static JsonNode n_true;
static JsonNode n_false;
static JsonNode n_null;

static JsonNode n_mixed_array;
static JsonNode n_nested_object;
static JsonNode n_nested_label;
static JsonNode n_nested_enabled;
static JsonNode n_nested_count;
static JsonNode n_tags_array;

static JsonNode n_empty_object;
static JsonNode n_empty_array;


/* Helper macros for initializing nodes. */

#define JSON_TEST_STRING(parent_, text_) \
    { \
        .kind = JSON_NODE_KIND_VALUE, \
        .root = &sample_root, \
        .parent = (parent_), \
        .value = { \
            .kind = JSON_VALUE_KIND_STRING, \
            .string = { .value = (text_) } \
        } \
    }

#define JSON_TEST_INTEGER(parent_, number_) \
    { \
        .kind = JSON_NODE_KIND_VALUE, \
        .root = &sample_root, \
        .parent = (parent_), \
        .value = { \
            .kind = JSON_VALUE_KIND_NUMBER, \
            .number = { \
                .kind = JSON_NUMBER_KIND_INTEGER, \
                .value = { .i = (number_) } \
            } \
        } \
    }

#define JSON_TEST_DOUBLE(parent_, number_) \
    { \
        .kind = JSON_NODE_KIND_VALUE, \
        .root = &sample_root, \
        .parent = (parent_), \
        .value = { \
            .kind = JSON_VALUE_KIND_NUMBER, \
            .number = { \
                .kind = JSON_NUMBER_KIND_DOUBLE, \
                .value = { .d = (number_) } \
            } \
        } \
    }

#define JSON_TEST_TRUE(parent_) \
    { \
        .kind = JSON_NODE_KIND_VALUE, \
        .root = &sample_root, \
        .parent = (parent_), \
        .value = { \
            .kind = JSON_VALUE_KIND_TRUE, \
            .boolean = { .value = (true) } \
        } \
}

#define JSON_TEST_FALSE(parent_) \
    { \
    .kind = JSON_NODE_KIND_VALUE, \
    .root = &sample_root, \
    .parent = (parent_), \
    .value = { \
    .kind = JSON_VALUE_KIND_FALSE, \
    .boolean = { .value = (true) } \
    } \
    }

#define JSON_TEST_NULL(parent_) \
    { \
        .kind = JSON_NODE_KIND_VALUE, \
        .root = &sample_root, \
        .parent = (parent_), \
        .value = { \
            .kind = JSON_VALUE_KIND_NULL, \
            .null = { ._dummy = 0 } \
        } \
    }

#define JSON_TEST_ARRAY(parent_, items_) \
    { \
        .kind = JSON_NODE_KIND_ARRAY, \
        .root = &sample_root, \
        .parent = (parent_), \
        .array = { \
            .items = (items_), \
            .item_count = sizeof(items_) / sizeof((items_)[0]) \
        } \
    }

#define JSON_TEST_OBJECT(parent_, members_) \
    { \
        .kind = JSON_NODE_KIND_OBJECT, \
        .root = &sample_root, \
        .parent = (parent_), \
        .object = { \
            .members = (members_), \
            .member_count = sizeof(members_) / sizeof((members_)[0]) \
        } \
    }


/* Mixed array contents. */

static JsonNode mixed_items[] = {
    JSON_TEST_STRING(&n_mixed_array, "First element"),
    JSON_TEST_INTEGER(&n_mixed_array, -123456789),
    JSON_TEST_DOUBLE(&n_mixed_array, -0.125),
    JSON_TEST_TRUE(&n_mixed_array),
    JSON_TEST_NULL(&n_mixed_array),
    JSON_TEST_STRING(&n_mixed_array, "Another element")
};


/* Nested array contents. */

static JsonNode tag_items[] = {
    JSON_TEST_STRING(&n_tags_array, "alpha"),
    JSON_TEST_STRING(&n_tags_array, "beta"),
    JSON_TEST_STRING(&n_tags_array, "gamma")
};


/* Nested object members. */

static JsonMember nested_members[] = {
    { "label",   &n_nested_label },
    { "enabled", &n_nested_enabled },
    { "count",   &n_nested_count },
    { "tags",    &n_tags_array }
};


/* Root object members. */

static JsonMember root_members[] = {
    { "message",                &n_message },
    { "int64_max",              &n_int64_max },
    { "int64_min",              &n_int64_min },
    { "beyond_js_safe_integer", &n_unsafe_integer },
    { "pi",                     &n_pi },
    { "scientific",             &n_scientific },
    { "boolean_true",           &n_true },
    { "boolean_false",          &n_false },
    { "null_value",             &n_null },
    { "mixed_array",            &n_mixed_array },
    { "nested_object",          &n_nested_object },
    { "empty_object",           &n_empty_object },
    { "empty_array",            &n_empty_array }
};


/* Actual node definitions. */

static JsonNode n_message =
    JSON_TEST_STRING(
        &sample_root,
        "Line 1\nLine 2\tquoted: \"hello\"; backslash: \\; UTF-8: Grüße — 世界"
    );

static JsonNode n_int64_max =
    JSON_TEST_INTEGER(&sample_root, INT64_MAX);

static JsonNode n_int64_min =
    JSON_TEST_INTEGER(&sample_root, INT64_MIN);

static JsonNode n_unsafe_integer =
    JSON_TEST_INTEGER(&sample_root, INT64_C(9007199254740993));

static JsonNode n_pi =
    JSON_TEST_DOUBLE(&sample_root, 3.141592653589793);

static JsonNode n_scientific =
    JSON_TEST_DOUBLE(&sample_root, 6.02214076e23);

static JsonNode n_true =
    JSON_TEST_TRUE(&sample_root);

static JsonNode n_false =
    JSON_TEST_FALSE(&sample_root);

static JsonNode n_null =
    JSON_TEST_NULL(&sample_root);


/* Mixed array. */

static JsonNode n_mixed_array =
    JSON_TEST_ARRAY(&sample_root, mixed_items);


/* Nested object fields. */

static JsonNode n_nested_label =
    JSON_TEST_STRING(&n_nested_object, "Grüße — 世界");

static JsonNode n_nested_enabled =
    JSON_TEST_TRUE(&n_nested_object);

static JsonNode n_nested_count =
    JSON_TEST_INTEGER(&n_nested_object, 42);

static JsonNode n_tags_array =
    JSON_TEST_ARRAY(&n_nested_object, tag_items);


/* Nested object. */

static JsonNode n_nested_object =
    JSON_TEST_OBJECT(&sample_root, nested_members);


/* Empty object. */

static JsonNode n_empty_object = {
    .kind = JSON_NODE_KIND_OBJECT,
    .root = &sample_root,
    .parent = &sample_root,
    .object = {
        .members = NULL,
        .member_count = 0
    }
};


/* Empty array. */

static JsonNode n_empty_array = {
    .kind = JSON_NODE_KIND_ARRAY,
    .root = &sample_root,
    .parent = &sample_root,
    .array = {
        .items = NULL,
        .item_count = 0
    }
};


/* Root node. */

static JsonNode sample_root = {
    .kind = JSON_NODE_KIND_OBJECT,
    .root = &sample_root,
    .parent = NULL,
    .object = {
        .members = root_members,
        .member_count = sizeof(root_members) / sizeof(root_members[0])
    }
};


/* Public function returning the test tree. */

int main(void) {
    const char *file_name = "..\\test_output.json";

    JsonStream stream;
    if (json_open_file_stream(&stream, file_name, "w") != JSON_RESULT_OK) {
        exit(1);
    }

    JsonSerializerOptions options = DEFAULT_SERIALIZER_OPTIONS;
    options.use_pretty_printing = true;
    options.indent = "\t";

    const int32_t result = json_serialize(&sample_root, &stream, &options);

    if (result != JSON_RESULT_OK) {
        const JsonSerializerError error = json_error();
        char *node_string = json_node_to_string(error.node);
        printf("Failed to serialize JSON with error: %s (%d): %s for %s\n",
            error.code_display_name,
            error.code,
            error.message,
            node_string);
        free(node_string);
        exit(1);
    }
}


/* Clean up helper macros. */

#undef JSON_TEST_STRING
#undef JSON_TEST_INTEGER
#undef JSON_TEST_DOUBLE
#undef JSON_TEST_BOOLEAN
#undef JSON_TEST_NULL
#undef JSON_TEST_ARRAY
#undef JSON_TEST_OBJECT