#include "json_min.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────
 * Parser.
 * ────────────────────────────────────────────────────────────────── */

typedef struct {
    const char *src;
    size_t len;
    size_t pos;
    bool error;
} Parser;

static void skip_ws(Parser *p) {
    while (p->pos < p->len) {
        char c = p->src[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->pos++;
        else break;
    }
}

static JsonValue *parse_value(Parser *p);

static JsonValue *make_value(JsonType t) {
    JsonValue *v = (JsonValue *)calloc(1, sizeof(JsonValue));
    if (v) v->type = t;
    return v;
}

static JsonValue *parse_string(Parser *p) {
    if (p->pos >= p->len || p->src[p->pos] != '"') { p->error = true; return NULL; }
    p->pos++;  /* consumir " */

    /* Reservamos buf con estimacion: a lo sumo la longitud restante. */
    size_t cap = 32;
    char *buf = (char *)malloc(cap);
    size_t len = 0;
    if (!buf) { p->error = true; return NULL; }

    while (p->pos < p->len && p->src[p->pos] != '"') {
        char c = p->src[p->pos];
        if (c == '\\' && p->pos + 1 < p->len) {
            char esc = p->src[p->pos + 1];
            p->pos += 2;
            char dec;
            switch (esc) {
                case '"':  dec = '"'; break;
                case '\\': dec = '\\'; break;
                case '/':  dec = '/'; break;
                case 'b':  dec = '\b'; break;
                case 'f':  dec = '\f'; break;
                case 'n':  dec = '\n'; break;
                case 'r':  dec = '\r'; break;
                case 't':  dec = '\t'; break;
                case 'u':
                    /* No soportamos \uXXXX en v1.52 — saltamos 4 chars
                     * y emitimos '?'. LSP rara vez usa estos escapes en
                     * los campos que leemos. */
                    if (p->pos + 4 <= p->len) p->pos += 4;
                    dec = '?';
                    break;
                default:   dec = esc; break;
            }
            if (len + 1 >= cap) {
                cap *= 2;
                char *nv = (char *)realloc(buf, cap);
                if (!nv) { free(buf); p->error = true; return NULL; }
                buf = nv;
            }
            buf[len++] = dec;
        } else {
            if (len + 1 >= cap) {
                cap *= 2;
                char *nv = (char *)realloc(buf, cap);
                if (!nv) { free(buf); p->error = true; return NULL; }
                buf = nv;
            }
            buf[len++] = c;
            p->pos++;
        }
    }
    if (p->pos >= p->len) { free(buf); p->error = true; return NULL; }
    p->pos++;  /* consumir " final */

    buf[len] = '\0';
    JsonValue *v = make_value(JV_STRING);
    if (!v) { free(buf); return NULL; }
    v->as.str.data = buf;
    v->as.str.len = len;
    return v;
}

static JsonValue *parse_number(Parser *p) {
    size_t start = p->pos;
    if (p->pos < p->len && p->src[p->pos] == '-') p->pos++;
    while (p->pos < p->len
            && (isdigit((unsigned char)p->src[p->pos])
                || p->src[p->pos] == '.'
                || p->src[p->pos] == 'e'
                || p->src[p->pos] == 'E'
                || p->src[p->pos] == '+'
                || p->src[p->pos] == '-')) {
        p->pos++;
    }
    if (p->pos == start) { p->error = true; return NULL; }

    char tmp[64];
    size_t n = p->pos - start;
    if (n >= sizeof(tmp)) { p->error = true; return NULL; }
    memcpy(tmp, p->src + start, n);
    tmp[n] = '\0';

    JsonValue *v = make_value(JV_NUMBER);
    if (!v) return NULL;
    v->as.num = strtod(tmp, NULL);
    return v;
}

static bool match_word(Parser *p, const char *w) {
    size_t n = strlen(w);
    if (p->pos + n > p->len) return false;
    if (memcmp(p->src + p->pos, w, n) != 0) return false;
    p->pos += n;
    return true;
}

static JsonValue *parse_array(Parser *p) {
    if (p->pos >= p->len || p->src[p->pos] != '[') { p->error = true; return NULL; }
    p->pos++;
    JsonValue *v = make_value(JV_ARRAY);
    if (!v) return NULL;

    skip_ws(p);
    if (p->pos < p->len && p->src[p->pos] == ']') { p->pos++; return v; }

    for (;;) {
        skip_ws(p);
        JsonValue *item = parse_value(p);
        if (!item) { json_free(v); return NULL; }

        if (v->as.arr.n >= v->as.arr.cap) {
            size_t nc = v->as.arr.cap ? v->as.arr.cap * 2 : 4;
            JsonValue **nv = (JsonValue **)realloc(v->as.arr.items, sizeof(JsonValue *) * nc);
            if (!nv) { json_free(item); json_free(v); return NULL; }
            v->as.arr.items = nv;
            v->as.arr.cap = nc;
        }
        v->as.arr.items[v->as.arr.n++] = item;

        skip_ws(p);
        if (p->pos < p->len && p->src[p->pos] == ',') { p->pos++; continue; }
        if (p->pos < p->len && p->src[p->pos] == ']') { p->pos++; return v; }
        p->error = true;
        json_free(v);
        return NULL;
    }
}

static JsonValue *parse_object(Parser *p) {
    if (p->pos >= p->len || p->src[p->pos] != '{') { p->error = true; return NULL; }
    p->pos++;
    JsonValue *v = make_value(JV_OBJECT);
    if (!v) return NULL;

    skip_ws(p);
    if (p->pos < p->len && p->src[p->pos] == '}') { p->pos++; return v; }

    for (;;) {
        skip_ws(p);
        JsonValue *key = parse_string(p);
        if (!key) { json_free(v); return NULL; }
        skip_ws(p);
        if (p->pos >= p->len || p->src[p->pos] != ':') {
            json_free(key); json_free(v); p->error = true; return NULL;
        }
        p->pos++;
        skip_ws(p);
        JsonValue *val = parse_value(p);
        if (!val) { json_free(key); json_free(v); return NULL; }

        if (v->as.obj.n >= v->as.obj.cap) {
            size_t nc = v->as.obj.cap ? v->as.obj.cap * 2 : 4;
            char **nk = (char **)realloc(v->as.obj.keys, sizeof(char *) * nc);
            size_t *nl = (size_t *)realloc(v->as.obj.key_lens, sizeof(size_t) * nc);
            JsonValue **nv = (JsonValue **)realloc(v->as.obj.values, sizeof(JsonValue *) * nc);
            if (!nk || !nl || !nv) {
                free(nk); free(nl); free(nv);
                json_free(key); json_free(val); json_free(v);
                return NULL;
            }
            v->as.obj.keys = nk;
            v->as.obj.key_lens = nl;
            v->as.obj.values = nv;
            v->as.obj.cap = nc;
        }
        v->as.obj.keys[v->as.obj.n] = key->as.str.data;
        v->as.obj.key_lens[v->as.obj.n] = key->as.str.len;
        key->as.str.data = NULL;  /* transferimos ownership */
        free(key);
        v->as.obj.values[v->as.obj.n] = val;
        v->as.obj.n++;

        skip_ws(p);
        if (p->pos < p->len && p->src[p->pos] == ',') { p->pos++; continue; }
        if (p->pos < p->len && p->src[p->pos] == '}') { p->pos++; return v; }
        p->error = true;
        json_free(v);
        return NULL;
    }
}

static JsonValue *parse_value(Parser *p) {
    skip_ws(p);
    if (p->pos >= p->len) { p->error = true; return NULL; }
    char c = p->src[p->pos];
    if (c == '"') return parse_string(p);
    if (c == '{') return parse_object(p);
    if (c == '[') return parse_array(p);
    if (c == 't') {
        if (!match_word(p, "true")) { p->error = true; return NULL; }
        JsonValue *v = make_value(JV_BOOL);
        if (v) v->as.b = true;
        return v;
    }
    if (c == 'f') {
        if (!match_word(p, "false")) { p->error = true; return NULL; }
        JsonValue *v = make_value(JV_BOOL);
        if (v) v->as.b = false;
        return v;
    }
    if (c == 'n') {
        if (!match_word(p, "null")) { p->error = true; return NULL; }
        return make_value(JV_NULL);
    }
    if (c == '-' || isdigit((unsigned char)c)) return parse_number(p);
    p->error = true;
    return NULL;
}

JsonValue *json_parse(const char *texto, size_t len) {
    Parser p = { texto, len, 0, false };
    JsonValue *v = parse_value(&p);
    if (p.error) {
        json_free(v);
        return NULL;
    }
    return v;
}

void json_free(JsonValue *v) {
    if (!v) return;
    switch (v->type) {
        case JV_STRING:
            free(v->as.str.data);
            break;
        case JV_ARRAY:
            for (size_t i = 0; i < v->as.arr.n; i++) json_free(v->as.arr.items[i]);
            free(v->as.arr.items);
            break;
        case JV_OBJECT:
            for (size_t i = 0; i < v->as.obj.n; i++) {
                free(v->as.obj.keys[i]);
                json_free(v->as.obj.values[i]);
            }
            free(v->as.obj.keys);
            free(v->as.obj.key_lens);
            free(v->as.obj.values);
            break;
        default: break;
    }
    free(v);
}

/* ──────────────────────────────────────────────────────────────────
 * Accessors.
 * ────────────────────────────────────────────────────────────────── */

JsonValue *json_obj_get(const JsonValue *v, const char *key) {
    if (!v || v->type != JV_OBJECT || !key) return NULL;
    size_t klen = strlen(key);
    for (size_t i = 0; i < v->as.obj.n; i++) {
        if (v->as.obj.key_lens[i] == klen
            && memcmp(v->as.obj.keys[i], key, klen) == 0) {
            return v->as.obj.values[i];
        }
    }
    return NULL;
}

JsonValue *json_arr_at(const JsonValue *v, size_t i) {
    if (!v || v->type != JV_ARRAY) return NULL;
    if (i >= v->as.arr.n) return NULL;
    return v->as.arr.items[i];
}

const char *json_string(const JsonValue *v) {
    if (!v || v->type != JV_STRING) return "";
    return v->as.str.data;
}

size_t json_string_len(const JsonValue *v) {
    if (!v || v->type != JV_STRING) return 0;
    return v->as.str.len;
}

double json_number(const JsonValue *v) {
    if (!v || v->type != JV_NUMBER) return 0.0;
    return v->as.num;
}

long json_int(const JsonValue *v) {
    if (!v || v->type != JV_NUMBER) return 0;
    return (long)v->as.num;
}

bool json_bool(const JsonValue *v) {
    if (!v || v->type != JV_BOOL) return false;
    return v->as.b;
}

bool json_is_null(const JsonValue *v) {
    return !v || v->type == JV_NULL;
}

/* ──────────────────────────────────────────────────────────────────
 * Builder.
 * ────────────────────────────────────────────────────────────────── */

void json_buf_init(JsonBuf *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
    b->depth = 0;
}

void json_buf_free(JsonBuf *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void buf_grow(JsonBuf *b, size_t extra) {
    if (b->len + extra + 1 <= b->cap) return;
    size_t nc = b->cap ? b->cap : 256;
    while (nc < b->len + extra + 1) nc *= 2;
    char *nv = (char *)realloc(b->data, nc);
    if (!nv) return;
    b->data = nv;
    b->cap = nc;
}

static void buf_putc(JsonBuf *b, char c) {
    buf_grow(b, 1);
    if (b->data) {
        b->data[b->len++] = c;
        b->data[b->len] = '\0';
    }
}

static void buf_puts(JsonBuf *b, const char *s, size_t n) {
    buf_grow(b, n);
    if (b->data) {
        memcpy(b->data + b->len, s, n);
        b->len += n;
        b->data[b->len] = '\0';
    }
}

static void buf_emit_comma_if_needed(JsonBuf *b) {
    if (b->depth > 0 && b->need_comma_stack[b->depth - 1]) {
        buf_putc(b, ',');
    }
    if (b->depth > 0) b->need_comma_stack[b->depth - 1] = true;
}

void json_buf_obj_start(JsonBuf *b) {
    buf_emit_comma_if_needed(b);
    buf_putc(b, '{');
    if (b->depth < 64) {
        b->need_comma_stack[b->depth] = false;
        b->depth++;
    }
}

void json_buf_obj_end(JsonBuf *b) {
    buf_putc(b, '}');
    if (b->depth > 0) b->depth--;
}

void json_buf_arr_start(JsonBuf *b) {
    buf_emit_comma_if_needed(b);
    buf_putc(b, '[');
    if (b->depth < 64) {
        b->need_comma_stack[b->depth] = false;
        b->depth++;
    }
}

void json_buf_arr_end(JsonBuf *b) {
    buf_putc(b, ']');
    if (b->depth > 0) b->depth--;
}

static void buf_emit_escaped_string(JsonBuf *b, const char *s, size_t n) {
    buf_putc(b, '"');
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  buf_puts(b, "\\\"", 2); break;
            case '\\': buf_puts(b, "\\\\", 2); break;
            case '\n': buf_puts(b, "\\n", 2); break;
            case '\r': buf_puts(b, "\\r", 2); break;
            case '\t': buf_puts(b, "\\t", 2); break;
            case '\b': buf_puts(b, "\\b", 2); break;
            case '\f': buf_puts(b, "\\f", 2); break;
            default:
                if (c < 0x20) {
                    char tmp[8];
                    int k = snprintf(tmp, sizeof(tmp), "\\u%04x", c);
                    if (k > 0) buf_puts(b, tmp, (size_t)k);
                } else {
                    buf_putc(b, (char)c);
                }
                break;
        }
    }
    buf_putc(b, '"');
}

