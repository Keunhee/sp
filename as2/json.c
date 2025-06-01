#include "json.h"

/**
 * JSON 라이브러리
 * 
 * 이 라이브러리는 JSON 데이터 구조를 생성, 조작, 직렬화 및 파싱하는 기능을 제공합니다.
 * 외부 라이브러리 의존성 없이 C 표준 라이브러리만으로 구현되었습니다.
 * 
 * 주요 기능:
 * - JSON 값 생성 (null, boolean, number, string, array, object)
 * - JSON 값 검사 및 접근
 * - JSON 문자열로 변환 (stringify)
 * - JSON 문자열 파싱 (parse)
 * - 메모리 관리 (할당 및 해제)
 */

/**
 * JSON null 값 생성 함수
 * 
 * @return 새로 생성된 JSON null 값
 */
JsonValue* json_null() {
    JsonValue *value = (JsonValue*)malloc(sizeof(JsonValue));
    if (value) {
        value->type = JSON_NULL;
    }
    return value;
}

JsonValue* json_boolean(int boolean) {
    JsonValue *value = (JsonValue*)malloc(sizeof(JsonValue));
    if (value) {
        value->type = JSON_BOOLEAN;
        value->value.boolean = boolean;
    }
    return value;
}

JsonValue* json_number(double number) {
    JsonValue *value = (JsonValue*)malloc(sizeof(JsonValue));
    if (value) {
        value->type = JSON_NUMBER;
        value->value.number = number;
    }
    return value;
}

JsonValue* json_string(const char *string) {
    JsonValue *value = (JsonValue*)malloc(sizeof(JsonValue));
    if (value) {
        value->type = JSON_STRING;
        value->value.string = strdup(string);
    }
    return value;
}

JsonValue* json_array() {
    JsonValue *value = (JsonValue*)malloc(sizeof(JsonValue));
    if (value) {
        value->type = JSON_ARRAY;
        value->value.array = NULL;
    }
    return value;
}

JsonValue* json_object() {
    JsonValue *value = (JsonValue*)malloc(sizeof(JsonValue));
    if (value) {
        value->type = JSON_OBJECT;
        value->value.object = NULL;
    }
    return value;
}

// JSON 배열에 값 추가
void json_array_append(JsonValue *array, JsonValue *value) {
    if (!array || array->type != JSON_ARRAY || !value) {
        return;
    }
    
    JsonArrayItem *new_item = (JsonArrayItem*)malloc(sizeof(JsonArrayItem));
    if (!new_item) {
        return;
    }
    
    new_item->value = value;
    new_item->next = NULL;
    
    if (!array->value.array) {
        array->value.array = new_item;
    } else {
        JsonArrayItem *item = array->value.array;
        while (item->next) {
            item = item->next;
        }
        item->next = new_item;
    }
}

// JSON 객체에 키-값 쌍 설정
void json_object_set(JsonValue *object, const char *key, JsonValue *value) {
    if (!object || object->type != JSON_OBJECT || !key || !value) {
        return;
    }
    
    // 기존 키가 있는지 확인
    JsonObjectItem *item = object->value.object;
    while (item) {
        if (strcmp(item->key, key) == 0) {
            json_free(item->value);
            item->value = value;
            return;
        }
        item = item->next;
    }
    
    // 새 항목 추가
    JsonObjectItem *new_item = (JsonObjectItem*)malloc(sizeof(JsonObjectItem));
    if (!new_item) {
        return;
    }
    
    new_item->key = strdup(key);
    new_item->value = value;
    new_item->next = object->value.object;
    object->value.object = new_item;
}

// JSON 타입 확인 함수
int json_is_null(const JsonValue *value) {
    return value && value->type == JSON_NULL;
}

int json_is_boolean(const JsonValue *value) {
    return value && value->type == JSON_BOOLEAN;
}

int json_is_number(const JsonValue *value) {
    return value && value->type == JSON_NUMBER;
}

int json_is_string(const JsonValue *value) {
    return value && value->type == JSON_STRING;
}

