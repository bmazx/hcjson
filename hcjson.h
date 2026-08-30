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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


typedef int32_t hcjson_flag;
typedef int32_t hcjson_result;
typedef int32_t hcjson_type;

typedef struct hcjson {
    struct hcjson *prev;
    struct hcjson *next;
    hcjson_type type;
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

bool hcjson_get_bool(hcjson *json);
const char* hcjson_get_str(hcjson *json);
double hcjson_get_num(hcjson *json);

size_t hcjson_list_size(hcjson *json);

char *hcjson_to_string(hcjson *json);
char *hcjson_to_string_format(hcjson *json, hcjson_flag flags);

hcjson *hcjson_parse(const char* json);


#ifdef __cplusplus
}
#endif


#ifdef HCJSON_IMPL

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
} hcjson_parser;

static void *hcjson__malloc(size_t size);
static void hcjson__free(void* ptr);
static char *hcjson__strdup(const char* str);

static void *hcjson__malloc(size_t size) {
    if (!size) { return NULL; }
    return HCJSON_MALLOC(size);
}

static void hcjson__free(void* ptr) {
    if (!ptr) { return; }
    HCJSON_FREE(ptr);
}

static char *hcjson__strdup(const char* str) {
    HCJSON_ASSERT(str, "str cannot be null");
    size_t length = strlen(str) + 1;
    char *result = (char*) hcjson__malloc(length);
    if (!result) { return NULL; }
    memcpy(result, str, length);
    return result;
}

static bool             hcjson__is_list_json(const hcjson *json);
static bool             hcjson__is_num_int(double num);
static bool             hcjson__whitespace(char c);
static bool             hcjson__is_digit(char c);
static hcjson_result    hcjson__add_item_to_list_json(hcjson *json, const char *key, hcjson *item);
static hcjson_result    hcjson__remove_item_in_obj(hcjson *obj, const char *key, bool destroy, hcjson **ret);
static hcjson_result    hcjson__remove_item_in_arr(hcjson *arr, uint32_t index, bool destroy, hcjson **ret);
static void             hcjson__to_string_rec(hcjson_token_buffer *tbuff, const hcjson *json, uint32_t indent);
static void             hcjson__token_buffer_expand(hcjson_token_buffer *tbuff);
static void             hcjson__token_buffer_add(hcjson_token_buffer *tbuff, const hcjson_token_str tstr);
static void             hcjson__lex_json(hcjson_parser *ps);
static hcjson_token_str hcjson__lex_value_str(hcjson_parser *ps, size_t *pos, size_t len);
static hcjson_token_str hcjson__lex_value(hcjson_parser *ps, size_t *pos, size_t len);
static hcjson*          hcjson__parse_object(const hcjson_parser *ps, size_t *tbuff_index);
static hcjson*          hcjson__parse_array(const hcjson_parser *ps, size_t *tbuff_index);
static hcjson*          hcjson__parse_item(const hcjson_parser *ps, size_t *tbuff_index);

static bool hcjson__is_list_json(const hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & (HCJSON_TYPE_OBJECT | HCJSON_TYPE_ARRAY);
}

static bool hcjson__is_num_int(double num) {
    return num == (int64_t)num;
}

static bool hcjson__whitespace(char c) {
    return (c == ' ' || c == '\n' || c == '\t' || c == '\r');
}

static bool hcjson__is_digit(char c) {
    return (c >= '0' && c <= '9');
}

static hcjson_result hcjson__add_item_to_list_json(hcjson *json, const char *key, hcjson *item) {
    HCJSON_ASSERT(json && key && item, "obj, key, and item cannot be null");

    if (!hcjson__is_list_json(json)) {
        return HCJSON_ERROR_INVALID_TYPE;
    }
    if (item->type & HCJSON_TAG_TRANSFERRED) {
        return HCJSON_ERROR_ITEM_ALREADY_TRANSFERRED;
    }

    item->type |= HCJSON_TAG_TRANSFERRED;
    item->next = NULL;
    item->prev = NULL;

    if (hcjson_is_object(json)) {
        item->key = hcjson__strdup(key);
    }

    if (!json->value.child) {
        json->value.child = item;
        json->m_tail = item;
        return HCJSON_SUCCESS;
    }

    json->m_tail->next = item;
    item->prev = json->m_tail;
    json->m_tail = item;

    return HCJSON_SUCCESS;
}

