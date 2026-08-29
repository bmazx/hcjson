#define HCJSON_MALLOC(sz) custom_malloc(sz)
#define HCJSON_FREE(ptr) custom_free(ptr)

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static size_t s_memAllocCount = 0;

void *custom_malloc(size_t sz) {
    if (!sz) { return NULL; }
    s_memAllocCount++;
    return malloc(sz);
}

void custom_free(void *ptr) {
    if (!ptr) { return; }
    s_memAllocCount--;
    free(ptr);
}


#include "hcjson.h"


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
static hcjson*          hcjson__parse_object(hcjson_parser *ps);

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

    while (ps->json[*pos] != ',' && !hcjson__whitespace(ps->json[*pos]) && *pos < len) {
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
        if (len > HCJSON_NUMBER_BUFF_SIZE) {
            memcpy(buff, tstr.str, HCJSON_NUMBER_BUFF_SIZE);
            buff[HCJSON_NUMBER_BUFF_SIZE - 1] = '\0';
        }
        else {
            memcpy(buff, tstr.str, tstr.len);
            buff[tstr.len - 1] = '\0';
        }
        tstr.num = strtod(buff, &end);
    }

    return tstr;
}

static hcjson *hcjson__parse_object(hcjson_parser *ps) {
    HCJSON_ASSERT(ps, "ps cannot be null");

    hcjson *json_obj = NULL, *curr_obj = NULL;

    json_obj = (hcjson*) hcjson__malloc(sizeof(hcjson));
    if (!json_obj) {
        goto fail_cleanup;
    }
    curr_obj = json_obj;

    if (ps->tbuff.tokens[ps->tbuff_index].token != HCJSON_TOKEN_LBRACE) {
        goto fail_cleanup;
    }

    ps->tbuff_index++;
    while (ps->tbuff.tokens[ps->tbuff_index].token != HCJSON_TOKEN_RBRACE) {
        if (ps->tbuff.tokens[ps->tbuff_index].token != HCJSON_TOKEN_STRING) {
            goto fail_cleanup;
        }
    }

    return json_obj;

fail_cleanup:
    hcjson__free(json_obj);
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
    hcjson *copy = hcjson__malloc(sizeof(hcjson));
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
    hcjson *copy = hcjson__malloc(sizeof(hcjson));
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

uint32_t hcjson_list_size(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");

    if (!hcjson__is_list_json(json)) {
        return 0;
    }

    uint32_t i = 0;
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

    hcjson__to_string_rec(&tbuff, json, 0);

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

    char *json_str = NULL;
    size_t pen = 0;

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
    hcjson__lex_json(&ps);

    for (int i = 0; i < ps.tbuff.size; i++) {
        printf("%.*s\n", (int)ps.tbuff.tokens[i].len, ps.tbuff.tokens[i].str);
    }

    hcjson__free(ps.tbuff.tokens);

    return NULL;
}

char* loadFile(const char* filepath) {
    char* file;
    FILE* fileptr;
    size_t filesize;

    fileptr = fopen(filepath, "r");
    if (!fileptr)
        return NULL;

    fseek(fileptr, 0, SEEK_END);
    filesize = ftell(fileptr);
    fseek(fileptr, 0, SEEK_SET);

    file = malloc(filesize * sizeof(uint8_t));
    if (!file) { return NULL; }

    fread(file, sizeof(uint8_t), filesize, fileptr);
    fclose(fileptr);

    return file;
}

int main(int argc, char **argv) {
    printf("s_memAllocCount: %zu\n", s_memAllocCount);

    hcjson *json = hcjson_create_object();

    hcjson *str = hcjson_create_string("hello hcjson");
    hcjson *num = hcjson_create_number(67);
    hcjson *flo = hcjson_create_number(3.1514);
    hcjson *boo = hcjson_create_true();

    hcjson *obj = hcjson_create_object();
    hcjson *num2 = hcjson_create_number(pow(2, 53));
    hcjson *num3 = hcjson_create_number(-5678);

    hcjson_add_item_to_object(json, "str", str);
    hcjson_add_item_to_object(json, "num", num);
    hcjson_add_item_to_object(json, "flo", flo);
    hcjson_add_item_to_object(json, "boo", boo);
    hcjson_add_item_to_object(json, "obj", obj);

    hcjson_add_item_to_object(obj, "num2", num2);
    hcjson_add_item_to_object(obj, "num3", num3);

    hcjson_copy_item_to_object(json, "num2", num2);
    hcjson_copy_item_to_object(json, "num3", num3);

    hcjson *ret = hcjson_remove_item_in_object(json, "num2");
    hcjson_destroy(ret);

    hcjson *arr = hcjson_create_array();

    hcjson *elm1 = hcjson_create_number(9);
    hcjson *elm2 = hcjson_create_number(8);
    hcjson *elm3 = hcjson_create_number(7);
    hcjson *elm4 = hcjson_create_string("woo");
    hcjson *elm5 = hcjson_create_false();

    hcjson_add_item_to_array(arr, elm1);
    hcjson_add_item_to_array(arr, elm2);
    hcjson_add_item_to_array(arr, elm3);
    hcjson_add_item_to_array(arr, elm4);
    hcjson_add_item_to_array(arr, elm5);

    hcjson_add_item_to_object(json, "arr", arr);

    printf("size: %u\n", hcjson_list_size(json));

    char *json_str = hcjson_to_string(json);
    printf("%s\n", json_str);

    hcjson__free(json_str);


    char *file = loadFile("file.json");
    hcjson_parse(file);


    printf("s_memAllocCount: %zu\n", s_memAllocCount);

    hcjson_destroy(json);

    printf("s_memAllocCount: %zu\n", s_memAllocCount);
    return 0;
}
