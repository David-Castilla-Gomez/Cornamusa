#include "formateador.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INDENT_ANCHO 4

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
    size_t nueva_cap = b->capacidad ? b->capacidad : 256;
    while (nueva_cap < necesario) nueva_cap *= 2;
    char *nuevo = (char *)realloc(b->datos, nueva_cap);
    if (!nuevo) return false;
    b->datos = nuevo;
    b->capacidad = nueva_cap;
    return true;
}

static bool buf_append(Buf *b, const char *s, size_t n) {
    if (!buf_asegurar(b, n)) return false;
    memcpy(b->datos + b->cuenta, s, n);
    b->cuenta += n;
    b->datos[b->cuenta] = '\0';
    return true;
}

static bool buf_append_char(Buf *b, char c) {
    return buf_append(b, &c, 1);
}

static bool buf_append_espacios(Buf *b, int n) {
    if (n <= 0) return true;
    if (!buf_asegurar(b, (size_t)n)) return false;
    memset(b->datos + b->cuenta, ' ', (size_t)n);
    b->cuenta += (size_t)n;
    b->datos[b->cuenta] = '\0';
    return true;
}

/* ──────────────────────────────────────────────────────────────────
 * Utilidades de linea.
 * ────────────────────────────────────────────────────────────────── */

/* Devuelve el offset (en `linea[..len)`) sin trailing ' '/'\t'/'\r'.
 * El llamador pasa `len` sin incluir el '\n' final si lo hubiera. */
static size_t trim_trailing(const char *linea, size_t len) {
    size_t fin = len;
    while (fin > 0 && (linea[fin - 1] == ' '
                       || linea[fin - 1] == '\t'
                       || linea[fin - 1] == '\r')) {
        fin--;
    }
    return fin;
}

/* Offset del primer caracter no-whitespace (' ', '\t'). */
static size_t skip_leading_ws(const char *linea, size_t fin) {
    size_t i = 0;
    while (i < fin && (linea[i] == ' ' || linea[i] == '\t')) i++;
    return i;
}

/* True si linea[inicio..fin) empieza por `palabra` seguido por un
 * delimitador (whitespace, ':', fin de linea). Util para detectar
 * keywords iniciales como `fin`, `sino`, `cuando`, etc. */
static bool empieza_por(const char *linea, size_t inicio, size_t fin,
                         const char *palabra) {
    size_t plen = strlen(palabra);
    if (inicio + plen > fin) return false;
    if (memcmp(linea + inicio, palabra, plen) != 0) return false;
    if (inicio + plen == fin) return true;
    char sig = linea[inicio + plen];
    return sig == ' ' || sig == '\t' || sig == ':' || sig == '\n' || sig == '\r';
}

/* True si la linea acaba en ':' tras descartar comentario al final y
 * trailing whitespace. Cadenas se respetan; '#' dentro de cadenas no
 * inicia comentario. */
static bool linea_acaba_en_dos_puntos(const char *linea, size_t fin) {
    bool en_cadena = false;
    char delim = 0;
    size_t corte = fin;
    for (size_t i = 0; i < fin; i++) {
        char c = linea[i];
        if (en_cadena) {
            if (c == '\\' && i + 1 < fin) { i++; continue; }
            if (c == delim) en_cadena = false;
            continue;
        }
        if (c == '"' || c == '\'') {
            en_cadena = true;
            delim = c;
            continue;
        }
        if (c == '#') { corte = i; break; }
    }
    while (corte > 0 && (linea[corte - 1] == ' ' || linea[corte - 1] == '\t')) {
        corte--;
    }
    return corte > 0 && linea[corte - 1] == ':';
}

/* Avanza por el contenido de una linea (sin trailing whitespace ni '\n')
 * actualizando profundidad de corchetes/parentesis/llaves y estado de
 * triple-quoted string. Comentarios y cadenas simples no contribuyen al
 * conteo. */
