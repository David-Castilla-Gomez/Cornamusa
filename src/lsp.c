#include "lsp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "json_min.h"
#include "lexer.h"
#include "linter.h"
#include "parser.h"

/* ──────────────────────────────────────────────────────────────────
 * Document store: URI → texto.
 * ────────────────────────────────────────────────────────────────── */

typedef struct {
    char *uri;       /* malloc'd */
    char *text;      /* malloc'd; tamano = strlen(text) */
} OpenDoc;

#define MAX_OPEN_DOCS 64

typedef struct {
    OpenDoc docs[MAX_OPEN_DOCS];
    int n;
} DocStore;

static int docstore_find(DocStore *s, const char *uri) {
    for (int i = 0; i < s->n; i++) {
        if (strcmp(s->docs[i].uri, uri) == 0) return i;
    }
    return -1;
}

static void docstore_set(DocStore *s, const char *uri, const char *text) {
    int idx = docstore_find(s, uri);
    if (idx < 0) {
        if (s->n >= MAX_OPEN_DOCS) {
            /* Silenciosamente descartamos. En la practica no se llega. */
            return;
        }
        idx = s->n++;
        s->docs[idx].uri = NULL;
        s->docs[idx].text = NULL;
    }
    free(s->docs[idx].uri);
    free(s->docs[idx].text);
    s->docs[idx].uri = strdup(uri);
    s->docs[idx].text = strdup(text);
}

static void docstore_remove(DocStore *s, const char *uri) {
    int idx = docstore_find(s, uri);
    if (idx < 0) return;
    free(s->docs[idx].uri);
    free(s->docs[idx].text);
    /* compactar */
    for (int i = idx; i < s->n - 1; i++) s->docs[i] = s->docs[i + 1];
    s->n--;
}

static void docstore_free_all(DocStore *s) {
    for (int i = 0; i < s->n; i++) {
        free(s->docs[i].uri);
        free(s->docs[i].text);
    }
    s->n = 0;
}

/* ──────────────────────────────────────────────────────────────────
 * Framing JSON-RPC.
 * ────────────────────────────────────────────────────────────────── */

/* Lee la siguiente cabecera Content-Length. Devuelve longitud del
 * body o -1 si EOF. */
static long leer_content_length(void) {
    long content_length = -1;
    char linea[256];
    for (;;) {
        int i = 0;
        int c;
        while ((c = fgetc(stdin)) != EOF && c != '\n') {
            if (i + 1 < (int)sizeof(linea)) linea[i++] = (char)c;
        }
        if (c == EOF && i == 0) return -1;
        linea[i] = '\0';
        /* Strip \r al final si lo hay. */
        if (i > 0 && linea[i - 1] == '\r') linea[--i] = '\0';
        if (i == 0) {
            /* Linea en blanco → fin de cabeceras. */
            return content_length;
        }
        if (strncmp(linea, "Content-Length:", 15) == 0) {
            content_length = strtol(linea + 15, NULL, 10);
        }
        /* Otras cabeceras (Content-Type, etc.) las ignoramos. */
    }
}

/* Lee `n` bytes desde stdin a un buffer reservado. Devuelve NULL si
 * fallo. El llamador libera. */
static char *leer_body(long n) {
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    long leido = 0;
    while (leido < n) {
        size_t got = fread(buf + leido, 1, (size_t)(n - leido), stdin);
        if (got == 0) {
            free(buf);
            return NULL;
        }
        leido += (long)got;
    }
    buf[n] = '\0';
    return buf;
}

static void enviar_mensaje(const char *body, size_t len) {
    fprintf(stdout, "Content-Length: %zu\r\n\r\n", len);
    fwrite(body, 1, len, stdout);
    fflush(stdout);
}