static hcjson_result hcjson__remove_item_in_obj(hcjson *obj, const char *key, bool destroy, hcjson **ret) {
    HCJSON_ASSERT(obj && key, "obj and key cannot be null");

    if (!hcjson_is_object(obj)) {
        return HCJSON_ERROR_INVALID_TYPE;
    }

    for (hcjson *ijs = obj->value.child; ijs; ijs = ijs->next) {
        if (strcmp(ijs->key, key) == 0) {
            hcjson *ijs_next = ijs->next;
            hcjson *ijs_prev = ijs->prev;

            if (ijs_next) { ijs_next->prev = ijs_prev; }
            if (ijs_prev) { ijs_prev->next = ijs_next; }

            if (ijs == obj->m_tail) {
                if (ijs_prev) { obj->m_tail = ijs_prev; }
                else { obj->m_tail = NULL; }
            }

            if (destroy) {
                hcjson_destroy(ijs);
            }

            ijs->type &= ~HCJSON_TAG_TRANSFERRED;
            if (ret) {
                *ret = ijs;
            }

            return HCJSON_SUCCESS;
        }
    }

    return HCJSON_ERROR_TASK_FAILED;
}

static hcjson_result hcjson__remove_item_in_arr(hcjson *arr, uint32_t index, bool destroy, hcjson **ret) {
    HCJSON_ASSERT(arr, "arr cannot be null");

    if (!hcjson_is_array(arr)) {
        return HCJSON_ERROR_INVALID_TYPE;
    }

    uint32_t i = 0;
    for (hcjson *ijs = arr->value.child; ijs; ijs = ijs->next) {
        if (index == i) {
            hcjson *ijs_next = ijs->next;
            hcjson *ijs_prev = ijs->prev;

            if (ijs_next) { ijs_next->prev = ijs_prev; }
            if (ijs_prev) { ijs_prev->next = ijs_next; }

            if (ijs == arr->m_tail) {
                if (ijs_prev) { arr->m_tail = ijs_prev; }
                else { arr->m_tail = NULL; }
            }

            if (destroy) {
                hcjson_destroy(ijs);
            }

            ijs->type &= ~HCJSON_TAG_TRANSFERRED;
            if (ret) {
                *ret = ijs;
            }

            return HCJSON_SUCCESS;
        }
        i++;
        if (i > index) {
            break;
        }
    }

    return HCJSON_ERROR_TASK_FAILED;
}

static void hcjson__to_string_rec(hcjson_token_buffer *tbuff, const hcjson *json, uint32_t indent) {
    HCJSON_ASSERT(tbuff && json, "tbuff and json cannot be null");

    if (indent) {
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_SPACING, "", indent});
    }

    if (json->key) {
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_STRING, json->key, strlen(json->key)});
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_COLON, ":", 1});
    }

    if (hcjson_is_object(json)) {
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_LBRACE, "{", 1});
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_NEWLINE, "\n", 1});
        for (hcjson *ijs = json->value.child; ijs; ijs = ijs->next) {
            hcjson__to_string_rec(tbuff, ijs, indent + HCJSON_INDENT_LENGTH);
            if (ijs->next) {
                hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_COMMA, ",", 1});
                hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_NEWLINE, "\n", 1});
            }
        }
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_NEWLINE, "\n", 1});
        if (indent) {
            hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_SPACING, "", indent});
        }
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_RBRACE, "}", 1});
    }
    else if (hcjson_is_array(json)) {
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_LBRACKET, "[", 1});
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_NEWLINE, "\n", 1});
        for (hcjson *ijs = json->value.child; ijs; ijs = ijs->next) {
            hcjson__to_string_rec(tbuff, ijs, indent + HCJSON_INDENT_LENGTH);
            if (ijs->next) {
                hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_COMMA, ",", 1});
                hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_NEWLINE, "\n", 1});
            }
        }
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_NEWLINE, "\n", 1});
        if (indent) {
            hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_SPACING, "", indent});
        }
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_RBRACKET, "]", 1});
    }

    if (hcjson_is_true(json)) {
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_TRUE, "true", 4});
    }
    else if (hcjson_is_false(json)) {
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_FALSE, "false", 5});
    }
    else if (hcjson_is_null(json)) {
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_NULL, "null", 4});
    }
    else if (hcjson_is_string(json)) {
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_STRING, json->value.str, strlen(json->value.str)});
    }
    else if (hcjson_is_number(json)) {
        hcjson__token_buffer_add(tbuff, (hcjson_token_str){HCJSON_TOKEN_NUMBER, NULL, 0, json->value.num});
    }

}

