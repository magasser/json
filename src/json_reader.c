#include "json.h"

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

uint8_t is_end_of_buffer(const JsonReader *reader);
size_t reader_size(const JsonReader *reader);
void skip_whitespace(JsonReader *reader);
uint8_t reader_advance(JsonReader *reader, uint32_t count);
uint8_t reader_peek(const JsonReader *reader, uint32_t count, uint8_t *value);
uint8_t match_literal(const JsonReader *reader, const char *literal);
bool match_character(const JsonReader *reader, char character);
bool match_character_at(const JsonReader *reader, char character, uint32_t offset);
uint8_t match_number(const JsonReader *reader, BufferView *value);
uint8_t match_string(const JsonReader *reader, BufferView *value);
int32_t parse_number(const BufferView *value, JsonNumber *number);

int32_t json_reader_init(JsonReader *reader, JsonStream *stream) {
    reader->token = (JsonToken){0};
    reader->stream = stream;
    reader->global_position = 0;
    reader->current_position = 0;
    reader->buffer_end = 0;
    reader->current_line = 1;
    reader->current_column = 1;
    reader->can_read = 1;

    return JSON_RESULT_OK;
}

bool json_reader_move_next(JsonReader *reader) {
    if (!reader->can_read) {
        return false;
    }

    if (is_end_of_buffer(reader)) {
        const size_t read = reader->stream->read(reader->stream, reader->buffer, sizeof(reader->buffer));

        if (read <= 0) {
            reader->can_read = 0;
            return false;
        }

        reader->current_position = 0;
        reader->buffer_end = read - 1;
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

char json_reader_current_char(const JsonReader *reader) {
    if (reader_size(reader) < 1) {
        return '\0';
    }

    return (char)reader->buffer[reader->current_position];
}

uint8_t is_end_of_buffer(const JsonReader *reader) {
    return reader->current_position >= reader->buffer_end;
}

void skip_whitespace(JsonReader *reader) {
    while (!is_end_of_buffer(reader)) {
        const uint8_t c = reader->buffer[reader->current_position];

        if (isspace(c)) {
            const bool is_newline = c == '\r' || c == '\n';
            const uint8_t advance_by = c == '\r' && match_character_at(reader, '\n', 1) ? 2 : 1;

            reader_advance(reader, advance_by);
            reader->current_line += is_newline ? 1 : 0;
            reader->current_column = is_newline ? 1 : reader->current_column;
            continue;
        }

        return;
    }
}

uint8_t reader_advance(JsonReader *reader, const uint32_t count) {
    if (reader->current_position + count > reader->buffer_end) {
        return 0;
    }

    reader->global_position += count;
    reader->current_position += count;
    reader->current_column += count;
    return 1;
}

uint8_t reader_peek(const JsonReader *reader, const uint32_t count, uint8_t *value) {
    if (reader->current_position + count > reader->buffer_end) {
        return 0;
    }

    *value = reader->buffer[reader->current_position + count];
    return 1;
}

size_t reader_size(const JsonReader *reader) {
    return reader->buffer_end - reader->current_position + 1;
}

void reader_get(const JsonReader *reader, char *value, const size_t length) {
    assert(reader_size(reader) >= length);

    memcpy(value, reader->buffer + reader->current_position, length);
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

bool match_character(const JsonReader *reader, char character) {
    return match_character_at(reader, character, 0);
}

bool match_character_at(const JsonReader *reader, char character, uint32_t offset) {
    if (reader_size(reader) < 1 + offset) {
        return 0;
    }

    return reader->buffer[reader->current_position + offset] == character;
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
        .data = &reader->buffer[reader->current_position]
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
        if (c == '\n' || c == '\r') {
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
            .data = &reader->buffer[reader->current_position + 1]
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