/* ──────────────────────────────────────────────────────────────────
 * Diagnostics: convertir Warnings del linter a LSP.
 *
 * LSP severities: 1=Error, 2=Warning, 3=Info, 4=Hint.
 * Nosotros emitimos todos como 2=Warning.
 *
 * Linter usa lineas/columnas 1-indexed. LSP usa 0-indexed.
 * ────────────────────────────────────────────────────────────────── */

/* Emite un solo diagnostic dentro del array. Helper interno. */
static void emit_diag_obj(JsonBuf *b, int linea_1, int columna_1,
                            int severity, const char *code, const char *msg) {
    int lin = linea_1 - 1; if (lin < 0) lin = 0;
    int col = columna_1 - 1; if (col < 0) col = 0;
    json_buf_obj_start(b);
        json_buf_key(b, "range");
        json_buf_obj_start(b);
            json_buf_key(b, "start");
            json_buf_obj_start(b);
                json_buf_key(b, "line"); json_buf_int(b, lin);
                json_buf_key(b, "character"); json_buf_int(b, col);
            json_buf_obj_end(b);
            json_buf_key(b, "end");
            json_buf_obj_start(b);
                json_buf_key(b, "line"); json_buf_int(b, lin);
                json_buf_key(b, "character"); json_buf_int(b, col + 1);
            json_buf_obj_end(b);
        json_buf_obj_end(b);
        json_buf_key(b, "severity"); json_buf_int(b, severity);
        json_buf_key(b, "source");   json_buf_string(b, "cornamusa");
        json_buf_key(b, "code");     json_buf_string(b, code);
        json_buf_key(b, "message");  json_buf_string(b, msg);
    json_buf_obj_end(b);
}

static void emitir_publishDiagnostics(const char *uri,
                                        const Warning *avisos, int n_warn,
                                        const ErrorParser *parse_errs, int n_perr) {
    JsonBuf b;
    json_buf_init(&b);
    json_buf_obj_start(&b);
        json_buf_key(&b, "jsonrpc"); json_buf_string(&b, "2.0");
        json_buf_key(&b, "method");  json_buf_string(&b, "textDocument/publishDiagnostics");
        json_buf_key(&b, "params");
        json_buf_obj_start(&b);
            json_buf_key(&b, "uri"); json_buf_string(&b, uri);
            json_buf_key(&b, "diagnostics");
            json_buf_arr_start(&b);
                /* Parse errors (severity 1) primero. */
                for (int i = 0; i < n_perr; i++) {
                    emit_diag_obj(&b, parse_errs[i].linea, parse_errs[i].columna,
                                    1, "syntax", parse_errs[i].mensaje);
                }
                /* Luego warnings del linter (severity 2). */
                for (int i = 0; i < n_warn; i++) {
                    const Warning *w = &avisos[i];
                    emit_diag_obj(&b, w->linea, w->columna, 2,
                                    linter_tipo_nombre(w->tipo), w->mensaje);
                }
            json_buf_arr_end(&b);
        json_buf_obj_end(&b);
    json_buf_obj_end(&b);

    if (b.data) enviar_mensaje(b.data, b.len);
    json_buf_free(&b);
}

/* Suprime stderr durante el parse para que los mensajes del parser no
 * escapen al stdout LSP. */
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
static int silenciar_stderr(void) {
    fflush(stderr);
    int saved = _dup(_fileno(stderr));
    FILE *devnull = fopen("nul", "w");
    if (devnull) _dup2(_fileno(devnull), _fileno(stderr));
    if (devnull) fclose(devnull);
    return saved;
}
static void restaurar_stderr(int saved) {
    fflush(stderr);
    _dup2(saved, _fileno(stderr));
    _close(saved);
}
#else
#include <unistd.h>
static int silenciar_stderr(void) {
    fflush(stderr);
    int saved = dup(STDERR_FILENO);
    int devnull = open("/dev/null", 1);
    if (devnull >= 0) {
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }
    return saved;
}
static void restaurar_stderr(int saved) {
    fflush(stderr);
    dup2(saved, STDERR_FILENO);
    close(saved);
}
#endif