static void hcjson__token_buffer_expand(hcjson_token_buffer *tbuff) {
    HCJSON_ASSERT(tbuff, "tbuff cannot be null");
    tbuff->capacity = tbuff->capacity ? (tbuff->capacity * HCJSON_TOKEN_BUFF_MULTIPLIER) : HCJSON_TOKEN_BUFF_SIZE;
    hcjson_token_str *temp = (hcjson_token_str*) hcjson__malloc(tbuff->capacity * sizeof(hcjson_token_str));
    memcpy(temp, tbuff->tokens, tbuff->size * sizeof(hcjson_token_str));
    hcjson__free(tbuff->tokens);
    tbuff->tokens = temp;
}

static void hcjson__token_buffer_add(hcjson_token_buffer *tbuff, const hcjson_token_str tstr) {
    HCJSON_ASSERT(tbuff, "tbuff cannot be null");
    if (tbuff->size >= tbuff->capacity) {
        hcjson__token_buffer_expand(tbuff);
    }
    tbuff->tokens[tbuff->size++] = tstr;
}

static void hcjson__lex_json(hcjson_parser *ps) {
    HCJSON_ASSERT(ps, "ps cannot be null");

    size_t pos = 0;
    size_t len = strlen(ps->json);

    while (pos < len) {
        char c = ps->json[pos];

        if (hcjson__whitespace(c)) {
            pos++;
            continue;
        }

        switch (c) {
            case '{':
                hcjson__token_buffer_add(&ps->tbuff, (hcjson_token_str){ HCJSON_TOKEN_LBRACE, "{", 1 });
                pos++;
                break;
            case '}':
                hcjson__token_buffer_add(&ps->tbuff, (hcjson_token_str){ HCJSON_TOKEN_RBRACE, "}", 1 });
                pos++;
                break;
            case '[':
                hcjson__token_buffer_add(&ps->tbuff, (hcjson_token_str){ HCJSON_TOKEN_LBRACKET, "[", 1 });
                pos++;
                break;
            case ']':
                hcjson__token_buffer_add(&ps->tbuff, (hcjson_token_str){ HCJSON_TOKEN_RBRACKET, "]", 1 });
                pos++;
                break;
            case ',':
                hcjson__token_buffer_add(&ps->tbuff, (hcjson_token_str){ HCJSON_TOKEN_COMMA, ",", 1 });
                pos++;
                break;
            case ':':
                hcjson__token_buffer_add(&ps->tbuff, (hcjson_token_str){ HCJSON_TOKEN_COLON, ":", 1 });
                pos++;
                break;
            case '\"':
                hcjson__token_buffer_add(&ps->tbuff, hcjson__lex_value_str(ps, &pos, len));
                break;
            default:
                hcjson__token_buffer_add(&ps->tbuff, hcjson__lex_value(ps, &pos, len));
                break;
        }
    }
}

static hcjson_token_str hcjson__lex_value_str(hcjson_parser *ps, size_t *pos, size_t len) {
    HCJSON_ASSERT(ps && pos, "ps and pos cannot be null");

    hcjson_token_str tstr = {
        .token = HCJSON_TOKEN_STRING,
        .str = &ps->json[++(*pos)],
        .len = 0,
    };

    while (ps->json[*pos] != '\"' && *pos < len) {
        (*pos)++;
        tstr.len++;
    }

    /* swallow end quote */
    (*pos)++;

    return tstr;
}

static hcjson_token_str hcjson__lex_value(hcjson_parser *ps, size_t *pos, size_t len) {
    HCJSON_ASSERT(ps && pos, "ps and pos cannot be null");

    hcjson_token_str tstr = {
        .token = HCJSON_TOKEN_INVALID,
        .str = &ps->json[*pos],
        .len = 0,
    };

    while (ps->json[*pos] != ',' && ps->json[*pos] != '}' && ps->json[*pos] != ']' &&
           !hcjson__whitespace(ps->json[*pos]) &&
           *pos < len) {
        (*pos)++;
        tstr.len++;
    }

    if (tstr.len == 4 && strncmp(tstr.str, "null", tstr.len) == 0) {
        tstr.token = HCJSON_TOKEN_NULL;
    }
    else if (tstr.len == 4 && strncmp(tstr.str, "true", tstr.len) == 0) {
        tstr.token = HCJSON_TOKEN_TRUE;
    }
    else if (tstr.len == 5 && strncmp(tstr.str, "false", tstr.len) == 0) {
        tstr.token = HCJSON_TOKEN_FALSE;
    }
    else if (tstr.len > 0 && (tstr.str[0] == '-' || (tstr.str[0] >= '0' && tstr.str[0] <= '9'))) {
        tstr.token = HCJSON_TOKEN_NUMBER;
        char *end;
        char buff[HCJSON_NUMBER_BUFF_SIZE];
        if (len > HCJSON_NUMBER_BUFF_SIZE - 1) {
            memcpy(buff, tstr.str, HCJSON_NUMBER_BUFF_SIZE);
            buff[HCJSON_NUMBER_BUFF_SIZE - 1] = '\0';
        }
        else {
            memcpy(buff, tstr.str, tstr.len);
            buff[tstr.len] = '\0';
        }
        tstr.num = strtod(buff, &end);
    }

    return tstr;
}