int json_is_array(const JsonValue *value) {
    return value && value->type == JSON_ARRAY;
}

int json_is_object(const JsonValue *value) {
    return value && value->type == JSON_OBJECT;
}

// JSON 값 접근 함수
int json_boolean_value(const JsonValue *value) {
    return (value && value->type == JSON_BOOLEAN) ? value->value.boolean : 0;
}

double json_number_value(const JsonValue *value) {
    return (value && value->type == JSON_NUMBER) ? value->value.number : 0;
}

const char* json_string_value(const JsonValue *value) {
    return (value && value->type == JSON_STRING) ? value->value.string : NULL;
}

// JSON 객체 키 검색
JsonValue* json_object_get(const JsonValue *object, const char *key) {
    if (!object || object->type != JSON_OBJECT || !key) {
        return NULL;
    }
    
    JsonObjectItem *item = object->value.object;
    while (item) {
        if (strcmp(item->key, key) == 0) {
            return item->value;
        }
        item = item->next;
    }
    
    return NULL;
}

// JSON 배열 인덱스 접근
JsonValue* json_array_get(const JsonValue *array, size_t index) {
    if (!array || array->type != JSON_ARRAY) {
        return NULL;
    }
    
    JsonArrayItem *item = array->value.array;
    size_t current = 0;
    
    while (item && current < index) {
        item = item->next;
        current++;
    }
    
    return (item) ? item->value : NULL;
}

// JSON 배열 크기 확인
size_t json_array_size(const JsonValue *array) {
    if (!array || array->type != JSON_ARRAY) {
        return 0;
    }
    
    size_t size = 0;
    JsonArrayItem *item = array->value.array;
    
    while (item) {
        size++;
        item = item->next;
    }
    
    return size;
}

// JSON 메모리 해제
void json_free(JsonValue *value) {
    if (!value) {
        return;
    }
    
    switch (value->type) {
        case JSON_STRING:
            free(value->value.string);
            break;
        
        case JSON_ARRAY: {
            JsonArrayItem *item = value->value.array;
            while (item) {
                JsonArrayItem *next = item->next;
                json_free(item->value);
                free(item);
                item = next;
            }
            break;
        }
        
        case JSON_OBJECT: {
            JsonObjectItem *item = value->value.object;
            while (item) {
                JsonObjectItem *next = item->next;
                free(item->key);
                json_free(item->value);
                free(item);
                item = next;
            }
            break;
        }
        
        default:
            break;
    }
    
    free(value);
}

// JSON 문자열 변환
static void json_stringify_recursive(const JsonValue *value, char **buffer, size_t *bufsize, size_t *offset);

static void ensure_buffer_size(char **buffer, size_t *bufsize, size_t *offset, size_t needed_size) {
    if (*offset + needed_size >= *bufsize) {
        *bufsize = (*bufsize == 0) ? 16 : *bufsize * 2;
        *buffer = (char*)realloc(*buffer, *bufsize);
    }
}

static void append_string(char **buffer, size_t *bufsize, size_t *offset, const char *str) {
    size_t len = strlen(str);
    ensure_buffer_size(buffer, bufsize, offset, len + 1);
    strcpy(*buffer + *offset, str);
    *offset += len;
}

static void append_char(char **buffer, size_t *bufsize, size_t *offset, char c) {
    ensure_buffer_size(buffer, bufsize, offset, 2);
    (*buffer)[*offset] = c;
    (*buffer)[*offset + 1] = '\0';
    (*offset)++;
}