static void diagnose_doc(const char *uri, const char *text) {
    Lexer l;
    lexer_iniciar(&l, text, uri);
    Arena a;
    arena_iniciar(&a, 16384);
    Parser p;
    parser_iniciar(&p, &l, &a, text, uri);

    /* v1.53: capturar errores del parser como datos en lugar de
     * dejar que los imprima a stderr. */
    ErroresParser perrs = {0};
    p.capturar_errores = true;
    p.errores_capturados = &perrs;

    int n = 0;
    Sent **sents = parser_parsear_programa(&p, &n);

    if (p.tuvo_error) {
        emitir_publishDiagnostics(uri, NULL, 0, perrs.items, perrs.n);
        parser_errores_liberar(&perrs);
        arena_destruir(&a);
        return;
    }

    LinterResultado r = linter_analizar(sents, n);
    emitir_publishDiagnostics(uri, r.avisos, r.n, NULL, 0);
    linter_resultado_destruir(&r);
    parser_errores_liberar(&perrs);
    arena_destruir(&a);
}

/* ──────────────────────────────────────────────────────────────────
 * Responses.
 * ────────────────────────────────────────────────────────────────── */

static void responder_initialize(long id) {
    JsonBuf b;
    json_buf_init(&b);
    json_buf_obj_start(&b);
        json_buf_key(&b, "jsonrpc"); json_buf_string(&b, "2.0");
        json_buf_key(&b, "id");      json_buf_int(&b, id);
        json_buf_key(&b, "result");
        json_buf_obj_start(&b);
            json_buf_key(&b, "capabilities");
            json_buf_obj_start(&b);
                /* textDocumentSync: 1 = Full (re-envia el documento completo en cada cambio) */
                json_buf_key(&b, "textDocumentSync"); json_buf_int(&b, 1);
                /* v1.53: hover sobre identificadores top-level. */
                json_buf_key(&b, "hoverProvider"); json_buf_bool(&b, true);
            json_buf_obj_end(&b);
            json_buf_key(&b, "serverInfo");
            json_buf_obj_start(&b);
                json_buf_key(&b, "name");    json_buf_string(&b, "cornamusa-lsp");
                json_buf_key(&b, "version"); json_buf_string(&b, "1.53.0");
            json_buf_obj_end(&b);
        json_buf_obj_end(&b);
    json_buf_obj_end(&b);
    if (b.data) enviar_mensaje(b.data, b.len);
    json_buf_free(&b);
}

static void responder_shutdown(long id) {
    JsonBuf b;
    json_buf_init(&b);
    json_buf_obj_start(&b);
        json_buf_key(&b, "jsonrpc"); json_buf_string(&b, "2.0");
        json_buf_key(&b, "id");      json_buf_int(&b, id);
        json_buf_key(&b, "result");  json_buf_null(&b);
    json_buf_obj_end(&b);
    if (b.data) enviar_mensaje(b.data, b.len);
    json_buf_free(&b);
}

/* ──────────────────────────────────────────────────────────────────
 * Dispatch.
 * ────────────────────────────────────────────────────────────────── */

static void manejar_didOpen(DocStore *store, JsonValue *params) {
    JsonValue *td = json_obj_get(params, "textDocument");
    if (!td) return;
    const char *uri  = json_string(json_obj_get(td, "uri"));
    const char *text = json_string(json_obj_get(td, "text"));
    if (!uri[0]) return;
    docstore_set(store, uri, text);
    diagnose_doc(uri, text);
}

