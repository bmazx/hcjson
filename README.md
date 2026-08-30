# hcjson
header C json, small and simple single header library written in C99 for reading and writing json.

# Quickstart
add a new empty source file to your project and include this
```c
#define HCJSON_IMPL
#include "hcjson.h"
```

## json Structure
```c
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
```
this is the basis json structure for all json objects and items
- the hcjson struct works like a linked list with each element being in the same object/array
- each hcjson can store a string, number, or child hcjson used to store nested objects/arrays

## Creating json
Use these functions to create and allocate a hcjson struct for each json type:
```c
hcjson *hcjson_create_object(void);
hcjson *hcjson_create_array(void);
hcjson *hcjson_create_true(void);
hcjson *hcjson_create_false(void);
hcjson *hcjson_create_null(void);
hcjson *hcjson_create_string(const char *str);
hcjson *hcjson_create_number(double num);
```

code example:
```c
hcjson *obj = hcjson_create_object();
hcjson *num = hcjson_create_number(3.1415);
hcjson *str = hcjson_create_string("foo");
```

json items can be added to objects or arrays using these functions:
```c
hcjson_result hcjson_add_item_to_object(hcjson *obj, const char *key, hcjson *item);
hcjson_result hcjson_add_item_to_array(hcjson *arr, hcjson *item);
```

code example:
```c
hcjson *obj = hcjson_create_object();

hcjson *num = hcjson_create_number(42);
hcjson_add_item_to_object(obj, "mykey", num);

hcjson *arr = hcjson_create_array();
hcjson *str = hcjson_create_string("hello hcjson");
hcjson_add_item_to_object(arr, str);
```
Notice there is no key parameter for `hcjson_add_item_to_array()` since arrays do not have keys in json.

**Important: hcjson does now allow references of the same struct to be added to the same hcjson struct **

meaning you cannot do this:
```c
hcjson *json = hcjson_create_object();
hcjson *num = hcjson_create_number(124);

hcjson_add_item_to_object(json, "key1", num); // add num to json
hcjson_add_item_to_object(json, "key2", num); // not allowed: adding the same num hcjson reference to json again
```
this would result in `hcjson_add_item_to_object` to return `HCJSON_ERROR_ITEM_ALREADY_TRANSFERRED` and hcjson will not add num again to the struct.

If you need to add items with duplicate values, use:
```c
hcjson_result hcjson_copy_item_to_object(hcjson *obj, const char *key, const hcjson *item);
hcjson_result hcjson_copy_item_to_array(hcjson *arr, const hcjson *item);
```

## Checking type
You can check the type of a hcjson struct using these functions:
```c
bool hcjson_is_object(const hcjson *json);
bool hcjson_is_array(const hcjson *json);
bool hcjson_is_true(const hcjson *json);
bool hcjson_is_false(const hcjson *json);
bool hcjson_is_null(const hcjson *json);
bool hcjson_is_string(const hcjson *json);
bool hcjson_is_number(const hcjson *json);
```

## Creating json structure and print to output
this example creates a json structure in C and then prints the structure to output, the resulting output should look like this:
```json
{
    "ships": [
        {
            "name": "Flivver",
            "category": "Transport",
            "cost": 180000,
            "shields": 1400,
            "fuel": 500,
            "cargo_space": 15
        },
        {
            "name": "Firebird",
            "category": "Medium Warship",
            "cost": 3700000,
            "shields": 5800,
            "fuel": 400,
            "cargo_space": 50
        },
        {
            "name": "Bactrian",
            "category": "Utility",
            "cost": 17600000,
            "shields": 17500,
            "fuel": 700,
            "cargo_space": 530
        }
    ]
}
```

example code to create json structure:
```c
#define HCJSON_IMPL
#include "hcjson.h"

struct ship_info {
    const char *name;
    const char *category;
    double cost;
    double shields;
    double mass;
    double fuel;
    double cargo_space;
};

hcjson *create_ship_info() {
    hcjson *json;
    hcjson *name;
    hcjson *ships;

    hcjson *ship;
    hcjson *category;
    hcjson *cost;
    hcjson *shields;
    hcjson *mass;
    hcjson *fuel;
    hcjson *cargo;

    json = hcjson_create_object();

    ships = hcjson_create_array();
    hcjson_add_item_to_object(json, "ships", ships);

    struct ship_info ship_infos[] = {
        { "Flivver", "Transport", 180000, 1400, 50, 500, 15 },
        { "Firebird", "Medium Warship", 3700000, 5800, 630, 400, 50 },
        { "Bactrian", "Utility", 17600000, 17500, 2450, 700, 530 },
    };

    for (int i = 0; i < sizeof(ship_infos) / sizeof(ship_infos[0]); i++) {
        ship = hcjson_create_object();
        hcjson_add_item_to_array(ships, ship);

        name = hcjson_create_string(ship_infos[i].name);
        hcjson_add_item_to_object(ship, "name", name);

        category = hcjson_create_string(ship_infos[i].category);
        hcjson_add_item_to_object(ship, "category", category);

        cost = hcjson_create_number(ship_infos[i].cost);
        hcjson_add_item_to_object(ship, "cost", cost);

        shields = hcjson_create_number(ship_infos[i].shields);
        hcjson_add_item_to_object(ship, "shields", shields);

        fuel = hcjson_create_number(ship_infos[i].fuel);
        hcjson_add_item_to_object(ship, "fuel", fuel);

        cargo = hcjson_create_number(ship_infos[i].cargo_space);
        hcjson_add_item_to_object(ship, "cargo_space", cargo);
    }

    return json;
}

int main(int argc, char **argv) {
    // create the json structure
    hcjson *json = create_ship_info();

    // get the json struct as a string and print it
    char *json_str = hcjson_to_string(json);

    printf("%s\n", json_str);

    // free the json struct
    hcjson_destroy(json);

    // also remember to free the string
    free(json_str);

    return 0;
}
```

## Create hcjson struct from json string
First get the the source string for json string either from a file or string in C.

then parse the string with `hcjson_parse()`, for example:
```c
int main() {
    const char *json_str = "{\"foo\": \"hello hcjson\", \"bar\": 567, \"val\": true}";

    // create json struct from string
    hcjson *json = hcjson_parse(json_str);

    // destroy the json structure when done
    hcjson_destroy(json);
    return 0;
}
```

## Memory Allocation
By default hcjson uses malloc and free to allocate memory.
To redirect memory allocation, define these macros with your own memory allocators before including hcjson.
```c

#define HCJSON_MALLOC(sz) custom_malloc(sz)
#define HCJSON_FREE(ptr) custom_free(ptr)

#define HCJSON_IMPL
#include "hcjson.h"
```
