/*
* this example shows how to create json structures and add them to each other
* and then print out the structure by converting it to a string
*
* the following program will create this json structure below and print it:
*
* {
*     "ships": [
*         {
*             "name": "Flivver",
*             "category": "Transport",
*             "cost": 180000,
*             "shields": 1400,
*             "fuel": 500,
*             "cargo_space": 15
*         },
*         {
*             "name": "Firebird",
*             "category": "Medium Warship",
*             "cost": 3700000,
*             "shields": 5800,
*             "fuel": 400,
*             "cargo_space": 50
*         },
*         {
*             "name": "Bactrian",
*             "category": "Utility",
*             "cost": 17600000,
*             "shields": 17500,
*             "fuel": 700,
*             "cargo_space": 530
*         }
*     ]
* }
*/

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