static void manejar_didChange(DocStore *store, JsonValue *params) {
    JsonValue *td = json_obj_get(params, "textDocument");
    if (!td) return;
    const char *uri = json_string(json_obj_get(td, "uri"));
    if (!uri[0]) return;

    /* Sincronizacion completa: tomamos el ultimo elemento del array
     * contentChanges y usamos su `.text` como el documento completo. */
    JsonValue *changes = json_obj_get(params, "contentChanges");
    if (!changes || changes->type != JV_ARRAY || changes->as.arr.n == 0) return;

    JsonValue *ult = changes->as.arr.items[changes->as.arr.n - 1];
    const char *text = json_string(json_obj_get(ult, "text"));
    docstore_set(store, uri, text);
    diagnose_doc(uri, text);
}

static void manejar_didClose(DocStore *store, JsonValue *params) {
    JsonValue *td = json_obj_get(params, "textDocument");
    if (!td) return;
    const char *uri = json_string(json_obj_get(td, "uri"));
    if (!uri[0]) return;
    /* Limpiar diagnostics. */
    emitir_publishDiagnostics(uri, NULL, 0, false, NULL);
    docstore_remove(store, uri);
}

/* ──────────────────────────────────────────────────────────────────
 * Hover (v1.53).
 *
 * Extrae la palabra bajo el cursor del texto del documento y busca
 * un simbolo top-level (funcion o clase) con ese nombre. Si lo
 * encuentra, responde con su firma + los comentarios `#` precedentes
 * como Markdown.
 *
 * Nota sobre coordenadas: LSP usa UTF-16 code units para `character`.
 * Para identificadores ASCII coincide con bytes. Para UTF-8 multibyte
 * el mapeo es aproximado — suficiente para el MVP.
 * ────────────────────────────────────────────────────────────────── */

static bool es_ident_char_byte(unsigned char c) {
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    if (c == '_') return true;
    if (c >= 0x80) return true;  /* continuamos por UTF-8 multibyte */
    return false;
}

/* Convierte (linea, columna) 0-indexed a offset en bytes dentro del
 * texto. Si la posicion esta fuera de rango devuelve el ultimo byte. */
static size_t pos_a_offset(const char *texto, int linea, int columna) {
    size_t i = 0;
    int l = 0, c = 0;
    while (texto[i]) {
        if (l == linea && c == columna) return i;
        if (texto[i] == '\n') { l++; c = 0; }
        else c++;
        i++;
    }
    return i;
}

/* Encuentra la palabra bajo el offset. Devuelve true si hay palabra,
 * con `out_inicio`/`out_longitud` apuntando al lexema dentro de
 * `texto`. Si el offset no esta en una palabra, devuelve false. */
static bool extraer_palabra(const char *texto, size_t offset,
                              const char **out_inicio, size_t *out_longitud) {
    /* Si el offset apunta a un caracter no-ident, intentar el anterior
     * (cursor "after" el ident funciona). */
    if (offset > 0 && (!texto[offset] || !es_ident_char_byte((unsigned char)texto[offset]))) {
        if (es_ident_char_byte((unsigned char)texto[offset - 1])) offset--;
        else return false;
    }
    if (!es_ident_char_byte((unsigned char)texto[offset])) return false;

    size_t ini = offset;
    while (ini > 0 && es_ident_char_byte((unsigned char)texto[ini - 1])) ini--;
    size_t fin = offset;
    while (texto[fin] && es_ident_char_byte((unsigned char)texto[fin])) fin++;

    *out_inicio = texto + ini;
    *out_longitud = fin - ini;
    return *out_longitud > 0;
}

/* Extrae comentarios `#` consecutivos inmediatamente anteriores a la
 * linea `linea_decl` (1-indexed). Devuelve cadena malloc'd o NULL.
 * Logica equivalente a `extraer_doc_antes` de docs.c pero standalone. */
