#include "hcjson.h"


bool hcjson__whitespace(char c);
bool hcjson__is_digit(char c);
void hcjson__token_buffer_expand(hcjson_token_buffer *tbuff);
void hcjson__token_buffer_add(hcjson_token_buffer *tbuff, const hcjson_token_str tstr);
hcjson_token_str hcjson__create_token_str(hcjson_token token, const char* str, size_t len);
hcjson_token_str hcjson__lex_value_str(hcjson_parser *ps, size_t *pos, size_t len);
hcjson_token_str hcjson__lex_value(hcjson_parser *ps, size_t *pos, size_t len);
void hcjson__lex_json(hcjson_parser *ps);


bool hcjson__whitespace(char c) {
    return (c == ' ' || c == '\n' || c == '\t' || c == '\r');
}

bool hcjson__is_digit(char c) {
    return (c >= '0' && c <= '9');
}

int hcjson__parse_number_str(const char* str, size_t len, hcjson *json) {
    HCJSON_ASSERT(str, "str cannot be null");

    int found_dec = false;
    int found_exp = false;
    int neg = false;

    /* check leading cases */
    if (!len) {
        return false;
    }
    if (!(str[0] == '-' || hcjson__is_digit(str[0]))) {
        return false;
    }
    if (len == 1 && str[0] == '-') {
        return false;
    }
    if (str[0] == '-') {
        neg = true;
    }

    /* check leading 0 cases */
    if (len > 1 && str[0] == '0') {
        if (hcjson__is_digit(str[1])) {
            return false;
        }
    }
    if (len > 2 && (str[1] == '0' && str[0] == '-')) {
        if (hcjson__is_digit(str[2])) {
            return false;
        }
    }

    for (size_t i = 1; i < len; i++) {
        /* check fractional cases */
        if (str[i] == '.') {
            if (found_exp) {
                return false;
            }
            if (found_dec) {
                return false;
            }
            found_dec = true;

            /* check digit defined before and after decimal */
            if (!hcjson__is_digit(str[i-1]) || !(i < len-1 && hcjson__is_digit(str[i+1]))) {
                return false;
            }
        }
        else if (str[i] == 'e' || str[i] == 'E') {
            if (found_exp) {
                return false;
            }
            found_exp = true;

            /* check digit defined before and after exponent or +/- */
            if (!hcjson__is_digit(str[i-1])) {
                return false;
            }
            if (!(i < len-1 && (hcjson__is_digit(str[i+1]) || str[i+1] == '+' || str[i+1] == '-'))) {
                return false;
            }
        }
        else if (str[i] == '-' || str[i] == '+') {
            /* check exponent defined before and digit after +/- */
            if (str[i-1] != 'e' && str[i-1] != 'E') {
                return false;
            }
            if (!(i < len-1 && hcjson__is_digit(str[i+1]))) {
                return false;
            }
        }
        /* check if valid number */
        else if (!hcjson__is_digit(str[i])) {
            return false;
        }
    }

    if (!json) {
        return true;
    }

    char *buff, *end;
    buff = (char*) hcjson__malloc(len + 1);
    if (!buff) { return false; }
    memcpy(buff, str, len);
    buff[len] = '\0';

    if (found_dec || found_exp) {
        double val = strtod(buff, &end);
        if (buff == end) {
            hcjson__free(buff);
            return false;
        }
        json->type = HCJSON_TYPE_NUMBER | HCJSON_TYPE_NUMBER_FLOAT;
        json->value.floatv = val;
    }
    else if (neg) {
        int64_t val = strtoll(buff, &end, 10);
        if (buff == end) {
            hcjson__free(buff);
            return false;
        }
        json->type = HCJSON_TYPE_NUMBER | HCJSON_TYPE_NUMBER_INT;
        json->value.intv = val;
    }
    else {
        uint64_t val = strtoull(buff, &end, 10);
        if (buff == end) {
            hcjson__free(buff);
            return false;
        }
        json->type = HCJSON_TYPE_NUMBER | HCJSON_TYPE_NUMBER_UINT;
        json->value.uintv = val;
    }

    hcjson__free(buff);

    return true;
}

void hcjson__token_buffer_expand(hcjson_token_buffer *tbuff) {
    HCJSON_ASSERT(tbuff, "tbuff cannot be null");
    tbuff->capacity = tbuff->capacity ? (tbuff->capacity * HCJSON_TOKEN_BUFF_MULTIPLIER) : HCJSON_TOKEN_BUFF_SIZE;
    hcjson_token_str *temp = (hcjson_token_str*) hcjson__malloc(tbuff->capacity * sizeof(hcjson_token_str));
    memcpy(temp, tbuff->tokens, tbuff->size * sizeof(hcjson_token_str));
    hcjson__free(tbuff->tokens);
    tbuff->tokens = temp;
}

