#include "hcjson.h"

#define HCJSON_FALSE 0
#define HCJSON_TRUE (!HCJSON_FALSE)

int hcjson__whitespace(char c);
int hcjson__is_digit(char c);
void hcjson__token_buffer_expand(hcjson_token_buffer *tbuff);
void hcjson__token_buffer_add(hcjson_token_buffer *tbuff, const hcjson_token_str tstr);
hcjson_token_str hcjson__create_token_str(hcjson_token token, const char* str, size_t len);
hcjson_token_str hcjson__lex_value_str(hcjson_parser *ps, size_t *pos, size_t len);
hcjson_token_str hcjson__lex_value(hcjson_parser *ps, size_t *pos, size_t len);
void hcjson__lex_json(hcjson_parser *ps);


int hcjson__whitespace(char c) {
    return (c == ' ' || c == '\n' || c == '\t' || c == '\r') ? HCJSON_TRUE : HCJSON_FALSE;
}

int hcjson__is_digit(char c) {
    return (c >= '0' && c <= '9') ? HCJSON_TRUE : HCJSON_FALSE;
}

int hcjson__parse_number_str(const char* str, size_t len, hcjson *json) {
    int found_dec, found_exp, neg;
    double vald;
    int64_t val64i;
    uint64_t val64ui;
    size_t i;

    HCJSON_ASSERT(str, "str cannot be null");

    found_dec = HCJSON_FALSE;
    found_exp = HCJSON_FALSE;
    neg = HCJSON_FALSE;

    /* check leading cases */
    if (!len) {
        return HCJSON_FALSE;
    }
    if (!(str[0] == '-' || hcjson__is_digit(str[0]))) {
        return HCJSON_FALSE;
    }
    if (len == 1 && str[0] == '-') {
        return HCJSON_FALSE;
    }
    if (str[0] == '-') {
        neg = HCJSON_TRUE;
    }

    /* check leading 0 cases */
    if (len > 1 && str[0] == '0') {
        if (hcjson__is_digit(str[1])) {
            return HCJSON_FALSE;
        }
    }
    if (len > 2 && (str[1] == '0' && str[0] == '-')) {
        if (hcjson__is_digit(str[2])) {
            return HCJSON_FALSE;
        }
    }

    for (i = 1; i < len; i++) {
        /* check fractional cases */
        if (str[i] == '.') {
            if (found_exp) {
                return HCJSON_FALSE;
            }
            if (found_dec) {
                return HCJSON_FALSE;
            }
            found_dec = HCJSON_TRUE;

            /* check digit defined before and after decimal */
            if (!hcjson__is_digit(str[i-1]) || !(i < len-1 && hcjson__is_digit(str[i+1]))) {
                return HCJSON_FALSE;
            }
        }
        else if (str[i] == 'e' || str[i] == 'E') {
            if (found_exp) {
                return HCJSON_FALSE;
            }
            found_exp = HCJSON_TRUE;

            /* check digit defined before and after exponent or +/- */
            if (!hcjson__is_digit(str[i-1])) {
                return HCJSON_FALSE;
            }
            if (!(i < len-1 && (hcjson__is_digit(str[i+1]) || str[i+1] == '+' || str[i+1] == '-'))) {
                return HCJSON_FALSE;
            }
        }
        else if (str[i] == '-' || str[i] == '+') {
            /* check exponent defined before and digit after +/- */
            if (str[i-1] != 'e' && str[i-1] != 'E') {
                return HCJSON_FALSE;
            }
            if (!(i < len-1 && hcjson__is_digit(str[i+1]))) {
                return HCJSON_FALSE;
            }
        }
        /* check if valid number */
        else if (!hcjson__is_digit(str[i])) {
            return HCJSON_FALSE;
        }
    }

    if (!json) {
        return HCJSON_TRUE;
    }

    return HCJSON_TRUE;
}

void hcjson__token_buffer_expand(hcjson_token_buffer *tbuff) {
    hcjson_token_str *temp;
    HCJSON_ASSERT(tbuff, "tbuff cannot be null");
    tbuff->capacity = tbuff->capacity ? (tbuff->capacity * HCJSON_TOKEN_BUFF_MULTIPLIER) : HCJSON_TOKEN_BUFF_SIZE;
    temp = (hcjson_token_str*) hcjson__malloc(tbuff->capacity * sizeof(hcjson_token_str));
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
    hcjson_token_str tstr;
    HCJSON_ASSERT(str, "str cannot be null");
    tstr.token = token;
    tstr.str = str;
    tstr.len = len;
    return tstr;
}

hcjson_token_str hcjson__lex_value_str(hcjson_parser *ps, size_t *pos, size_t len) {
    hcjson_token_str tstr;

    HCJSON_ASSERT(ps && pos, "ps and pos cannot be null");

    tstr.token = HCJSON_TOKEN_STRING;
    tstr.str = &ps->json[++(*pos)];
    tstr.len = 0;
    while (ps->json[*pos] != '\"' && *pos < len) {
        (*pos)++;
        tstr.len++;
    }

    /* swallow end quote */
    (*pos)++;

    return tstr;
}

hcjson_token_str hcjson__lex_value(hcjson_parser *ps, size_t *pos, size_t len) {
    hcjson_token_str tstr;

    HCJSON_ASSERT(ps && pos, "ps and pos cannot be null");

    tstr.token = HCJSON_TOKEN_INVALID;
    tstr.str = &ps->json[*pos];
    tstr.len = 0;
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
    size_t pos, len;

    HCJSON_ASSERT(ps, "ps cannot be null");

    pos = 0;
    len = strlen(ps->json);

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
    hcjson_parser ps;
    int i;

    HCJSON_ASSERT(json, "json cannot be null");

    memset(&ps, 0, sizeof(hcjson_parser));
    ps.json = json;
    hcjson__lex_json(&ps);

    for (i = 0; i < ps.tbuff.size; i++) {
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
}