static hcjson *hcjson__parse_object(const hcjson_parser *ps, size_t *tbuff_index) {
    HCJSON_ASSERT(ps && tbuff_index, "ps and tbuff_index cannot be null");

    if (ps->tbuff.tokens[*tbuff_index].token != HCJSON_TOKEN_LBRACE) {
        return NULL;
    }
    if (*tbuff_index >= ps->tbuff.size) {
        return NULL;
    }

    hcjson *json = NULL;
    char *key_buff = NULL;

    json = hcjson_create_object();
    if (!json) {
        goto fail_cleanup;
    }

    (*tbuff_index)++;

    while (ps->tbuff.tokens[*tbuff_index].token != HCJSON_TOKEN_RBRACE && *tbuff_index < ps->tbuff.size) {
        // key string
        if (*tbuff_index >= ps->tbuff.size || ps->tbuff.tokens[*tbuff_index].token != HCJSON_TOKEN_STRING) {
            goto fail_cleanup;
        }

        size_t str_len = ps->tbuff.tokens[*tbuff_index].len;
        key_buff = (char*) hcjson__malloc(str_len * sizeof(char));
        if (!key_buff) {
            goto fail_cleanup;
        }
        memcpy(key_buff, ps->tbuff.tokens[*tbuff_index].str, str_len * sizeof(char));
        key_buff[str_len] = '\0';

        // skip colon
        (*tbuff_index)++;
        if (*tbuff_index >= ps->tbuff.size || ps->tbuff.tokens[*tbuff_index].token != HCJSON_TOKEN_COLON) {
            goto fail_cleanup;
        }

        // item
        (*tbuff_index)++;
        if (*tbuff_index >= ps->tbuff.size) {
            goto fail_cleanup;
        }

        hcjson *item = hcjson__parse_item(ps, tbuff_index);
        if (!item) {
            goto fail_cleanup;
        }

        if (hcjson_add_item_to_object(json, key_buff, item) != HCJSON_SUCCESS) {
            goto fail_cleanup;
        }

        // do this to prevent skipping comma two times
        if (!(hcjson_is_object(item) || hcjson_is_array(item))) {
            // skip comma if there
            (*tbuff_index)++;
            if (*tbuff_index >= ps->tbuff.size) {
                goto fail_cleanup;
            }
            if (ps->tbuff.tokens[*tbuff_index].token == HCJSON_TOKEN_COMMA) {
                (*tbuff_index)++;
            }
        }

        hcjson__free(key_buff);
        key_buff = NULL;
    }

    if (*tbuff_index < ps->tbuff.size && ps->tbuff.tokens[*tbuff_index].token != HCJSON_TOKEN_RBRACE) {
        goto fail_cleanup;
    }
    (*tbuff_index)++;

    if (*tbuff_index < ps->tbuff.size && ps->tbuff.tokens[*tbuff_index].token == HCJSON_TOKEN_COMMA) {
        (*tbuff_index)++;
    }

    hcjson__free(key_buff);
    return json;

fail_cleanup:
    hcjson_destroy(json);
    hcjson__free(key_buff);
    return NULL;
}

