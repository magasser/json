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