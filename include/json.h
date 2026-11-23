#ifndef JSON_LIBRARY_H
#define JSON_LIBRARY_H

#include "json_types.h"
#include "json_stream.h"

JsonNode * json_deserialize(JsonStream *stream, JsonSerializerOptions *options);

int32_t json_serialize(JsonNode *node, JsonStream *stream, JsonSerializerOptions *options);

#endif // JSON_LIBRARY_H
