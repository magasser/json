#ifndef JSON_JSON_TOKENIZER_H
#define JSON_JSON_TOKENIZER_H

#include <stdint.h>

#include "json_stream.h"
#include "json_types.h"

#define JSON_TOKENIZER_BUFFER_SIZE 4096

typedef enum {
    JSON_TOKEN_KIND_NONE = 0,
    JSON_TOKEN_KIND_BEGIN_ARRAY,
    JSON_TOKEN_KIND_BEGIN_OBJECT,
    JSON_TOKEN_KIND_END_ARRAY,
    JSON_TOKEN_KIND_END_OBJECT,
    JSON_TOKEN_KIND_NAME_SEPARATOR,
    JSON_TOKEN_KIND_VALUE_SEPARATOR,
    JSON_TOKEN_KIND_NUMBER,
    JSON_TOKEN_KIND_STRING,
    JSON_TOKEN_KIND_FALSE,
    JSON_TOKEN_KIND_TRUE,
    JSON_TOKEN_KIND_NULL,
} JsonTokenKind;

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
    uint32_t read_position;
    uint32_t end_position;
    uint8_t can_read;
} JsonReader;

int32_t json_reader_init(JsonReader *reader, JsonStream *stream);

bool json_reader_move_next(JsonReader *reader);

#endif //JSON_JSON_TOKENIZER_H
