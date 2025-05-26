#ifndef JSON_H
#define JSON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// JSON 값 유형
typedef enum {
    JSON_NULL,
    JSON_BOOLEAN,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

// JSON 값 구조체
typedef struct JsonValue JsonValue;

// JSON 객체 항목
typedef struct JsonObjectItem {
    char *key;
    JsonValue *value;
    struct JsonObjectItem *next;
} JsonObjectItem;

// JSON 배열 항목
typedef struct JsonArrayItem {
    JsonValue *value;
    struct JsonArrayItem *next;
} JsonArrayItem;

// JSON 값
struct JsonValue {
    JsonType type;
    union {
        int boolean;
        double number;
        char *string;
        JsonArrayItem *array;
        JsonObjectItem *object;
    } value;
};

// JSON 값 생성 함수
JsonValue* json_null();
JsonValue* json_boolean(int boolean);
JsonValue* json_number(double number);
JsonValue* json_string(const char *string);
JsonValue* json_array();
JsonValue* json_object();

// JSON 배열/객체 수정 함수
void json_array_append(JsonValue *array, JsonValue *value);
void json_object_set(JsonValue *object, const char *key, JsonValue *value);

// JSON 값 접근 함수
int json_is_null(const JsonValue *value);
int json_is_boolean(const JsonValue *value);
int json_is_number(const JsonValue *value);
int json_is_string(const JsonValue *value);
int json_is_array(const JsonValue *value);
int json_is_object(const JsonValue *value);

int json_boolean_value(const JsonValue *value);
double json_number_value(const JsonValue *value);
const char* json_string_value(const JsonValue *value);

// JSON 객체 키 검색
JsonValue* json_object_get(const JsonValue *object, const char *key);

// JSON 배열 인덱스 접근
JsonValue* json_array_get(const JsonValue *array, size_t index);
size_t json_array_size(const JsonValue *array);

// JSON 문자열 변환
char* json_stringify(const JsonValue *value);
JsonValue* json_parse(const char *string);

// JSON 값 메모리 해제
void json_free(JsonValue *value);

#endif /* JSON_H */