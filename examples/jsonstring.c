#define HCJSON_IMPL
#include "hcjson.h"

const char *json_str =
"{"
"  \"stars\": ["
"    {"
"      \"name\": \"Alpha Centauri\","
"      \"distanceFromEarth\": 4.37,"
"      \"classType\": \"G2V\","
"      \"mass\": 1.1"
"    },"
"    {"
"      \"name\": \"Tau Ceti\","
"      \"distanceFromEarth\": 11.91,"
"      \"classType\": \"G8V\","
"      \"mass\": 0.78"
"    },"
"    {"
"      \"name\": \"Sirius\","
"      \"distanceFromEarth\": 8.6,"
"      \"classType\": \"A1V\","
"      \"mass\": 2.02"
"    }"
"  ]"
"}";

int main(int argc, char **argv) {
    // create json struct from string
    hcjson *json = hcjson_parse(json_str);

    // print the json struct
    char *out_str = hcjson_to_string(json);

    printf("%s\n", out_str);

    // destroy json struct and string
    hcjson_destroy(json);
    free(out_str);

    return 0;
}
