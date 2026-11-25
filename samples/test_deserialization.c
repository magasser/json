#include <stdlib.h>

#include "json.h"

#include <windows.h>

typedef struct {
    int32_t i;
    bool b;
} NestedStruct;

typedef struct {
    const char *name;
    double d;
    uint64_t i;
    NestedStruct nested;
} TestStruct;

int main() {
    const char *file_name = "..\\test_struct.json";

    JsonStream stream;
    if (json_open_file_stream(&stream, file_name, "r") != JSON_RESULT_OK) {
        exit(1);
    }

    JsonNode *node = json_deserialize(&stream, NULL);
    json_stream_dispose(&stream);

    if (!node) {
        const JsonSerializerError error = json_error();
        printf("Failed to deserialize JSON with error: %s (%d): %s at %s:%d:%d\n",
            error.code_display_name,
            error.code,
            error.message,
            file_name,
            error.line,
            error.column);
        exit(1);
    }

    TestStruct test_struct = (TestStruct) {
        .name = json_node_get_item(node, "name")->value.string.value,
        .d = json_node_get_item(node, "d")->value.number.value.d,
        .i = json_node_get_item(node, "i")->value.number.value.i,
        .nested = {
            .i = json_node_get_item_by_path(node, (JsonPath){ .value = (const char *[]){ "nested", "i"}, .length =  2})->value.number.value.i,
            .b = json_node_get_item_by_path(node, (JsonPath){ .value = (const char *[]){ "nested", "b"}, .length =  2})->value.boolean.value,
        }
    };
}