void hcjson__token_buffer_add(hcjson_token_buffer *tbuff, const hcjson_token_str tstr) {
    HCJSON_ASSERT(tbuff, "tbuff cannot be null");
    if (tbuff->size >= tbuff->capacity) {
        hcjson__token_buffer_expand(tbuff);
    }
    tbuff->tokens[tbuff->size++] = tstr;
}

hcjson_token_str hcjson__create_token_str(hcjson_token token, const char* str, size_t len) {
    HCJSON_ASSERT(str, "str cannot be null");
    return (hcjson_token_str){
        .token = token,
        .str = str,
        .len = len,
    };
}

hcjson_token_str hcjson__lex_value_str(hcjson_parser *ps, size_t *pos, size_t len) {
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

hcjson_token_str hcjson__lex_value(hcjson_parser *ps, size_t *pos, size_t len) {
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
    else if (hcjson__parse_number_str(tstr.str, tstr.len, NULL)) {
        tstr.token = HCJSON_TOKEN_NUMBER;
    }

    return tstr;
}

void hcjson__lex_json(hcjson_parser *ps) {
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
                hcjson__token_buffer_add(&ps->tbuff, hcjson__create_token_str(HCJSON_TOKEN_LBRACE, "{", 1));
                pos++;
                break;
            case '}':
                hcjson__token_buffer_add(&ps->tbuff, hcjson__create_token_str(HCJSON_TOKEN_RBRACE, "}", 1));
                pos++;
                break;
            case '[':
                hcjson__token_buffer_add(&ps->tbuff, hcjson__create_token_str(HCJSON_TOKEN_LBRACKET, "[", 1));
                pos++;
                break;
            case ']':
                hcjson__token_buffer_add(&ps->tbuff, hcjson__create_token_str(HCJSON_TOKEN_RBRACKET, "]", 1));
                pos++;
                break;
            case ',':
                hcjson__token_buffer_add(&ps->tbuff, hcjson__create_token_str(HCJSON_TOKEN_COMMA, ",", 1));
                pos++;
                break;
            case ':':
                hcjson__token_buffer_add(&ps->tbuff, hcjson__create_token_str(HCJSON_TOKEN_COLON, ":", 1));
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

hcjson *hcjson__parse_item(hcjson_parser *ps) {
    HCJSON_ASSERT(ps, "ps cannot be null");

    switch (ps->tbuff.tokens[ps->tbuff_index].token) {
        case HCJSON_TOKEN_LBRACE:
            break;
        case HCJSON_TOKEN_LBRACKET:
            break;
        case HCJSON_TOKEN_STRING:
            break;
    }
}

hcjson *hcjson__parse_json(hcjson_parser *ps) {
}

hcjson *hcjson_parse(const char* json) {
    HCJSON_ASSERT(json, "json cannot be null");

    hcjson_parser ps = {0};

    ps.json = json;
    hcjson__lex_json(&ps);

    for (int i = 0; i < ps.tbuff.size; i++) {
        printf("%.*s\n", (int)ps.tbuff.tokens[i].len, ps.tbuff.tokens[i].str);
    }

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
    char *file, *json;
    const char *str;
    hcjson obj;

    file = loadFile("file.json");

    hcjson_parse(file);


    /* ==================== */
    /* Valid JSON numbers   */
    /* ==================== */

    str = "0";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "-0";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "-1";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "123456789";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "-123456789";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    /* Fractional */

    str = "0.0";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "0.5";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "-0.5";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "10.0";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "-10.0";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "12.45";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "123456789.123456789";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    /* Exponents */

    str = "1e0";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1E0";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1e1";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1e+1";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1e-1";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1E+10";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1E-10";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "3.5e+10";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "3.5e-10";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "2.2E-3";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "-2.2E-3";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "6.022e23";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    /* Large/small exponents */

    str = "1e100";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1e-100";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1.23456789e+100";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "-1.23456789e-100";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));


    /* ==================== */
    /* Invalid JSON numbers */
    /* ==================== */

    str = "";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "-";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "+1";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "01";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "00";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "-01";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = ".5";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1.";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "-.5";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1.e5";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1e";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1e+";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1e-";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1e.";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1e1.5";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1ee5";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1E+";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1E-";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1.2.3";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "1a";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "123abc";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "abc123";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "NaN";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "Infinity";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "inf";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = " 10";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "10 ";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));

    str = "10\n";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), NULL));


    str = "-1234123412341234";
    printf("%s | %d\n", str, hcjson__parse_number_str(str, strlen(str), &obj));

    if (obj.type & HCJSON_TYPE_NUMBER_FLOAT) {
        printf("float: %lf\n", obj.value.floatv);
    }
    else if (obj.type & HCJSON_TYPE_NUMBER_INT) {
        printf("int: %ld\n", obj.value.intv);
    }
    else if (obj.type & HCJSON_TYPE_NUMBER_UINT) {
        printf("uint: %lu\n", obj.value.uintv);
    }
}
