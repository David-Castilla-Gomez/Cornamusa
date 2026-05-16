/*
 * Tests del parser/builder JSON minimo (v1.52 - soporte LSP).
 *
 * Cubre los casos que el LSP usa:
 *   - Parse de objetos anidados, strings con escapes, numeros, null/bool, arrays.
 *   - Lookup por clave.
 *   - Builder: emite formato canonico, comas correctas, escapes correctos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_min.h"

static int fallos = 0;
static int casos = 0;

#define AFIRMAR(cond, etiqueta)                                                \
    do {                                                                        \
        casos++;                                                                \
        if (!(cond)) {                                                          \
            fprintf(stderr, "FALLO %s:%d (%s)\n", __FILE__, __LINE__, etiqueta);\
            fallos++;                                                           \
        }                                                                       \
    } while (0)

int main(void) {
    /* ─── Parser ─── */

    {
        const char *s = "{\"a\": 1, \"b\": \"hola\", \"c\": true, \"d\": null}";
        JsonValue *v = json_parse(s, strlen(s));
        AFIRMAR(v && v->type == JV_OBJECT, "obj_parse");
        AFIRMAR(json_int(json_obj_get(v, "a")) == 1, "obj_int");
        AFIRMAR(strcmp(json_string(json_obj_get(v, "b")), "hola") == 0, "obj_str");
        AFIRMAR(json_bool(json_obj_get(v, "c")) == true, "obj_bool");
        AFIRMAR(json_is_null(json_obj_get(v, "d")), "obj_null");
        AFIRMAR(json_obj_get(v, "no_existe") == NULL, "obj_miss");
        json_free(v);
    }

    /* Array. */
    {
        const char *s = "[1, 2, 3]";
        JsonValue *v = json_parse(s, strlen(s));
        AFIRMAR(v && v->type == JV_ARRAY, "arr_parse");
        AFIRMAR(json_int(json_arr_at(v, 0)) == 1, "arr_0");
        AFIRMAR(json_int(json_arr_at(v, 2)) == 3, "arr_2");
        AFIRMAR(json_arr_at(v, 10) == NULL, "arr_oob");
        json_free(v);
    }

    /* Anidacion. */
    {
        const char *s = "{\"params\":{\"textDocument\":{\"uri\":\"file:///a.cor\"}}}";
        JsonValue *v = json_parse(s, strlen(s));
        AFIRMAR(v != NULL, "anidado_parse");
        const char *uri = json_string(
            json_obj_get(
                json_obj_get(
                    json_obj_get(v, "params"),
                    "textDocument"),
                "uri"));
        AFIRMAR(strcmp(uri, "file:///a.cor") == 0, "anidado_uri");
        json_free(v);
    }

    /* Escapes en strings. */
    {
        const char *s = "{\"k\":\"linea1\\nlinea2\\ttab\"}";
        JsonValue *v = json_parse(s, strlen(s));
        AFIRMAR(v != NULL, "esc_parse");
        AFIRMAR(strcmp(json_string(json_obj_get(v, "k")),
                        "linea1\nlinea2\ttab") == 0, "esc_decode");
        json_free(v);
    }

    /* Numero negativo y decimal. */
    {
        const char *s = "{\"x\":-3.5}";
        JsonValue *v = json_parse(s, strlen(s));
        double x = json_number(json_obj_get(v, "x"));
        AFIRMAR(x < -3.4 && x > -3.6, "num_negativo");
        json_free(v);
    }

    /* JSON invalido. */
    {
        const char *s = "{invalid}";
        JsonValue *v = json_parse(s, strlen(s));
        AFIRMAR(v == NULL, "invalido");
    }

    /* ─── Builder ─── */

    {
        JsonBuf b;
        json_buf_init(&b);
        json_buf_obj_start(&b);
            json_buf_key(&b, "id");      json_buf_int(&b, 42);
            json_buf_key(&b, "method");  json_buf_string(&b, "test");
        json_buf_obj_end(&b);
        AFIRMAR(strcmp(b.data, "{\"id\":42,\"method\":\"test\"}") == 0, "build_obj_simple");
        json_buf_free(&b);
    }

    {
        JsonBuf b;
        json_buf_init(&b);
        json_buf_arr_start(&b);
            json_buf_int(&b, 1);
            json_buf_int(&b, 2);
            json_buf_int(&b, 3);
        json_buf_arr_end(&b);
        AFIRMAR(strcmp(b.data, "[1,2,3]") == 0, "build_arr_ints");
        json_buf_free(&b);
    }

    /* Anidacion + escape. */
    {
        JsonBuf b;
        json_buf_init(&b);
        json_buf_obj_start(&b);
            json_buf_key(&b, "msg"); json_buf_string(&b, "linea1\nlinea2");
            json_buf_key(&b, "arr");
            json_buf_arr_start(&b);
                json_buf_obj_start(&b);
                    json_buf_key(&b, "x"); json_buf_int(&b, 1);
                json_buf_obj_end(&b);
                json_buf_obj_start(&b);
                    json_buf_key(&b, "x"); json_buf_int(&b, 2);
                json_buf_obj_end(&b);
            json_buf_arr_end(&b);
        json_buf_obj_end(&b);
        AFIRMAR(strcmp(b.data,
            "{\"msg\":\"linea1\\nlinea2\",\"arr\":[{\"x\":1},{\"x\":2}]}") == 0,
            "build_anidado");
        json_buf_free(&b);
    }

    /* Escape de comilla. */
    {
        JsonBuf b;
        json_buf_init(&b);
        json_buf_obj_start(&b);
            json_buf_key(&b, "k"); json_buf_string(&b, "a\"b");
        json_buf_obj_end(&b);
        AFIRMAR(strcmp(b.data, "{\"k\":\"a\\\"b\"}") == 0, "build_escape_quote");
        json_buf_free(&b);
    }

    /* null y bool. */
    {
        JsonBuf b;
        json_buf_init(&b);
        json_buf_obj_start(&b);
            json_buf_key(&b, "a"); json_buf_null(&b);
            json_buf_key(&b, "b"); json_buf_bool(&b, true);
            json_buf_key(&b, "c"); json_buf_bool(&b, false);
        json_buf_obj_end(&b);
        AFIRMAR(strcmp(b.data, "{\"a\":null,\"b\":true,\"c\":false}") == 0, "build_null_bool");
        json_buf_free(&b);
    }

    /* Round-trip: parsear lo que el builder produjo. */
    {
        JsonBuf b;
        json_buf_init(&b);
        json_buf_obj_start(&b);
            json_buf_key(&b, "method"); json_buf_string(&b, "initialize");
            json_buf_key(&b, "id"); json_buf_int(&b, 1);
        json_buf_obj_end(&b);

        JsonValue *v = json_parse(b.data, b.len);
        AFIRMAR(v != NULL, "round_trip");
        AFIRMAR(strcmp(json_string(json_obj_get(v, "method")), "initialize") == 0, "rt_method");
        AFIRMAR(json_int(json_obj_get(v, "id")) == 1, "rt_id");
        json_free(v);
        json_buf_free(&b);
    }

    if (fallos == 0) {
        printf("json_min: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "json_min: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