static void json_stringify_string(const char *string, char **buffer, size_t *bufsize, size_t *offset) {
    append_char(buffer, bufsize, offset, '"');
    
    const char *c = string;
    while (*c) {
        switch (*c) {
            case '\\': append_string(buffer, bufsize, offset, "\\\\"); break;
            case '"': append_string(buffer, bufsize, offset, "\\\""); break;
            case '\b': append_string(buffer, bufsize, offset, "\\b"); break;
            case '\f': append_string(buffer, bufsize, offset, "\\f"); break;
            case '\n': append_string(buffer, bufsize, offset, "\\n"); break;
            case '\r': append_string(buffer, bufsize, offset, "\\r"); break;
            case '\t': append_string(buffer, bufsize, offset, "\\t"); break;
            default:
                append_char(buffer, bufsize, offset, *c);
                break;
        }
        c++;
    }
    
    append_char(buffer, bufsize, offset, '"');
}

static void json_stringify_recursive(const JsonValue *value, char **buffer, size_t *bufsize, size_t *offset) {
    if (!value) {
        append_string(buffer, bufsize, offset, "null");
        return;
    }
    
    switch (value->type) {
        case JSON_NULL:
            append_string(buffer, bufsize, offset, "null");
            break;
        
        case JSON_BOOLEAN:
            append_string(buffer, bufsize, offset, value->value.boolean ? "true" : "false");
            break;
        
        case JSON_NUMBER: {
            char numstr[64];
            snprintf(numstr, sizeof(numstr), "%g", value->value.number);
            append_string(buffer, bufsize, offset, numstr);
            break;
        }
        
        case JSON_STRING:
            json_stringify_string(value->value.string, buffer, bufsize, offset);
            break;
        
        case JSON_ARRAY: {
            append_char(buffer, bufsize, offset, '[');
            
            JsonArrayItem *item = value->value.array;
            int first = 1;
            
            while (item) {
                if (!first) {
                    append_char(buffer, bufsize, offset, ',');
                }
                json_stringify_recursive(item->value, buffer, bufsize, offset);
                first = 0;
                item = item->next;
            }
            
            append_char(buffer, bufsize, offset, ']');
            break;
        }
        
        case JSON_OBJECT: {
            append_char(buffer, bufsize, offset, '{');
            
            JsonObjectItem *item = value->value.object;
            int first = 1;
            
            while (item) {
                if (!first) {
                    append_char(buffer, bufsize, offset, ',');
                }
                
                json_stringify_string(item->key, buffer, bufsize, offset);
                append_char(buffer, bufsize, offset, ':');
                json_stringify_recursive(item->value, buffer, bufsize, offset);
                
                first = 0;
                item = item->next;
            }
            
            append_char(buffer, bufsize, offset, '}');
            break;
        }
    }
}

char* json_stringify(const JsonValue *value) {
    size_t bufsize = 256;
    size_t offset = 0;
    char *buffer = (char*)malloc(bufsize);
    
    if (buffer) {
        buffer[0] = '\0';
        json_stringify_recursive(value, &buffer, &bufsize, &offset);
    }
    
    return buffer;
}

