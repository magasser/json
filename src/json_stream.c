#include "json.h"

#include <stdlib.h>

size_t read_from_file(JsonStream *stream,  uint8_t *data, size_t count);
size_t write_to_file(JsonStream *stream, const uint8_t *data, size_t count);
void close_file_stream(JsonStream *stream);

/// FILE STREAM

int32_t json_open_file_stream(JsonStream *stream, const char *file_name, const char *mode) {
    stream->context = NULL;
    stream->read = &read_from_file;
    stream->write = &write_to_file;
    stream->close = &close_file_stream;

    FILE *file = fopen(file_name, mode);

    if (file == NULL) {
        return JSON_RESULT_ERROR;
    }

    JsonFileStreamContext *context = malloc(sizeof(JsonFileStreamContext));

    context->file = file;
    stream->context = context;

    return JSON_RESULT_OK;
}

void json_stream_dispose(JsonStream *stream) {
    stream->close(stream);
    free(stream->context);
}

void close_file_stream(JsonStream *stream) {
    const JsonFileStreamContext *context = stream->context;

    fclose(context->file);
}

size_t read_from_file(JsonStream *stream, uint8_t *data, const size_t count) {
    const JsonFileStreamContext *context = stream->context;

    const size_t read = fread(data, 1, count, context->file);

    if (read != count) {
        // TODO: Use feof and ferror
    }

    return read;
}

size_t write_to_file(JsonStream *stream, const uint8_t *data, const size_t count) {
    const JsonFileStreamContext *context = stream->context;

    const size_t written = fwrite(data, 1, count, context->file);

    if (written != count) {
        // TODO: Handle error?
    }

    return written;
}