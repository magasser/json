#include <stdlib.h>

#include "json.h"
#include "json_reader.h"

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
    JsonStream stream;
    if (json_open_file_stream(&stream, "..\\test_struct.json", "r") != JSON_RESULT_OK) {
        exit(1);
    }

    JsonNode *node = json_deserialize(&stream, NULL);
    json_stream_dispose(&stream);

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
