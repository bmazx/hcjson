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

int32_t hcjson_add_item_to_object(hcjson *obj, const char *key, hcjson *item) {
    HCJSON_ASSERT(obj && key && item, "obj, key, and item cannot be null");

    if (!(obj->type & HCJSON_TYPE_OBJECT)) {
        return HCJSON_ERROR_INVALID_TYPE;
    }
    if (item->type & HCJSON_TAG_TRANSFERRED) {
        return HCJSON_ERROR_ITEM_ALREADY_TRANSFERRED;
    }

    item->key = hcjson__strdup(key);
    item->type |= HCJSON_TAG_TRANSFERRED;
    item->next = NULL;
    item->prev = NULL;

    if (!obj->value.child) {
        obj->value.child = item;
        obj->m_tail = item;
        return HCJSON_SUCCESS;
    }

    obj->m_tail->next = item;
    item->prev = obj->m_tail;
    obj->m_tail = item;

    return HCJSON_SUCCESS;
}

int32_t hcjson_add_item_to_array(hcjson *obj, const char *key, hcjson *item) {
    HCJSON_ASSERT(obj && key && item, "obj, key, and item cannot be null");

    if (!(obj->type & HCJSON_TYPE_ARRAY)) {
        return HCJSON_ERROR_INVALID_TYPE;
    }
    if (item->type & HCJSON_TAG_TRANSFERRED) {
        return HCJSON_ERROR_ITEM_ALREADY_TRANSFERRED;
    }

    item->key = hcjson__strdup(key);
    item->type |= HCJSON_TAG_TRANSFERRED;
    item->next = NULL;
    item->prev = NULL;

    if (!obj->value.child) {
        obj->value.child = item;
        obj->m_tail = item;
        return HCJSON_SUCCESS;
    }

    obj->m_tail->next = item;
    item->prev = obj->m_tail;
    obj->m_tail = item;

    return HCJSON_SUCCESS;
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

    printf("s_memAllocCount: %zu\n", s_memAllocCount);

    hcjson_destroy(json);

    printf("s_memAllocCount: %zu\n", s_memAllocCount);
    return 0;
}
