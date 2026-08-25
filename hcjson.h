#ifndef HG_HCJSON_H
#define HG_HCJSON_H

/* assert */
#ifndef HCJSON_ASSERT
    #define HCJSON_ASSERT(x, str) assert((x) && str)
#endif

/* memory */
#if defined(HCJSON_MALLOC) && defined(HCJSON_FREE)
/* ok */
#elif !defined(HCJSON_MALLOC) && !defined(HCJSON_FREE)
    #define HCJSON_MALLOC(sz) malloc(sz)
    #define HCJSON_FREE(ptr)  free(ptr)
#else
    #error "must define both HCJSON_MALLOC and HCJSON_FREE or neither"
#endif

#define HCJSON_TYPE_INVALID      (0x00000000)
#define HCJSON_TYPE_TRUE         (0x00000001)
#define HCJSON_TYPE_FALSE        (0x00000002)
#define HCJSON_TYPE_NULL         (0x00000004)
#define HCJSON_TYPE_STRING       (0x00000008)
#define HCJSON_TYPE_ARRAY        (0x00000010)
#define HCJSON_TYPE_OBJECT       (0x00000020)
#define HCJSON_TYPE_NUMBER       (0x00000040)
#define HCJSON_TYPE_NUMBER_INT   (0x00000080)
#define HCJSON_TYPE_NUMBER_UINT  (0x00000100)
#define HCJSON_TYPE_NUMBER_FLOAT (0x00000200)

#define HCJSON_TOKEN_BUFF_SIZE (64)
#define HCJSON_TOKEN_BUFF_MULTIPLIER (2)

#include <stdint.h>


typedef struct hcjson {
    struct hcjson *prev;
    struct hcjson *next;
    int32_t type;
    char* name;
    union {
        struct hcjson *childv;
        char *stringv;
        int64_t intv;
        uint64_t uintv;
        double floatv;
    } value;
} hcjson;


#ifdef __cplusplus
extern "C" {
#endif

hcjson *hcjson_parse(const char* json);

#ifdef __cplusplus
}
#endif

#define HCJSON_IMPL /* NOTE: temp */
#ifdef HCJSON_IMPL

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef enum hcjson_token {
    HCJSON_TOKEN_INVALID,
    HCJSON_TOKEN_LBRACE,
    HCJSON_TOKEN_RBRACE,
    HCJSON_TOKEN_LBRACKET,
    HCJSON_TOKEN_RBRACKET,
    HCJSON_TOKEN_COMMA,
    HCJSON_TOKEN_COLON,
    HCJSON_TOKEN_NULL,
    HCJSON_TOKEN_TRUE,
    HCJSON_TOKEN_FALSE,
    HCJSON_TOKEN_STRING,
    HCJSON_TOKEN_NUMBER
} hcjson_token;

typedef struct hcjson_token_str {
    hcjson_token token;
    const char *str;
    size_t len;
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
    return HCJSON_MALLOC(size);
}

void hcjson__free(void* ptr) {
    HCJSON_FREE(ptr);
}

char *hcjson__strdup(const char* str) {
    size_t length;
    char *result;
    HCJSON_ASSERT(str, "str cannot be null");
    length = strlen(str) + 1;
    result = (char*) hcjson__malloc(length);
    if (!result) { return NULL; }
    memcpy(result, str, length);
    return result;
}

#endif /* HCJSON_IMPL */

#endif
