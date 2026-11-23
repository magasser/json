#ifndef JSON_JSON_STREAM_H
#define JSON_JSON_STREAM_H

#include <stdint.h>
#include <stdio.h>

#define JSON_BUFFER_SIZE 4096

#include "json_types.h"

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

int32_t json_open_file_stream(JsonStream *stream, const char *file_name, const char *mode);

void json_stream_dispose(JsonStream *stream);

#endif //JSON_JSON_STREAM_H