static hcjson *hcjson__parse_array(const hcjson_parser *ps, size_t *tbuff_index) {
    HCJSON_ASSERT(ps && tbuff_index, "ps and tbuff_index cannot be null");

    if (ps->tbuff.tokens[*tbuff_index].token != HCJSON_TOKEN_LBRACKET) {
        return NULL;
    }
    if (*tbuff_index >= ps->tbuff.size) {
        return NULL;
    }

    hcjson *json = NULL;

    json = hcjson_create_array();
    if (!json) {
        goto fail_cleanup;
    }

    (*tbuff_index)++;

    while (ps->tbuff.tokens[*tbuff_index].token != HCJSON_TOKEN_RBRACKET && *tbuff_index < ps->tbuff.size) {
        // item
        if (*tbuff_index >= ps->tbuff.size) {
            goto fail_cleanup;
        }

        hcjson *item = hcjson__parse_item(ps, tbuff_index);
        if (!item) {
            goto fail_cleanup;
        }

        if (hcjson_add_item_to_array(json, item) != HCJSON_SUCCESS) {
            goto fail_cleanup;
        }

        // skip comma if there
        // do this to prevent skipping comma two times
        if (!(hcjson_is_object(item) || hcjson_is_array(item))) {
            (*tbuff_index)++;
            if (*tbuff_index >= ps->tbuff.size) {
                goto fail_cleanup;
            }
            if (ps->tbuff.tokens[*tbuff_index].token == HCJSON_TOKEN_COMMA) {
                (*tbuff_index)++;
            }
        }
    }

    if (*tbuff_index < ps->tbuff.size && ps->tbuff.tokens[*tbuff_index].token != HCJSON_TOKEN_RBRACKET) {
        goto fail_cleanup;
    }
    (*tbuff_index)++;

    if (*tbuff_index < ps->tbuff.size && ps->tbuff.tokens[*tbuff_index].token == HCJSON_TOKEN_COMMA) {
        (*tbuff_index)++;
    }

    return json;

fail_cleanup:
    hcjson_destroy(json);
    return NULL;
}

static hcjson *hcjson__parse_item(const hcjson_parser *ps, size_t *tbuff_index) {
    HCJSON_ASSERT(ps && tbuff_index, "ps and tbuff_index cannot be null");

    if (*tbuff_index >= ps->tbuff.size) {
        return NULL;
    }

    const hcjson_token_str *tstr = &ps->tbuff.tokens[*tbuff_index];

    switch (tstr->token) {
        case HCJSON_TOKEN_LBRACE: {
            return hcjson__parse_object(ps, tbuff_index);
        }
        case HCJSON_TOKEN_LBRACKET: {
            return hcjson__parse_array(ps, tbuff_index);
        }
        case HCJSON_TOKEN_TRUE: {
            return hcjson_create_true();
        }
        case HCJSON_TOKEN_FALSE: {
            return hcjson_create_false();
        }
        case HCJSON_TOKEN_NULL: {
            return hcjson_create_null();
        }
        case HCJSON_TOKEN_STRING: {
            char *buff = (char*) hcjson__malloc(tstr->len * sizeof(char) + 1);
            if (!buff) {
                return NULL;
            }
            memcpy(buff, tstr->str, tstr->len * sizeof(char));
            buff[tstr->len] = '\0';
            hcjson *json = hcjson_create_string(buff);
            hcjson__free(buff);
            return json;
        }
        case HCJSON_TOKEN_NUMBER: {
            return hcjson_create_number(tstr->num);
        }
        default: {
            break;
        }
    }

    return NULL;
}

hcjson *hcjson_create_object(void) {
    hcjson *json_obj = (hcjson*) hcjson__malloc(sizeof(hcjson));
    if (!json_obj) { return NULL; }
    memset(json_obj, 0, sizeof(hcjson));
    json_obj->type = HCJSON_TYPE_OBJECT;
    return json_obj;
}

hcjson *hcjson_create_array(void) {
    hcjson *json_obj = (hcjson*) hcjson__malloc(sizeof(hcjson));
    if (!json_obj) { return NULL; }
    memset(json_obj, 0, sizeof(hcjson));
    json_obj->type = HCJSON_TYPE_ARRAY;
    return json_obj;
}

hcjson *hcjson_create_true(void) {
    hcjson *json_obj = (hcjson*) hcjson__malloc(sizeof(hcjson));
    if (!json_obj) { return NULL; }
    memset(json_obj, 0, sizeof(hcjson));
    json_obj->type = HCJSON_TYPE_TRUE;
    return json_obj;
}

hcjson *hcjson_create_false(void) {
    hcjson *json_obj = (hcjson*) hcjson__malloc(sizeof(hcjson));
    if (!json_obj) { return NULL; }
    memset(json_obj, 0, sizeof(hcjson));
    json_obj->type = HCJSON_TYPE_FALSE;
    return json_obj;
}