static char *doc_antes_de_linea(const char *texto, int linea_decl) {
    if (linea_decl <= 1) return NULL;

    /* Construir un indice de offsets de lineas. */
    int n_lineas = 1;
    for (const char *p = texto; *p; p++) if (*p == '\n') n_lineas++;
    if (linea_decl > n_lineas) return NULL;

    int *idx = (int *)malloc(sizeof(int) * (size_t)(n_lineas + 2));
    if (!idx) return NULL;
    idx[1] = 0;
    int l = 1;
    for (size_t i = 0; texto[i]; i++) {
        if (texto[i] == '\n') {
            l++;
            idx[l] = (int)(i + 1);
        }
    }

    /* Test si linea `n` es un comentario (posible whitespace + '#'). */
    int top = linea_decl - 1;
    while (top >= 1) {
        int p = idx[top];
        while (texto[p] == ' ' || texto[p] == '\t') p++;
        if (texto[p] != '#') break;
        top--;
    }
    top++;

    if (top >= linea_decl) {
        free(idx);
        return NULL;
    }

    /* Concatenar contenido (sin `#` y un espacio opcional). */
    size_t cap = 256, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) { free(idx); return NULL; }

    for (int li = top; li < linea_decl; li++) {
        int p = idx[li];
        while (texto[p] == ' ' || texto[p] == '\t') p++;
        if (texto[p] != '#') continue;
        p++;
        if (texto[p] == ' ') p++;
        int q = p;
        while (texto[q] && texto[q] != '\n') q++;
        int linelen = q - p;
        if (len + (size_t)linelen + 2 >= cap) {
            cap = (len + (size_t)linelen + 2) * 2;
            char *nv = (char *)realloc(buf, cap);
            if (!nv) { free(buf); free(idx); return NULL; }
            buf = nv;
        }
        memcpy(buf + len, texto + p, (size_t)linelen);
        len += (size_t)linelen;
        buf[len++] = '\n';
    }
    free(idx);
    if (len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

/* Construye el markdown de un hover para una funcion: firma + doc. */
static char *hover_markdown_funcion(Sent *s, const char *texto) {
    JsonBuf b;  /* reuso JsonBuf como string builder */
    /* En realidad no es JSON aqui — solo necesito un buffer. Pero su
     * API funciona. */
    json_buf_init(&b);

    /* Firma: ```cornamusa\nfuncion nombre(args)\n``` */
    const char *prefix = "```cornamusa\nfuncion ";
    json_buf_raw(&b, prefix, strlen(prefix));
    json_buf_raw(&b, s->como.funcion.nombre, (size_t)s->como.funcion.longitud_nombre);
    json_buf_raw(&b, "(", 1);
    for (int i = 0; i < s->como.funcion.n_parametros; i++) {
        if (i > 0) json_buf_raw(&b, ", ", 2);
        Parametro *p = &s->como.funcion.parametros[i];
        if (p->es_estrella)        json_buf_raw(&b, "*", 1);
        if (p->es_doble_estrella)  json_buf_raw(&b, "**", 2);
        json_buf_raw(&b, p->nombre, (size_t)p->longitud_nombre);
        if (p->valor_defecto)      json_buf_raw(&b, "=...", 4);
    }
    json_buf_raw(&b, ")\n```\n\n", 7);

    char *doc = doc_antes_de_linea(texto, s->linea);
    if (doc) {
        json_buf_raw(&b, doc, strlen(doc));
        free(doc);
    }

    char *out = b.data ? strdup(b.data) : strdup("");
    json_buf_free(&b);
    return out;
}

static char *hover_markdown_clase(Sent *s, const char *texto) {
    JsonBuf b;
    json_buf_init(&b);

    json_buf_raw(&b, "```cornamusa\nclase ", strlen("```cornamusa\nclase "));
    json_buf_raw(&b, s->como.clase.nombre, (size_t)s->como.clase.longitud_nombre);
    if (s->como.clase.n_superclases > 0) {
        json_buf_raw(&b, " extiende ...", strlen(" extiende ..."));
    }
    json_buf_raw(&b, "\n```\n\n", 6);

    char *doc = doc_antes_de_linea(texto, s->linea);
    if (doc) {
        json_buf_raw(&b, doc, strlen(doc));
        free(doc);
    }

    /* Listar metodos. */
    Sent *cuerpo = s->como.clase.cuerpo;
    if (cuerpo && cuerpo->tipo == SENT_BLOQUE && cuerpo->como.bloque.n_sentencias > 0) {
        json_buf_raw(&b, "\n**Metodos:**\n", strlen("\n**Metodos:**\n"));
        for (int i = 0; i < cuerpo->como.bloque.n_sentencias; i++) {
            Sent *m = cuerpo->como.bloque.sentencias[i];
            if (!m || m->tipo != SENT_FUNCION) continue;
            json_buf_raw(&b, "- `", 3);
            json_buf_raw(&b, m->como.funcion.nombre,
                          (size_t)m->como.funcion.longitud_nombre);
            json_buf_raw(&b, "(", 1);
            for (int j = 0; j < m->como.funcion.n_parametros; j++) {
                if (j > 0) json_buf_raw(&b, ", ", 2);
                Parametro *p = &m->como.funcion.parametros[j];
                if (p->es_estrella) json_buf_raw(&b, "*", 1);
                if (p->es_doble_estrella) json_buf_raw(&b, "**", 2);
                json_buf_raw(&b, p->nombre, (size_t)p->longitud_nombre);
            }
            json_buf_raw(&b, ")`\n", 3);
        }
    }

    char *out = b.data ? strdup(b.data) : strdup("");
    json_buf_free(&b);
    return out;
}

/* Busca un simbolo top-level por nombre en el AST. Devuelve la Sent
 * (SENT_FUNCION o SENT_CLASE) o NULL si no se encuentra. */
static Sent *buscar_top_level(Sent **sents, int n,
                                const char *nombre, size_t longitud) {
    for (int i = 0; i < n; i++) {
        Sent *s = sents[i];
        if (!s) continue;
        if (s->tipo == SENT_FUNCION) {
            if (s->como.funcion.longitud_nombre == (int)longitud
                && memcmp(s->como.funcion.nombre, nombre, longitud) == 0) {
                return s;
            }
        } else if (s->tipo == SENT_CLASE) {
            if (s->como.clase.longitud_nombre == (int)longitud
                && memcmp(s->como.clase.nombre, nombre, longitud) == 0) {
                return s;
            }
        }
    }
    return NULL;
}

static void responder_hover(DocStore *store, long id, JsonValue *params) {
    /* result: null si no hay hover. */
    JsonValue *td = json_obj_get(params, "textDocument");
    JsonValue *pos = json_obj_get(params, "position");
    if (!td || !pos) {
        JsonBuf b; json_buf_init(&b);
        json_buf_obj_start(&b);
            json_buf_key(&b, "jsonrpc"); json_buf_string(&b, "2.0");
            json_buf_key(&b, "id");      json_buf_int(&b, id);
            json_buf_key(&b, "result");  json_buf_null(&b);
        json_buf_obj_end(&b);
        if (b.data) enviar_mensaje(b.data, b.len);
        json_buf_free(&b);
        return;
    }
    const char *uri = json_string(json_obj_get(td, "uri"));
    int linea  = (int)json_int(json_obj_get(pos, "line"));
    int columna = (int)json_int(json_obj_get(pos, "character"));

    int idx = docstore_find(store, uri);
    if (idx < 0) {
        JsonBuf b; json_buf_init(&b);
        json_buf_obj_start(&b);
            json_buf_key(&b, "jsonrpc"); json_buf_string(&b, "2.0");
            json_buf_key(&b, "id");      json_buf_int(&b, id);
            json_buf_key(&b, "result");  json_buf_null(&b);
        json_buf_obj_end(&b);
        if (b.data) enviar_mensaje(b.data, b.len);
        json_buf_free(&b);
        return;
    }
    const char *texto = store->docs[idx].text;

    size_t off = pos_a_offset(texto, linea, columna);
    const char *palabra;
    size_t plen;
    bool ok = extraer_palabra(texto, off, &palabra, &plen);

    char *contenido = NULL;
    if (ok) {
        /* Parsear el documento para buscar el simbolo. */
        Lexer l;
        lexer_iniciar(&l, texto, uri);
        Arena a;
        arena_iniciar(&a, 16384);
        Parser p;
        parser_iniciar(&p, &l, &a, texto, uri);
        ErroresParser perrs = {0};
        p.capturar_errores = true;
        p.errores_capturados = &perrs;
        int n = 0;
        Sent **sents = parser_parsear_programa(&p, &n);

        if (!p.tuvo_error) {
            Sent *s = buscar_top_level(sents, n, palabra, plen);
            if (s && s->tipo == SENT_FUNCION) {
                contenido = hover_markdown_funcion(s, texto);
            } else if (s && s->tipo == SENT_CLASE) {
                contenido = hover_markdown_clase(s, texto);
            }
        }
        parser_errores_liberar(&perrs);
        arena_destruir(&a);
    }

    JsonBuf b; json_buf_init(&b);
    json_buf_obj_start(&b);
        json_buf_key(&b, "jsonrpc"); json_buf_string(&b, "2.0");
        json_buf_key(&b, "id");      json_buf_int(&b, id);
        json_buf_key(&b, "result");
        if (contenido && *contenido) {
            json_buf_obj_start(&b);
                json_buf_key(&b, "contents");
                json_buf_obj_start(&b);
                    json_buf_key(&b, "kind");  json_buf_string(&b, "markdown");
                    json_buf_key(&b, "value"); json_buf_string(&b, contenido);
                json_buf_obj_end(&b);
            json_buf_obj_end(&b);
        } else {
            json_buf_null(&b);
        }
    json_buf_obj_end(&b);
    if (b.data) enviar_mensaje(b.data, b.len);
    json_buf_free(&b);
    free(contenido);
}

int lsp_run(void) {
    DocStore store = { .n = 0 };
    bool shutdown_recibido = false;

    for (;;) {
        long len = leer_content_length();
        if (len < 0) break;  /* EOF */
        if (len == 0) continue;

        char *body = leer_body(len);
        if (!body) break;

        JsonValue *msg = json_parse(body, (size_t)len);
        free(body);
        if (!msg) continue;

        JsonValue *method_v = json_obj_get(msg, "method");
        JsonValue *id_v     = json_obj_get(msg, "id");
        JsonValue *params_v = json_obj_get(msg, "params");

        const char *method = json_string(method_v);
        bool tiene_id = (id_v != NULL && id_v->type == JV_NUMBER);
        long id = tiene_id ? json_int(id_v) : 0;

        if (strcmp(method, "initialize") == 0) {
            if (tiene_id) responder_initialize(id);
        } else if (strcmp(method, "initialized") == 0) {
            /* Notification, no response. */
        } else if (strcmp(method, "shutdown") == 0) {
            shutdown_recibido = true;
            if (tiene_id) responder_shutdown(id);
        } else if (strcmp(method, "exit") == 0) {
            json_free(msg);
            docstore_free_all(&store);
            return shutdown_recibido ? 0 : 1;
        } else if (strcmp(method, "textDocument/didOpen") == 0) {
            manejar_didOpen(&store, params_v);
        } else if (strcmp(method, "textDocument/didChange") == 0) {
            manejar_didChange(&store, params_v);
        } else if (strcmp(method, "textDocument/didClose") == 0) {
            manejar_didClose(&store, params_v);
        } else if (strcmp(method, "textDocument/hover") == 0) {
            if (tiene_id) responder_hover(&store, id, params_v);
        }
        /* Otros metodos: ignorados silenciosamente (LSP permite que el
         * servidor no implemente todo). Para requests deberiamos
         * responder con error, pero por simplicidad los dejamos pasar. */

        json_free(msg);
    }

    docstore_free_all(&store);
    return 0;
}