static void avanzar_linea(const char *inicio, size_t len,
                           int *prof, bool *en_triple, char *triple_delim) {
    size_t i = 0;
    while (i < len) {
        char c = inicio[i];
        if (*en_triple) {
            /* Buscar cierre del triple. */
            if (c == *triple_delim
                && i + 2 < len
                && inicio[i + 1] == *triple_delim
                && inicio[i + 2] == *triple_delim) {
                *en_triple = false;
                i += 3;
                continue;
            }
            if (c == '\\' && i + 1 < len) { i += 2; continue; }
            i++;
            continue;
        }
        if (c == '#') {
            /* Comentario hasta fin de linea — paramos el barrido. */
            return;
        }
        if (c == '"' || c == '\'') {
            /* Triple? */
            if (i + 2 < len && inicio[i + 1] == c && inicio[i + 2] == c) {
                *en_triple = true;
                *triple_delim = c;
                i += 3;
                continue;
            }
            /* Cadena simple — saltar hasta el cierre en la misma linea.
             * Si no se cierra en la linea, la asumimos malformada y
             * paramos (la sintaxis fallara aguas abajo del formateador). */
            char d = c;
            i++;
            while (i < len) {
                if (inicio[i] == '\\' && i + 1 < len) { i += 2; continue; }
                if (inicio[i] == d) { i++; break; }
                i++;
            }
            continue;
        }
        if (c == '(' || c == '[' || c == '{') (*prof)++;
        else if (c == ')' || c == ']' || c == '}') (*prof)--;
        i++;
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Formateador principal.
 * ────────────────────────────────────────────────────────────────── */

/* Pila ligera de bloques abiertos. Solo guardamos un caracter por
 * bloque — basta con distinguir 'C' (cuando) del resto para resolver
 * el caso especial de `coincidir`/`cuando ... cuando ...`. */
#define MAX_PILA_BLOQUES 512

FormatoResultado formateador_formatear(const char *fuente) {
    FormatoResultado r;
    r.fuente = NULL;
    r.longitud = 0;
    r.cambiada = false;
    r.mensaje_error = NULL;

    Buf buf;
    buf_iniciar(&buf);

    size_t total = strlen(fuente);
    const char *fin_total = fuente + total;

    int profundidad = 0;
    int prof_corch = 0;
    bool en_triple = false;
    char triple_delim = 0;
    int blancas_consec = 0;
    char pila[MAX_PILA_BLOQUES];
    int sp = 0;

    const char *cursor = fuente;
    while (cursor < fin_total) {
        const char *fin_linea = cursor;
        while (fin_linea < fin_total && *fin_linea != '\n') fin_linea++;
        size_t len = (size_t)(fin_linea - cursor);
        size_t code_fin = trim_trailing(cursor, len);
        size_t code_inicio = skip_leading_ws(cursor, code_fin);
        bool es_blanca = (code_inicio >= code_fin);

        bool es_continuacion = (prof_corch > 0) || en_triple;

        if (es_blanca) {
            blancas_consec++;
            if (blancas_consec == 1) {
                buf_append_char(&buf, '\n');
            }
        } else {
            blancas_consec = 0;
            if (es_continuacion) {
                /* Preserva leading whitespace original; solo trimea trailing. */
                buf_append(&buf, cursor, code_fin);
                buf_append_char(&buf, '\n');
            } else {
                bool es_fin = empieza_por(cursor, code_inicio, code_fin, "fin");
                bool es_sino = empieza_por(cursor, code_inicio, code_fin, "sino");
                bool es_atrapar = empieza_por(cursor, code_inicio, code_fin, "atrapar");
                bool es_finalmente = empieza_por(cursor, code_inicio, code_fin, "finalmente");
                bool es_cuando = empieza_por(cursor, code_inicio, code_fin, "cuando");
                bool es_mid_block_puro = es_sino || es_atrapar || es_finalmente;

                /* `cuando` consecutivo dentro de coincidir: cierra el case
                 * anterior antes de abrir el nuevo. */
                if (es_cuando && sp > 0 && pila[sp - 1] == 'C') {
                    sp--;
                    profundidad--;
                    if (profundidad < 0) profundidad = 0;
                }

                /* `fin` cierra cualquier `cuando` colgante antes de cerrar
                 * el bloque exterior (idem a la regla de cuando consecutivo). */
                if (es_fin && sp > 0 && pila[sp - 1] == 'C') {
                    sp--;
                    profundidad--;
                    if (profundidad < 0) profundidad = 0;
                }

                int prof_render = profundidad;
                if (es_fin) {
                    if (sp > 0) sp--;
                    prof_render = profundidad - 1;
                    if (prof_render < 0) prof_render = 0;
                    profundidad = prof_render;
                } else if (es_mid_block_puro) {
                    prof_render = profundidad - 1;
                    if (prof_render < 0) prof_render = 0;
                }

                buf_append_espacios(&buf, prof_render * INDENT_ANCHO);
                buf_append(&buf, cursor + code_inicio, code_fin - code_inicio);
                buf_append_char(&buf, '\n');

                /* Lineas que abren bloque: incrementan profundidad y
                 * empujan en la pila el caracter que identifica el bloque.
                 * Mid-block puros (sino/atrapar/finalmente) y `fin` no
                 * abren — su cuerpo continua en la profundidad existente. */
                if (!es_fin && !es_mid_block_puro
                    && linea_acaba_en_dos_puntos(cursor + code_inicio,
                                                  code_fin - code_inicio)) {
                    char marca = 'X';
                    if (es_cuando) marca = 'C';
                    /* No necesitamos distinguir mas tipos de bloque — la
                     * pila solo se consulta para el caso 'C'. */
                    if (sp < MAX_PILA_BLOQUES) pila[sp++] = marca;
                    profundidad++;
                }
            }
        }

        avanzar_linea(cursor + code_inicio, code_fin - code_inicio,
                       &prof_corch, &en_triple, &triple_delim);

        cursor = fin_linea;
        if (cursor < fin_total) cursor++;  /* consumir '\n' */
    }

    /* Normalizar trailing newlines: como mucho uno, y solo si hubo
     * contenido. */
    while (buf.cuenta > 0 && buf.datos[buf.cuenta - 1] == '\n') {
        buf.cuenta--;
    }
    if (buf.cuenta > 0) {
        buf_append_char(&buf, '\n');
    }
    if (buf.datos) buf.datos[buf.cuenta] = '\0';

    if (buf.datos == NULL) {
        /* Entrada vacia — devolvemos cadena vacia explicita. */
        buf.datos = (char *)malloc(1);
        if (buf.datos) buf.datos[0] = '\0';
        buf.cuenta = 0;
    }

    r.fuente = buf.datos;
    r.longitud = buf.cuenta;
    if (r.longitud != total || memcmp(r.fuente, fuente, total) != 0) {
        r.cambiada = true;
    }
    return r;
}

void formato_resultado_destruir(FormatoResultado *r) {
    if (!r) return;
    free(r->fuente);
    r->fuente = NULL;
    free(r->mensaje_error);
    r->mensaje_error = NULL;
    r->longitud = 0;
    r->cambiada = false;
}
