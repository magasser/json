#include "json_reader.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

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
#define NEWLINE                 '\n'
#define ESCAPE                  '\\'
#define REVERSE_SOLIDUS         '\\'
#define SOLIDUS                 '/'
#define BACKSPACE               'b'
#define FORM_FEED               'f'
#define LINE_FEED               'n'
#define CARRIAGE_RETURN         'r'
#define TAB                     't'
#define U_XXXX                  'u'


#ifdef _WIN32
char *strndup(const char *src, size_t n) {
    size_t len = 0;
    while (len < n && src[len] != '\0') {
        len++;
    }

    char *dst = malloc(len + 1);

    memcpy(dst, src, len);
    dst[len] = '\0';

    return dst;
}
#endif

typedef struct {
    const uint8_t *data;
    size_t length;
} BufferView;

uint8_t reader_is_end_of_buffer(const JsonReader *reader);
size_t reader_size(const JsonReader *reader);
void skip_whitespace(JsonReader *reader);
uint8_t reader_advance(JsonReader *reader, uint32_t count);
uint8_t reader_peek(const JsonReader *reader, uint32_t count, uint8_t *value);
uint8_t match_literal(const JsonReader *reader, const char *literal);
uint8_t match_character(const JsonReader *reader, char character);
uint8_t match_number(const JsonReader *reader, BufferView *value);
uint8_t match_string(const JsonReader *reader, BufferView *value);
int32_t parse_number(const BufferView *value, JsonNumber *number);

int32_t json_reader_init(JsonReader *reader, JsonStream *stream) {
    reader->token = (JsonToken){0};
    reader->stream = stream;
    reader->read_position = 0;
    reader->end_position = 0;
    reader->can_read = 1;

    return JSON_RESULT_OK;
}

bool json_reader_move_next(JsonReader *reader) {
    if (!reader->can_read) {
        return false;
    }

    if (reader_is_end_of_buffer(reader)) {
        const size_t read = reader->stream->read(reader->stream, reader->buffer, sizeof(reader->buffer));

        if (read <= 0) {
            reader->can_read = 0;
            return false;
        }

        reader->read_position = 0;
        reader->end_position = read - 1;
    }

    JsonToken token = {0};
    size_t token_size = 0;
    BufferView value = {0};

    skip_whitespace(reader);

    if (match_character(reader, JSON_BEGIN_OBJECT)) {
        token.kind = JSON_TOKEN_KIND_BEGIN_OBJECT;
        token_size = 1;
        goto found;
    }
    if (match_character(reader, JSON_END_OBJECT)) {
        token.kind = JSON_TOKEN_KIND_END_OBJECT;
        token_size = 1;
        goto found;
    }
    if (match_character(reader, JSON_BEGIN_ARRAY)) {
        token.kind = JSON_TOKEN_KIND_BEGIN_ARRAY;
        token_size = 1;
        goto found;
    }
    if (match_character(reader, JSON_END_ARRAY)) {
        token.kind = JSON_TOKEN_KIND_END_ARRAY;
        token_size = 1;
        goto found;
    }
    if (match_character(reader, JSON_NAME_SEPARATOR)) {
        token.kind = JSON_TOKEN_KIND_NAME_SEPARATOR;
        token_size = 1;
        goto found;
    }
    if (match_character(reader, JSON_VALUE_SEPARATOR)) {
        token.kind = JSON_TOKEN_KIND_VALUE_SEPARATOR;
        token_size = 1;
        goto found;
    }
    if (match_literal(reader, JSON_LITERAL_FALSE)) {
        token.kind = JSON_TOKEN_KIND_FALSE;
        token_size = 5;
        goto found;
    }
    if (match_literal(reader, JSON_LITERAL_TRUE)) {
        token.kind = JSON_TOKEN_KIND_TRUE;
        token_size = 4;
        goto found;
    }
    if (match_literal(reader, JSON_LITERAL_NULL)) {
        token.kind = JSON_TOKEN_KIND_NULL;
        token_size = 4;
        goto found;
    }
    if (match_number(reader, &value)) {
        JsonNumber number = {0};
        if (parse_number(&value, &number) != JSON_RESULT_OK) {
            return JSON_RESULT_ERROR;
        }

        token.kind = JSON_TOKEN_KIND_NUMBER;
        token.value.number = number;
        token_size = value.length;
        goto found;
    }
    if (match_string(reader, &value)) {
        token.kind = JSON_TOKEN_KIND_STRING;
        token.value.string = (JsonString){
            .value = strndup((char *)value.data, value.length)
        };
        token_size = value.length + 2;
        goto found;
    }

    return false;

found:
    reader->token = token;
    reader_advance(reader, token_size);
    return true;
}