void json_buf_key(JsonBuf *b, const char *key) {
    buf_emit_comma_if_needed(b);
    buf_emit_escaped_string(b, key, strlen(key));
    buf_putc(b, ':');
    /* Despues de la clave, el siguiente valor NO debe llevar coma
     * delante — pero el tracking de coma "needed" se refiere a items
     * del objeto. Marcamos que el siguiente valor no fuerza coma
     * directamente: ya emitimos la posible coma antes de la clave. */
    if (b->depth > 0) b->need_comma_stack[b->depth - 1] = false;
}

void json_buf_string(JsonBuf *b, const char *s) {
    json_buf_string_n(b, s, strlen(s));
}

void json_buf_string_n(JsonBuf *b, const char *s, size_t n) {
    /* Tras key:, no emitimos coma; en arrays, si. */
    /* La logica de coma esta en buf_emit_comma_if_needed: solo emite
     * coma si need_comma_stack[depth-1] es true. Tras key se resetea
     * a false, asi que aqui no se emite coma. En arrays el primer
     * item tambien tiene need_comma=false. */
    buf_emit_comma_if_needed(b);
    buf_emit_escaped_string(b, s, n);
    if (b->depth > 0) b->need_comma_stack[b->depth - 1] = true;
}

void json_buf_int(JsonBuf *b, long v) {
    buf_emit_comma_if_needed(b);
    char tmp[32];
    int k = snprintf(tmp, sizeof(tmp), "%ld", v);
    if (k > 0) buf_puts(b, tmp, (size_t)k);
    if (b->depth > 0) b->need_comma_stack[b->depth - 1] = true;
}

void json_buf_bool(JsonBuf *b, bool v) {
    buf_emit_comma_if_needed(b);
    if (v) buf_puts(b, "true", 4);
    else buf_puts(b, "false", 5);
    if (b->depth > 0) b->need_comma_stack[b->depth - 1] = true;
}

void json_buf_null(JsonBuf *b) {
    buf_emit_comma_if_needed(b);
    buf_puts(b, "null", 4);
    if (b->depth > 0) b->need_comma_stack[b->depth - 1] = true;
}

void json_buf_raw(JsonBuf *b, const char *s, size_t n) {
    buf_emit_comma_if_needed(b);
    buf_puts(b, s, n);
    if (b->depth > 0) b->need_comma_stack[b->depth - 1] = true;
}