// JSON 파싱
static const char* skip_whitespace(const char *str) {
    while (*str && (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')) {
        str++;
    }
    return str;
}

static const char* parse_value(const char *str, JsonValue **value);

static const char* parse_string(const char *str, char **result) {
    if (*str != '"') {
        return NULL;
    }
    
    str++;
    
    // 문자열 길이 계산
    const char *start = str;
    int escaped = 0;
    int len = 0;
    
    while (*str) {
        if (*str == '\\') {
            escaped = 1;
            str++;
            if (!*str) {
                return NULL; // 불완전한 이스케이프 시퀀스
            }
        } else if (*str == '"' && !escaped) {
            break;
        } else {
            escaped = 0;
        }
        
        str++;
        len++;
    }
    
    if (*str != '"') {
        return NULL; // 종료되지 않은 문자열
    }
    
    // 문자열 복사
    *result = (char*)malloc(len + 1);
    if (!*result) {
        return NULL;
    }
    
    char *out = *result;
    str = start;
    
    while (len > 0) {
        if (*str == '\\') {
            str++;
            switch (*str) {
                case '"': *out = '"'; break;
                case '\\': *out = '\\'; break;
                case '/': *out = '/'; break;
                case 'b': *out = '\b'; break;
                case 'f': *out = '\f'; break;
                case 'n': *out = '\n'; break;
                case 'r': *out = '\r'; break;
                case 't': *out = '\t'; break;
                default: *out = *str; break;
            }
        } else {
            *out = *str;
        }
        
        out++;
        str++;
        len--;
    }
    
    *out = '\0';
    return str + 1; // '"' 건너뛰기
}

static const char* parse_number(const char *str, double *result) {
    char *end;
    *result = strtod(str, &end);
    
    if (end == str) {
        return NULL;
    }
    
    return end;
}

static const char* parse_array(const char *str, JsonValue **value) {
    if (*str != '[') {
        return NULL;
    }
    
    *value = json_array();
    if (!*value) {
        return NULL;
    }
    
    str = skip_whitespace(str + 1);
    
    // 빈 배열 처리
    if (*str == ']') {
        return str + 1;
    }
    
    while (1) {
        JsonValue *item_value = NULL;
        str = parse_value(str, &item_value);
        
        if (!str || !item_value) {
            json_free(*value);
            *value = NULL;
            return NULL;
        }
        
        json_array_append(*value, item_value);
        
        str = skip_whitespace(str);
        if (*str == ']') {
            return str + 1;
        }
        
        if (*str != ',') {
            json_free(*value);
            *value = NULL;
            return NULL;
        }
        
        str = skip_whitespace(str + 1);
    }
}

static const char* parse_object(const char *str, JsonValue **value) {
    if (*str != '{') {
        return NULL;
    }
    
    *value = json_object();
    if (!*value) {
        return NULL;
    }
    
    str = skip_whitespace(str + 1);
    
    // 빈 객체 처리
    if (*str == '}') {
        return str + 1;
    }
    
    while (1) {
        char *key = NULL;
        str = parse_string(str, &key);
        
        if (!str || !key) {
            json_free(*value);
            *value = NULL;
            return NULL;
        }
        
        str = skip_whitespace(str);
        if (*str != ':') {
            free(key);
            json_free(*value);
            *value = NULL;
            return NULL;
        }
        
        JsonValue *item_value = NULL;
        str = parse_value(skip_whitespace(str + 1), &item_value);
        
        if (!str || !item_value) {
            free(key);
            json_free(*value);
            *value = NULL;
            return NULL;
        }
        
        json_object_set(*value, key, item_value);
        free(key);
        
        str = skip_whitespace(str);
        if (*str == '}') {
            return str + 1;
        }
        
        if (*str != ',') {
            json_free(*value);
            *value = NULL;
            return NULL;
        }
        
        str = skip_whitespace(str + 1);
    }
}

static const char* parse_value(const char *str, JsonValue **value) {
    str = skip_whitespace(str);
    
    switch (*str) {
        case 'n':
            if (strncmp(str, "null", 4) == 0) {
                *value = json_null();
                return str + 4;
            }
            break;
        
        case 't':
            if (strncmp(str, "true", 4) == 0) {
                *value = json_boolean(1);
                return str + 4;
            }
            break;
        
        case 'f':
            if (strncmp(str, "false", 5) == 0) {
                *value = json_boolean(0);
                return str + 5;
            }
            break;
        
        case '"': {
            char *string_value;
            const char *new_str = parse_string(str, &string_value);
            if (new_str) {
                *value = json_string(string_value);
                free(string_value);
                return new_str;
            }
            break;
        }
        
        case '[':
            return parse_array(str, value);
        
        case '{':
            return parse_object(str, value);
        
        default:
            if ((*str >= '0' && *str <= '9') || *str == '-') {
                double number_value;
                const char *new_str = parse_number(str, &number_value);
                if (new_str) {
                    *value = json_number(number_value);
                    return new_str;
                }
            }
            break;
    }
    
    return NULL;
}

JsonValue* json_parse(const char *string) {
    JsonValue *value = NULL;
    const char *end = parse_value(string, &value);
    
    if (!end || *skip_whitespace(end) != '\0') {
        json_free(value);
        return NULL;
    }
    
    return value;
}