hcjson *hcjson_create_null(void) {
    hcjson *json_obj = (hcjson*) hcjson__malloc(sizeof(hcjson));
    if (!json_obj) { return NULL; }
    memset(json_obj, 0, sizeof(hcjson));
    json_obj->type = HCJSON_TYPE_NULL;
    return json_obj;
}

hcjson *hcjson_create_string(const char *str) {
    HCJSON_ASSERT(str, "str cannot be null");
    hcjson *json_obj = (hcjson*) hcjson__malloc(sizeof(hcjson));
    if (!json_obj) { return NULL; }
    memset(json_obj, 0, sizeof(hcjson));
    json_obj->type = HCJSON_TYPE_STRING;
    json_obj->value.str = hcjson__strdup(str);
    return json_obj;
}

hcjson *hcjson_create_number(double num) {
    hcjson *json_obj = (hcjson*) hcjson__malloc(sizeof(hcjson));
    if (!json_obj) { return NULL; }
    memset(json_obj, 0, sizeof(hcjson));
    json_obj->type = HCJSON_TYPE_NUMBER;
    json_obj->value.num = num;
    return json_obj;
}

void hcjson_destroy(hcjson *json) {
    if (!json) { return; }

    if (json->type & (HCJSON_TYPE_OBJECT | HCJSON_TYPE_ARRAY)) {
        hcjson *ijs = json->value.child;
        while (ijs) {
            hcjson *temp = ijs;
            ijs = ijs->next;
            hcjson_destroy(temp);
        }
    }
    else if (json->type & HCJSON_TYPE_STRING) {
        if (json->value.str) {
            hcjson__free(json->value.str);
        }
    }

    if (json->key) { hcjson__free(json->key); }
    hcjson__free(json);
}

bool hcjson_is_object(const hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_OBJECT;
}

bool hcjson_is_array(const hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_ARRAY;
}

bool hcjson_is_true(const hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_TRUE;
}

bool hcjson_is_false(const hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_FALSE;
}

bool hcjson_is_null(const hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_NULL;
}

bool hcjson_is_string(const hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_STRING;
}

bool hcjson_is_number(const hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_NUMBER;
}

bool hcjson_is_number_int(const hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    if (!hcjson_is_number(json)) { return false; }
    double val = json->value.num;
    return hcjson__is_num_int(val);
}

hcjson_result hcjson_add_item_to_object(hcjson *obj, const char *key, hcjson *item) {
    HCJSON_ASSERT(obj && key && item, "obj, key, and item cannot be null");
    return hcjson__add_item_to_list_json(obj, key, item);
}

hcjson_result hcjson_copy_item_to_object(hcjson *obj, const char *key, const hcjson *item) {
    HCJSON_ASSERT(obj && key && item, "obj, key, and item cannot be null");
    if (!hcjson_is_object(obj)) { return HCJSON_ERROR_INVALID_TYPE; }
    hcjson *copy = (hcjson*) hcjson__malloc(sizeof(hcjson));
    if (!copy) { return HCJSON_ERROR_MALLOC_FAILURE; }
    memcpy(copy, item, sizeof(hcjson));
    copy->type &= ~HCJSON_TAG_TRANSFERRED;
    return hcjson_add_item_to_object(obj, key, copy);
}

hcjson_result hcjson_destroy_item_in_object(hcjson *obj, const char *key) {
    HCJSON_ASSERT(obj && key, "obj and key cannot be null");
    return hcjson__remove_item_in_obj(obj, key, true, NULL);
}

hcjson *hcjson_remove_item_in_object(hcjson *obj, const char *key) {
    HCJSON_ASSERT(obj && key, "obj and key cannot be null");
    hcjson *ret = NULL;
    if (hcjson__remove_item_in_obj(obj, key, false, &ret) != HCJSON_SUCCESS) {
        return NULL;
    }
    return ret;
}

hcjson *hcjson_get_item_in_object(hcjson *obj, const char *key) {
    HCJSON_ASSERT(obj && key, "obj and key cannot be null");

    if (!hcjson_is_object(obj)) {
        return NULL;
    }

    for (hcjson *ijs = obj->value.child; ijs; ijs = ijs->next) {
        if (strcmp(ijs->key, key) == 0) {
            return ijs;
        }
    }

    return NULL;
}

