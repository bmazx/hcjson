#ifndef HG_HCJSON_H
#define HG_HCJSON_H

// assert
#ifndef HCJSON_ASSERT
    #define HCJSON_ASSERT(x, str) assert((x) && str)
#endif

// memory
#if defined(HCJSON_MALLOC) && defined(HCJSON_FREE)
// ok
#elif !defined(HCJSON_MALLOC) && !defined(HCJSON_FREE)
    #define HCJSON_MALLOC(sz) malloc(sz)
    #define HCJSON_FREE(ptr)  free(ptr)
#else
    #error "must define both HCJSON_MALLOC and HCJSON_FREE or neither"
#endif

// config
#ifndef HCJSON_INDENT_LENGTH
    #define HCJSON_INDENT_LENGTH (4)
#endif
#ifndef HCJSON_NUMBER_BUFF_SIZE
    #define HCJSON_NUMBER_BUFF_SIZE (64)
#endif

#define HCJSON_TYPE_INVALID      (0x00000000)
#define HCJSON_TYPE_TRUE         (0x00000001)
#define HCJSON_TYPE_FALSE        (0x00000002)
#define HCJSON_TYPE_NULL         (0x00000004)
#define HCJSON_TYPE_STRING       (0x00000008)
#define HCJSON_TYPE_ARRAY        (0x00000010)
#define HCJSON_TYPE_OBJECT       (0x00000020)
#define HCJSON_TYPE_NUMBER       (0x00000040)
#define HCJSON_TAG_TRANSFERRED   (0x80000000)

#define HCJSON_TOSTR_FLAG_INLINE              (0x00000001)
#define HCJSON_TOSTR_FLAG_WHITESPACE          (0x00000002)
#define HCJSON_TOSTR_FLAG_USE_TABS            (0x00000004)
#define HCJSON_TOSTR_FLAG_CAST_NUMBER_TYPES   (0x00000008)

#define HCJSON_SUCCESS (0)
#define HCJSON_ERROR_TASK_FAILED (-1)
#define HCJSON_ERROR_MALLOC_FAILURE (-2)
#define HCJSON_ERROR_INVALID_TYPE (-3)
#define HCJSON_ERROR_ITEM_ALREADY_TRANSFERRED (-4)

#define HCJSON_TOKEN_BUFF_SIZE (64)
#define HCJSON_TOKEN_BUFF_MULTIPLIER (2)

#include <stdint.h>
#include <stdbool.h>


typedef int32_t hcjson_flag;
typedef int32_t hcjson_result;

typedef struct hcjson {
    struct hcjson *prev;
    struct hcjson *next;
    int32_t type;
    char* key;
    union {
        struct hcjson *child;
        char *str;
        double num;
    } value;
    struct hcjson *m_tail;
} hcjson;


#ifdef __cplusplus
extern "C" {
#endif

hcjson *hcjson_create_object(void);
hcjson *hcjson_create_array(void);
hcjson *hcjson_create_true(void);
hcjson *hcjson_create_false(void);
hcjson *hcjson_create_null(void);
hcjson *hcjson_create_string(const char *str);
hcjson *hcjson_create_number(double num);

void hcjson_destroy(hcjson *json);

bool hcjson_is_object(const hcjson *json);
bool hcjson_is_array(const hcjson *json);
bool hcjson_is_true(const hcjson *json);
bool hcjson_is_false(const hcjson *json);
bool hcjson_is_null(const hcjson *json);
bool hcjson_is_string(const hcjson *json);
bool hcjson_is_number(const hcjson *json);

bool hcjson_is_number_int(const hcjson *json);


hcjson_result hcjson_add_item_to_object(hcjson *obj, const char *key, hcjson *item);
hcjson_result hcjson_copy_item_to_object(hcjson *obj, const char *key, const hcjson *item);
hcjson_result hcjson_destroy_item_in_object(hcjson *obj, const char *key);
hcjson *hcjson_remove_item_in_object(hcjson *obj, const char *key);
hcjson *hcjson_get_item_in_object(hcjson *obj, const char *key);

hcjson_result hcjson_add_item_to_array(hcjson *arr, hcjson *item);
hcjson_result hcjson_copy_item_to_array(hcjson *arr, const hcjson *item);
hcjson_result hcjson_destroy_item_in_array(hcjson *arr, uint32_t index);
hcjson *hcjson_remove_item_in_array(hcjson *arr, uint32_t index);
hcjson *hcjson_get_item_in_array(hcjson *arr, uint32_t index);

uint32_t hcjson_list_size(hcjson *json);

char *hcjson_to_string(hcjson *json);
char *hcjson_to_string_format(hcjson *json, hcjson_flag flags);

hcjson *hcjson_parse(const char* json);


#ifdef __cplusplus
}
#endif

#define HCJSON_IMPL /* NOTE: temp */
#ifdef HCJSON_IMPL

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

typedef enum hcjson_token {
    HCJSON_TOKEN_INVALID,
    HCJSON_TOKEN_LBRACE,
    HCJSON_TOKEN_RBRACE,
    HCJSON_TOKEN_LBRACKET,
    HCJSON_TOKEN_RBRACKET,
    HCJSON_TOKEN_COMMA,
    HCJSON_TOKEN_COLON,
    HCJSON_TOKEN_TRUE,
    HCJSON_TOKEN_FALSE,
    HCJSON_TOKEN_NULL,
    HCJSON_TOKEN_STRING,
    HCJSON_TOKEN_NUMBER,
    HCJSON_TOKEN_SPACING,
    HCJSON_TOKEN_NEWLINE,
} hcjson_token;

typedef struct hcjson_token_str {
    hcjson_token token;
    const char *str;
    size_t len;
    double num;
} hcjson_token_str;

typedef struct hcjson_token_buffer {
    hcjson_token_str *tokens;
    size_t size;
    size_t capacity;
} hcjson_token_buffer;

typedef struct hcjson_parser {
    const char *json;
    hcjson_token_buffer tbuff;
    size_t tbuff_index;
} hcjson_parser;

void *hcjson__malloc(size_t size);
void hcjson__free(void* ptr);
char *hcjson__strdup(const char* str);

void *hcjson__malloc(size_t size) {
    if (!size) { return NULL; }
    return HCJSON_MALLOC(size);
}

void hcjson__free(void* ptr) {
    if (!ptr) { return; }
    HCJSON_FREE(ptr);
}

char *hcjson__strdup(const char* str) {
    HCJSON_ASSERT(str, "str cannot be null");
    size_t length = strlen(str) + 1;
    char *result = (char*) hcjson__malloc(length);
    if (!result) { return NULL; }
    memcpy(result, str, length);
    return result;
}

#endif /* HCJSON_IMPL */

#endif
