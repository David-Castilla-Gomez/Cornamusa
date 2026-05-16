#ifndef CORNAMUSA_JSON_MIN_H
#define CORNAMUSA_JSON_MIN_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Mini parser/builder JSON pensado para el LSP server (v1.52).
 *
 * Cubre el subset necesario: objetos, arrays, strings (con escapes
 * basicos), numeros (enteros y decimales), booleanos, null.
 *
 * No es un parser RFC-completo: no maneja escapes \u Unicode, no
 * rechaza ciertos casos exoticos. Suficiente para mensajes LSP que
 * son ASCII + UTF-8 estandar.
 */

typedef enum {
    JV_NULL,
    JV_BOOL,
    JV_NUMBER,
    JV_STRING,
    JV_ARRAY,
    JV_OBJECT,
} JsonType;

typedef struct JsonValue JsonValue;

struct JsonValue {
    JsonType type;
    union {
        bool b;
        double num;
        struct {
            char *data;    /* malloc'd, NUL-terminada */
            size_t len;    /* bytes (sin contar NUL) */
        } str;
        struct {
            JsonValue **items;
            size_t n;
            size_t cap;
        } arr;
        struct {
            char **keys;
            size_t *key_lens;
            JsonValue **values;
            size_t n;
            size_t cap;
        } obj;
    } as;
};

/* Parser. Devuelve NULL si la entrada no es JSON valido. */
JsonValue *json_parse(const char *texto, size_t len);

void json_free(JsonValue *v);

/* Lookup en objetos. Devuelve NULL si la clave no existe. La
 * comparacion es por bytes — UTF-8 funciona si el origen tambien. */
JsonValue *json_obj_get(const JsonValue *v, const char *key);

/* Lookup en arrays. NULL si fuera de rango. */
JsonValue *json_arr_at(const JsonValue *v, size_t i);

/* Convenience accessors. Devuelven valor por defecto si el tipo no
 * coincide. */
const char *json_string(const JsonValue *v);    /* "" si no es string */
size_t json_string_len(const JsonValue *v);     /* 0 si no es string */
double json_number(const JsonValue *v);          /* 0.0 si no es numero */
long json_int(const JsonValue *v);               /* 0 si no es numero */
bool json_bool(const JsonValue *v);              /* false si no es bool */
bool json_is_null(const JsonValue *v);

/* ──────────────────────────────────────────────────────────────────
 * Builder: construye JSON en un buffer.
 *
 * Patron de uso:
 *   JsonBuf b; json_buf_init(&b);
 *   json_buf_obj_start(&b);
 *     json_buf_key(&b, "id");           json_buf_int(&b, 1);
 *     json_buf_key(&b, "method");       json_buf_string(&b, "initialize");
 *   json_buf_obj_end(&b);
 *   // b.data, b.len contienen el resultado.
 *   json_buf_free(&b);
 * ────────────────────────────────────────────────────────────────── */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    /* Stack para tracking de comas — para cada nivel anidado,
     * llevamos si ya emitimos al menos un elemento. */
    bool need_comma_stack[64];
    int depth;
} JsonBuf;

void json_buf_init(JsonBuf *b);
void json_buf_free(JsonBuf *b);

void json_buf_obj_start(JsonBuf *b);
void json_buf_obj_end(JsonBuf *b);
void json_buf_arr_start(JsonBuf *b);
void json_buf_arr_end(JsonBuf *b);

/* Emite una clave dentro de un objeto. Llama esto antes de cada
 * valor que añadas al objeto. */
void json_buf_key(JsonBuf *b, const char *key);

/* Valores. Si estas dentro de un objeto, llama json_buf_key primero.
 * Si estas dentro de un array, no llames key. */
void json_buf_string(JsonBuf *b, const char *s);
void json_buf_string_n(JsonBuf *b, const char *s, size_t n);
void json_buf_int(JsonBuf *b, long v);
void json_buf_bool(JsonBuf *b, bool v);
void json_buf_null(JsonBuf *b);
/* Inserta JSON crudo (avanzado — usar solo si sabes lo que haces). */
void json_buf_raw(JsonBuf *b, const char *s, size_t n);

#endif /* CORNAMUSA_JSON_MIN_H */
