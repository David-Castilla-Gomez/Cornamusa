#include "docs.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────
 * Buffer dinamico de salida.
 * ────────────────────────────────────────────────────────────────── */

typedef struct {
    char *datos;
    size_t cuenta;
    size_t capacidad;
} Buf;

static void buf_iniciar(Buf *b) {
    b->datos = NULL;
    b->cuenta = 0;
    b->capacidad = 0;
}

static bool buf_asegurar(Buf *b, size_t extra) {
    size_t necesario = b->cuenta + extra + 1;
    if (necesario <= b->capacidad) return true;
    size_t nueva = b->capacidad ? b->capacidad : 256;
    while (nueva < necesario) nueva *= 2;
    char *nv = (char *)realloc(b->datos, nueva);
    if (!nv) return false;
    b->datos = nv;
    b->capacidad = nueva;
    return true;
}

static bool buf_append(Buf *b, const char *s, size_t n) {
    if (!buf_asegurar(b, n)) return false;
    memcpy(b->datos + b->cuenta, s, n);
    b->cuenta += n;
    b->datos[b->cuenta] = '\0';
    return true;
}

static bool buf_append_cstr(Buf *b, const char *s) {
    return buf_append(b, s, strlen(s));
}

static bool buf_append_char(Buf *b, char c) {
    return buf_append(b, &c, 1);
}

