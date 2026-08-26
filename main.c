#define HCJSON_MALLOC(sz) custom_malloc(sz)
#define HCJSON_FREE(ptr) custom_free(ptr)

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

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


static bool          hcjson__is_list_json(hcjson *json);
static hcjson_result hcjson__add_item_to_list_json(hcjson *json, const char *key, hcjson *item);
static hcjson_result hcjson__remove_item_in_obj(hcjson *obj, const char *key, bool destroy, hcjson **ret);
static hcjson_result hcjson__remove_item_in_arr(hcjson *arr, uint32_t index, bool destroy, hcjson **ret);

static bool hcjson__is_list_json(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & (HCJSON_TYPE_OBJECT | HCJSON_TYPE_ARRAY);
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

bool hcjson_is_object(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_OBJECT;
}

bool hcjson_is_array(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_ARRAY;
}

bool hcjson_is_true(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_TRUE;
}

bool hcjson_is_false(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_FALSE;
}

bool hcjson_is_null(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_NULL;
}

bool hcjson_is_string(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_STRING;
}

bool hcjson_is_number(hcjson *json) {
    HCJSON_ASSERT(json, "json cannot be null");
    return json->type & HCJSON_TYPE_NUMBER;
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

hcjson_result hcjson_add_item_to_array(hcjson *arr, const char *key, hcjson *item) {
    HCJSON_ASSERT(arr && key && item, "arr, key, and item cannot be null");
    return hcjson__add_item_to_list_json(arr, key, item);
}

hcjson_result hcjson_copy_item_to_array(hcjson *arr, const char *key, const hcjson *item) {
    HCJSON_ASSERT(arr && key && item, "arr, key, and item cannot be null");
    if (!hcjson_is_array(arr)) { return HCJSON_ERROR_INVALID_TYPE; }
    hcjson *copy = hcjson__malloc(sizeof(hcjson));
    if (!copy) { return HCJSON_ERROR_MALLOC_FAILURE; }
    memcpy(copy, item, sizeof(hcjson));
    copy->type &= ~HCJSON_TAG_TRANSFERRED;
    return hcjson_add_item_to_array(arr, key, copy);
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
    HCJSON_ASSERT(json, "json cannot be null");
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
    hcjson *num2 = hcjson_create_number(1234);
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

    for (hcjson *item = json->value.child; item; item = item->next) {
        if (item->type & HCJSON_TYPE_TRUE) {
            printf("%s: true\n", item->key);
        }
        else if (item->type & HCJSON_TYPE_FALSE) {
            printf("%s: false\n", item->key);
        }
        else if (item->type & HCJSON_TYPE_NULL) {
            printf("%s: null\n", item->key);
        }
        else if (item->type & HCJSON_TYPE_STRING) {
            printf("%s: %s\n", item->key, item->value.str);
        }
        else if (item->type & HCJSON_TYPE_NUMBER) {
            printf("%s: %lf\n", item->key, item->value.num);
        }
    }

    printf("size: %u\n", hcjson_list_size(json));

    printf("s_memAllocCount: %zu\n", s_memAllocCount);

    hcjson_destroy(json);

    printf("s_memAllocCount: %zu\n", s_memAllocCount);
    return 0;
}