hcjson_result hcjson_add_item_to_array(hcjson *arr, hcjson *item) {
    HCJSON_ASSERT(arr && item, "arr and item cannot be null");
    return hcjson__add_item_to_list_json(arr, "", item);
}

hcjson_result hcjson_copy_item_to_array(hcjson *arr, const hcjson *item) {
    HCJSON_ASSERT(arr && item, "arr and item cannot be null");
    if (!hcjson_is_array(arr)) { return HCJSON_ERROR_INVALID_TYPE; }
    hcjson *copy = (hcjson*) hcjson__malloc(sizeof(hcjson));
    if (!copy) { return HCJSON_ERROR_MALLOC_FAILURE; }
    memcpy(copy, item, sizeof(hcjson));
    copy->type &= ~HCJSON_TAG_TRANSFERRED;
    return hcjson_add_item_to_array(arr, copy);
}

hcjson_result hcjson_destroy_item_in_array(hcjson *arr, uint32_t index) {
    HCJSON_ASSERT(arr, "arr cannot be null");
    return hcjson__remove_item_in_arr(arr, index, true, NULL);
}

hcjson *hcjson_remove_item_in_array(hcjson *arr, uint32_t index) {
    HCJSON_ASSERT(arr, "arr cannot be null");
    hcjson *ret = NULL;
    if (hcjson__remove_item_in_arr(arr, index, false, &ret) != HCJSON_SUCCESS) {
        return NULL;
    }
    return ret;
}

hcjson *hcjson_get_item_in_array(hcjson *arr, uint32_t index) {
    HCJSON_ASSERT(arr, "arr cannot be null");

    if (!hcjson_is_array(arr)) {
        return NULL;
    }

    uint32_t i = 0;
    for (hcjson *ijs = arr->value.child; ijs; ijs = ijs->next) {
        if (index == i) {
            return ijs;
        }
        i++;
        if (i > index) {
            break;
        }
    }

    return NULL;
}

bool hcjson_get_bool(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    HCJSON_ASSERT(hcjson_is_true(json) || hcjson_is_false(json), "json needs to be true or false item");
    return hcjson_is_true(json) ? true : false;
}

const char* hcjson_get_str(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    HCJSON_ASSERT(hcjson_is_string(json), "json needs to be string item");
    return json->value.str;
}

double hcjson_get_num(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    HCJSON_ASSERT(hcjson_is_number(json), "json needs to be number item");
    return json->value.num;
}

size_t hcjson_list_size(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");

    if (!hcjson__is_list_json(json)) {
        return 0;
    }

    size_t i = 0;
    for (hcjson *ijs = json->value.child; ijs; ijs = ijs->next) {
        i++;
    }

    return i;
}

char *hcjson_to_string(hcjson *json) {
    return hcjson_to_string_format(json, HCJSON_TOSTR_FLAG_WHITESPACE | HCJSON_TOSTR_FLAG_CAST_NUMBER_TYPES);
}