static bool buf_appendf(Buf *b, const char *fmt, ...) {
    char tmp[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if (n < 0) return false;
    return buf_append(b, tmp, (size_t)n);
}

/* ──────────────────────────────────────────────────────────────────
 * Indice de lineas — offset al inicio de cada linea, 1-indexed.
 *
 * idx[i] da el offset (bytes desde inicio de fuente) donde empieza la
 * linea i. Convencion 1-indexed: idx[0] no se usa. idx[1] siempre 0.
 *
 * Permite extraer cualquier linea por su numero AST sin re-scannear.
 * ────────────────────────────────────────────────────────────────── */

typedef struct {
    const char *fuente;
    int *idx;       /* malloc'd */
    int n_lineas;
} IndiceLineas;

static IndiceLineas indice_construir(const char *fuente) {
    IndiceLineas li;
    li.fuente = fuente;
    li.n_lineas = 1;
    for (const char *p = fuente; *p; p++) {
        if (*p == '\n') li.n_lineas++;
    }
    li.idx = (int *)malloc(sizeof(int) * (size_t)(li.n_lineas + 2));
    int linea = 1;
    li.idx[0] = -1;
    li.idx[1] = 0;
    for (const char *p = fuente; *p; p++) {
        if (*p == '\n') {
            linea++;
            li.idx[linea] = (int)(p - fuente) + 1;
        }
    }
    return li;
}

static void indice_destruir(IndiceLineas *li) {
    free(li->idx);
    li->idx = NULL;
}

/* Devuelve longitud de la linea (sin '\n'/'\r' finales). */
static int linea_longitud(const IndiceLineas *li, int linea) {
    if (linea < 1 || linea > li->n_lineas) return 0;
    int ini = li->idx[linea];
    int fin = ini;
    while (li->fuente[fin] && li->fuente[fin] != '\n') fin++;
    /* Quitar \r al final si lo hay. */
    if (fin > ini && li->fuente[fin - 1] == '\r') fin--;
    return fin - ini;
}

/* True si la linea consiste solamente de un comentario `# ...`
 * (posiblemente con whitespace al inicio). Linea vacia => false. */
static bool linea_es_comentario(const IndiceLineas *li, int linea) {
    int ini = li->idx[linea];
    int len = linea_longitud(li, linea);
    int i = 0;
    while (i < len && (li->fuente[ini + i] == ' '
                       || li->fuente[ini + i] == '\t')) {
        i++;
    }
    if (i >= len) return false;
    return li->fuente[ini + i] == '#';
}

/* True si la linea esta vacia (solo whitespace o '\0'). */
static bool linea_esta_vacia(const IndiceLineas *li, int linea) {
    int ini = li->idx[linea];
    int len = linea_longitud(li, linea);
    for (int i = 0; i < len; i++) {
        char c = li->fuente[ini + i];
        if (c != ' ' && c != '\t') return false;
    }
    return true;
}

/* Extrae el contenido tras `#` de una linea de comentario y, si lo
 * hay, quita UN espacio inicial. El resultado se appendiza al `buf`
 * con un '\n' final.
 *
 * "  # hola"     -> "hola\n"
 * "# hola"       -> "hola\n"
 * "#hola"        -> "hola\n"
 * "  #   x"      -> "  x\n"  (solo se come un espacio tras `#`)
 */
static void append_contenido_comentario(Buf *out, const IndiceLineas *li,
                                          int linea) {
    int ini = li->idx[linea];
    int len = linea_longitud(li, linea);
    int i = 0;
    while (i < len && (li->fuente[ini + i] == ' '
                       || li->fuente[ini + i] == '\t')) {
        i++;
    }
    if (i >= len || li->fuente[ini + i] != '#') return;
    i++;  /* saltar '#' */
    if (i < len && li->fuente[ini + i] == ' ') i++;
    if (i < len) buf_append(out, li->fuente + ini + i, (size_t)(len - i));
    buf_append_char(out, '\n');
}

/* ──────────────────────────────────────────────────────────────────
 * Extraccion de docstrings.
 * ────────────────────────────────────────────────────────────────── */

/* Doc del modulo: bloque de comentarios contiguos al inicio del
 * archivo, antes de la primera sentencia. Una linea en blanco DENTRO
 * del bloque termina el doc. */
static char *extraer_doc_modulo(const IndiceLineas *li) {
    Buf b;
    buf_iniciar(&b);
    int linea = 1;
    while (linea <= li->n_lineas) {
        if (linea_es_comentario(li, linea)) {
            append_contenido_comentario(&b, li, linea);
            linea++;
        } else {
            break;
        }
    }
    if (b.cuenta == 0) {
        free(b.datos);
        return NULL;
    }
    return b.datos;
}

/* Doc de un item: bloque de comentarios consecutivos inmediatamente
 * anteriores a `linea_decl` (sin linea en blanco entre ellos y el
 * declarador). Devuelve cadena malloc'd o NULL si no hay doc. */
static char *extraer_doc_antes(const IndiceLineas *li, int linea_decl) {
    if (linea_decl <= 1) return NULL;

    int linea = linea_decl - 1;
    if (!linea_es_comentario(li, linea)) return NULL;

    /* Subir mientras sean comentarios contiguos. */
    int top = linea;
    while (top > 1 && linea_es_comentario(li, top - 1)) {
        top--;
    }

    Buf b;
    buf_iniciar(&b);
    for (int l = top; l <= linea; l++) {
        append_contenido_comentario(&b, li, l);
    }
    if (b.cuenta == 0) {
        free(b.datos);
        return NULL;
    }
    return b.datos;
}

/* ──────────────────────────────────────────────────────────────────
 * Sintetizar firma de funcion / metodo.
 *
 * Produce algo como: `nombre(a, b=1, *args, **kw)`
 * El AST no preserva la expresion del default — emitimos `=...`
 * generico para indicar que hay default sin reconstruir la expresion.
 * ────────────────────────────────────────────────────────────────── */

static void emitir_firma(Buf *out, const char *nombre, int longitud_nombre,
                          Parametro *params, int n) {
    buf_append(out, nombre, (size_t)longitud_nombre);
    buf_append_char(out, '(');
    for (int i = 0; i < n; i++) {
        if (i > 0) buf_append_cstr(out, ", ");
        if (params[i].es_estrella) buf_append_char(out, '*');
        if (params[i].es_doble_estrella) buf_append_cstr(out, "**");
        buf_append(out, params[i].nombre, (size_t)params[i].longitud_nombre);
        if (params[i].valor_defecto) buf_append_cstr(out, "=...");
    }
    buf_append_char(out, ')');
}

/* ──────────────────────────────────────────────────────────────────
 * Emitir un metodo (subseccion de clase).
 * ────────────────────────────────────────────────────────────────── */

static void emitir_metodo(Buf *out, const IndiceLineas *li, Sent *s) {
    if (s->tipo != SENT_FUNCION) return;
    buf_append_cstr(out, "### `");
    emitir_firma(out, s->como.funcion.nombre, s->como.funcion.longitud_nombre,
                  s->como.funcion.parametros, s->como.funcion.n_parametros);
    buf_append_cstr(out, "`\n\n");
    char *doc = extraer_doc_antes(li, s->linea);
    if (doc) {
        buf_append_cstr(out, doc);
        buf_append_char(out, '\n');
        free(doc);
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Emitir top-level item.
 * ────────────────────────────────────────────────────────────────── */

static void emitir_funcion_top(Buf *out, const IndiceLineas *li, Sent *s) {
    buf_append_cstr(out, "## `");
    emitir_firma(out, s->como.funcion.nombre, s->como.funcion.longitud_nombre,
                  s->como.funcion.parametros, s->como.funcion.n_parametros);
    buf_append_cstr(out, "`\n\n");
    char *doc = extraer_doc_antes(li, s->linea);
    if (doc) {
        buf_append_cstr(out, doc);
        buf_append_char(out, '\n');
        free(doc);
    }
}

static void emitir_clase_top(Buf *out, const IndiceLineas *li, Sent *s) {
    buf_append_cstr(out, "## clase `");
    buf_append(out, s->como.clase.nombre, (size_t)s->como.clase.longitud_nombre);
    if (s->como.clase.n_superclases > 0) {
        /* No reproducimos las expresiones de superclase exactas — solo
         * indicamos que extiende algo. Para v1, lo basico basta. */
        buf_append_cstr(out, " extiende ...");
    }
    buf_append_cstr(out, "`\n\n");

    char *doc = extraer_doc_antes(li, s->linea);
    if (doc) {
        buf_append_cstr(out, doc);
        buf_append_char(out, '\n');
        free(doc);
    }

    /* Recorrer el cuerpo emitiendo metodos. El cuerpo es SENT_BLOQUE. */
    Sent *cuerpo = s->como.clase.cuerpo;
    if (cuerpo && cuerpo->tipo == SENT_BLOQUE) {
        for (int i = 0; i < cuerpo->como.bloque.n_sentencias; i++) {
            emitir_metodo(out, li, cuerpo->como.bloque.sentencias[i]);
        }
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Top-level orchestrator.
 * ────────────────────────────────────────────────────────────────── */

DocsResultado docs_generar(const char *fuente,
                            const char *nombre_modulo,
                            Sent **sents, int n) {
    DocsResultado r = {0};
    Buf out;
    buf_iniciar(&out);

    IndiceLineas li = indice_construir(fuente);

    /* H1 = nombre del modulo. */
    if (nombre_modulo && *nombre_modulo) {
        buf_appendf(&out, "# %s\n\n", nombre_modulo);
    }

    char *doc_modulo = extraer_doc_modulo(&li);
    if (doc_modulo) {
        buf_append_cstr(&out, doc_modulo);
        buf_append_char(&out, '\n');
        free(doc_modulo);
    }

    for (int i = 0; i < n; i++) {
        Sent *s = sents[i];
        if (!s) continue;
        switch (s->tipo) {
            case SENT_FUNCION:
                emitir_funcion_top(&out, &li, s);
                break;
            case SENT_CLASE:
                emitir_clase_top(&out, &li, s);
                break;
            default:
                /* Asignaciones top-level, importaciones, etc. se ignoran
                 * en v1.51. Constantes y reexports son scope para v1.52. */
                break;
        }
    }

    indice_destruir(&li);

    if (out.datos == NULL) {
        out.datos = (char *)malloc(1);
        if (out.datos) out.datos[0] = '\0';
        out.cuenta = 0;
    }
    r.markdown = out.datos;
    r.longitud = out.cuenta;
    return r;
}

void docs_resultado_destruir(DocsResultado *r) {
    if (!r) return;
    free(r->markdown);
    r->markdown = NULL;
    free(r->mensaje_error);
    r->mensaje_error = NULL;
    r->longitud = 0;
}
