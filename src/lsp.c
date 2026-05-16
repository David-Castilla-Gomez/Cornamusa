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

static void emitir_publishDiagnostics(const char *uri,
                                        const Warning *avisos, int n,
                                        bool parse_error,
                                        const char *parse_error_msg) {
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
                if (parse_error) {
                    json_buf_obj_start(&b);
                        json_buf_key(&b, "range");
                        json_buf_obj_start(&b);
                            json_buf_key(&b, "start");
                            json_buf_obj_start(&b);
                                json_buf_key(&b, "line"); json_buf_int(&b, 0);
                                json_buf_key(&b, "character"); json_buf_int(&b, 0);
                            json_buf_obj_end(&b);
                            json_buf_key(&b, "end");
                            json_buf_obj_start(&b);
                                json_buf_key(&b, "line"); json_buf_int(&b, 0);
                                json_buf_key(&b, "character"); json_buf_int(&b, 1);
                            json_buf_obj_end(&b);
                        json_buf_obj_end(&b);
                        json_buf_key(&b, "severity"); json_buf_int(&b, 1);  /* Error */
                        json_buf_key(&b, "source");   json_buf_string(&b, "cornamusa");
                        json_buf_key(&b, "code");     json_buf_string(&b, "syntax");
                        json_buf_key(&b, "message");  json_buf_string(&b, parse_error_msg);
                    json_buf_obj_end(&b);
                } else {
                    for (int i = 0; i < n; i++) {
                        const Warning *w = &avisos[i];
                        json_buf_obj_start(&b);
                            json_buf_key(&b, "range");
                            json_buf_obj_start(&b);
                                int lin = w->linea - 1; if (lin < 0) lin = 0;
                                int col = w->columna - 1; if (col < 0) col = 0;
                                json_buf_key(&b, "start");
                                json_buf_obj_start(&b);
                                    json_buf_key(&b, "line"); json_buf_int(&b, lin);
                                    json_buf_key(&b, "character"); json_buf_int(&b, col);
                                json_buf_obj_end(&b);
                                json_buf_key(&b, "end");
                                json_buf_obj_start(&b);
                                    json_buf_key(&b, "line"); json_buf_int(&b, lin);
                                    json_buf_key(&b, "character"); json_buf_int(&b, col + 1);
                                json_buf_obj_end(&b);
                            json_buf_obj_end(&b);
                            json_buf_key(&b, "severity"); json_buf_int(&b, 2);  /* Warning */
                            json_buf_key(&b, "source");   json_buf_string(&b, "cornamusa");
                            json_buf_key(&b, "code");     json_buf_string(&b, linter_tipo_nombre(w->tipo));
                            json_buf_key(&b, "message");  json_buf_string(&b, w->mensaje);
                        json_buf_obj_end(&b);
                    }
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

    int sav = silenciar_stderr();
    int n = 0;
    Sent **sents = parser_parsear_programa(&p, &n);
    restaurar_stderr(sav);

    if (p.tuvo_error) {
        emitir_publishDiagnostics(uri, NULL, 0, true,
            "Error de sintaxis en el archivo. Ejecuta `cornamusa --check` para detalles.");
        arena_destruir(&a);
        return;
    }

    LinterResultado r = linter_analizar(sents, n);
    emitir_publishDiagnostics(uri, r.avisos, r.n, false, NULL);
    linter_resultado_destruir(&r);
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
            json_buf_obj_end(&b);
            json_buf_key(&b, "serverInfo");
            json_buf_obj_start(&b);
                json_buf_key(&b, "name");    json_buf_string(&b, "cornamusa-lsp");
                json_buf_key(&b, "version"); json_buf_string(&b, "1.52.0");
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
        }
        /* Otros metodos: ignorados silenciosamente (LSP permite que el
         * servidor no implemente todo). Para requests deberiamos
         * responder con error, pero por simplicidad los dejamos pasar. */

        json_free(msg);
    }

    docstore_free_all(&store);
    return 0;
}