char *hcjson_to_string_format(hcjson *json, hcjson_flag flags) {
    HCJSON_ASSERT(json, "json cannot be null");

    bool whitespace = flags & HCJSON_TOSTR_FLAG_WHITESPACE;
    bool use_inline = flags & HCJSON_TOSTR_FLAG_INLINE;
    bool use_tabs = flags & HCJSON_TOSTR_FLAG_USE_TABS;
    bool cast_num = flags & HCJSON_TOSTR_FLAG_CAST_NUMBER_TYPES;

    hcjson_token_buffer tbuff = {0};
    char *json_str = NULL;
    size_t pen = 0;

    hcjson__to_string_rec(&tbuff, json, 0);

    // calc string length
    size_t len = 0;
    for (size_t i = 0; i < tbuff.size; i++) {
        if (tbuff.tokens[i].token == HCJSON_TOKEN_NUMBER) {
            int ret = 0;
            double vald = tbuff.tokens[i].num;
            if (cast_num && hcjson__is_num_int(vald)) {
                ret = snprintf(NULL, 0, "%" PRId64, (int64_t)vald);
            }
            else {
                ret = snprintf(NULL, 0, "%g", vald);
            }
            if (ret < 0) { goto cleanup; }
            len += ret;
        }
        if (tbuff.tokens[i].token == HCJSON_TOKEN_STRING) {
            len += 2; // for quotes '\"'
        }
        else if (!use_inline && tbuff.tokens[i].token == HCJSON_TOKEN_NEWLINE) {
            len++; // new line '\n'
        }
        else if (whitespace && tbuff.tokens[i].token == HCJSON_TOKEN_COLON) {
            len += 1; // add spacing in colon ": "
        }
        else if (tbuff.tokens[i].token == HCJSON_TOKEN_SPACING) {
            if (use_inline) {
                continue;
            }
            else if (use_tabs) {
                len += tbuff.tokens[i].len / HCJSON_INDENT_LENGTH;
                continue;
            }
        }

        len += tbuff.tokens[i].len;
    }
    len++; // null terminator

    // fill string
    json_str = (char*) hcjson__malloc(len * sizeof(char));
    if (!json_str) { goto cleanup; }

    for (size_t i = 0; i < tbuff.size; i++) {
        switch (tbuff.tokens[i].token) {
            case HCJSON_TOKEN_SPACING: {
                if (use_inline || !whitespace) {
                    break;
                }
                if (use_tabs) {
                    uint32_t indent = tbuff.tokens[i].len / HCJSON_INDENT_LENGTH;
                    for (uint32_t j = 0; j < indent; j++) {
                        json_str[pen++] = '\t';
                    }
                }
                else {
                    for (uint32_t j = 0; j < tbuff.tokens[i].len; j++) {
                        json_str[pen++] = ' ';
                    }
                }
                break;
            }
            case HCJSON_TOKEN_NEWLINE: {
                if (!use_inline) {
                    json_str[pen++] = '\n';
                }
                break;
            }
            case HCJSON_TOKEN_LBRACE: {
                json_str[pen++] = '{';
                break;
            }
            case HCJSON_TOKEN_RBRACE: {
                json_str[pen++] = '}';
                break;
            }
            case HCJSON_TOKEN_LBRACKET: {
                json_str[pen++] = '[';
                break;
            }
            case HCJSON_TOKEN_RBRACKET: {
                json_str[pen++] = ']';
                break;
            }
            case HCJSON_TOKEN_COMMA: {
                json_str[pen++] = ',';
                break;
            }
            case HCJSON_TOKEN_COLON: {
                json_str[pen++] = ':';
                if (whitespace) {
                    json_str[pen++] = ' ';
                }
                break;
            }
            case HCJSON_TOKEN_TRUE: {
                json_str[pen++] = 't';
                json_str[pen++] = 'r';
                json_str[pen++] = 'u';
                json_str[pen++] = 'e';
                break;
            }
            case HCJSON_TOKEN_FALSE: {
                json_str[pen++] = 'f';
                json_str[pen++] = 'a';
                json_str[pen++] = 'l';
                json_str[pen++] = 's';
                json_str[pen++] = 'e';
                break;
            }
            case HCJSON_TOKEN_NULL: {
                json_str[pen++] = 'n';
                json_str[pen++] = 'u';
                json_str[pen++] = 'l';
                json_str[pen++] = 'l';
                break;
            }
            case HCJSON_TOKEN_STRING: {
                json_str[pen++] = '\"';
                for (uint32_t j = 0; j < tbuff.tokens[i].len; j++) {
                    json_str[pen++] = tbuff.tokens[i].str[j];
                }
                json_str[pen++] = '\"';
                break;
            }
            case HCJSON_TOKEN_NUMBER: {
                double vald = tbuff.tokens[i].num;
                char buff[HCJSON_NUMBER_BUFF_SIZE];
                if (cast_num && hcjson__is_num_int(vald)) {
                    snprintf(buff, HCJSON_NUMBER_BUFF_SIZE, "%" PRId64, (int64_t)vald);
                }
                else {
                    snprintf(buff, HCJSON_NUMBER_BUFF_SIZE, "%g", vald);
                }
                size_t num_len = strlen(buff);
                for (uint32_t j = 0; j < num_len; j++) {
                    json_str[pen++] = buff[j];
                }
                break;
            }
            default:
                break;
        }
    }

    json_str[pen] = '\0';

cleanup:
    hcjson__free(tbuff.tokens);
    return json_str;
}

hcjson *hcjson_parse(const char* json) {
    HCJSON_ASSERT(json, "json cannot be null");

    hcjson_parser ps = {0};
    ps.json = json;

    // lex json string
    hcjson__lex_json(&ps);

    // parse json tokens
    size_t tbuff_index = 0;
    hcjson *json_item = hcjson__parse_item(&ps, &tbuff_index);

    hcjson__free(ps.tbuff.tokens);

    return json_item;
}

#endif /* HCJSON_IMPL */

#endif