uint8_t reader_is_end_of_buffer(const JsonReader *reader) {
    return reader->read_position >= reader->end_position;
}

void skip_whitespace(JsonReader *reader) {
    while (!reader_is_end_of_buffer(reader)) {
        const uint8_t c = reader->buffer[reader->read_position];

        if (isspace(c)) {
            reader->read_position++;
            continue;
        }

        return;
    }
}

uint8_t reader_advance(JsonReader *reader, const uint32_t count) {
    if (reader->read_position + count > reader->end_position) {
        return 0;
    }

    reader->read_position += count;
    return 1;
}

uint8_t reader_peek(const JsonReader *reader, const uint32_t count, uint8_t *value) {
    if (reader->read_position + count > reader->end_position) {
        return 0;
    }

    *value = reader->buffer[reader->read_position + count];
    return 1;
}

size_t reader_size(const JsonReader *reader) {
    return reader->end_position - reader->read_position + 1;
}

void reader_get(const JsonReader *reader, char *value, const size_t length) {
    assert(reader_size(reader) >= length);

    memcpy(value, reader->buffer + reader->read_position, length);
}

uint8_t match_literal(const JsonReader *reader, const char *literal) {
    const uint8_t length = strlen(literal);
    if (reader_size(reader) < length) {
        return 0;
    }

    char value[length];
    reader_get(reader, value, length);

    return strncmp(value, literal, length) == 0;
}

uint8_t match_character(const JsonReader *reader, char character) {
    if (reader_size(reader) < 1) {
        return 0;
    }

    return reader->buffer[reader->read_position] == character;
}
uint8_t match_number(const JsonReader *reader, BufferView *value) {
    if (reader_size(reader) < 1) {
        return 0;
    }

    uint8_t c;
    size_t value_length = match_character(reader, '-') ? 1 : 0;

    while (reader_peek(reader, value_length, &c)) {
        if (!isdigit(c)) {
            break;
        }
        value_length++;
    }

    if (reader_peek(reader, value_length, &c) && c == '.') {
        value_length++;

        size_t before = value_length;
        while (reader_peek(reader, value_length, &c)) {
            if (!isdigit(c)) {
                break;
            }
            value_length++;
        }

        if (before == value_length) {
            return 0;
        }
    }

    if (reader_peek(reader, value_length, &c) && (c == 'e' || c == 'E')) {
        value_length++;
        if (!reader_peek(reader, value_length, &c) || (c != '-' && c != '+')) {
            return 0;
        }

        value_length++;

        size_t before = value_length;
        while (reader_peek(reader, value_length, &c)) {
            if (!isdigit(c)) {
                break;
            }
            value_length++;
        }

        if (before == value_length) {
            return 0;
        }
    }

    if (value_length == 0) {
        return 0;
    }

    *value = (BufferView) {
        .length = value_length,
        .data = &reader->buffer[reader->read_position]
    };

    return 1;
}
uint8_t match_string(const JsonReader *reader, BufferView *value) {
    if (!match_character(reader, QUOTATION_MARK)) {
        return 0;
    }

    uint8_t c;
    size_t value_length = 0;

    bool escaped = false;
    while (reader_peek(reader, value_length + 1, &c)) {
        if (c == NEWLINE) {
            return 0;
        }

        if (!escaped && c == ESCAPE) {
            escaped = true;
            value_length++;
            continue;
        }

        if (!escaped && c == QUOTATION_MARK) {
            break;
        }

        value_length++;

        if (escaped) {
            escaped = false;
        }
    }

    *value = (BufferView) {
            .length = value_length,
            .data = &reader->buffer[reader->read_position + 1]
        };

    return 1;
}

int32_t parse_number(const BufferView *value, JsonNumber *number) {
    char number_string[value->length + 1];
    memcpy(number_string, value->data, value->length);
    number_string[value->length] = '\n';

    if (strchr(number_string, '.')) {
        number->kind = JSON_NUMBER_KIND_DOUBLE;
        number->value.d = strtod(number_string, NULL);
        return JSON_RESULT_OK;
    }

    number->kind = JSON_NUMBER_KIND_INTEGER;
    number->value.i = strtoimax(number_string, NULL, 10);
    return JSON_RESULT_OK;
}
