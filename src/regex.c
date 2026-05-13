/*
 * Cornamusa v1.28 — Motor regex (parser + matcher backtracking).
 *
 * Estrategia:
 *   1. Parser recursivo descendente construye un árbol de nodos
 *      (NODO_*) en un arena local.
 *   2. Matcher recursivo intenta `match_nodo(nodo, texto, pos)` con
 *      backtracking — para alternancia y quantifiers ambiciosos.
 *
 * El alfabeto opera sobre bytes (no code points UTF-8). Esto es
 * adecuado para la mayoría de patrones ASCII; texto UTF-8 multi-byte
 * funciona si el patrón también es UTF-8 byte-a-byte. Las clases
 * predefinidas (\d, \w, \s) operan sobre ASCII.
 */

#include "regex.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ────────────────────────────────────────────────────────────────
 * AST
 * ──────────────────────────────────────────────────────────────── */

typedef enum {
    NODO_LITERAL,      /* match exact byte */
    NODO_CUALQUIER,    /* `.` */
    NODO_CLASE,        /* [abc], [^a-z], \d, \w, etc. — máscara de 256 bits */
    NODO_ANCLA_INICIO, /* `^` */
    NODO_ANCLA_FIN,    /* `$` */
    NODO_CONCAT,       /* secuencia de hijos */
    NODO_ALT,          /* alternancia de hijos */
    NODO_QUANT,        /* hijo + (min, max). max=-1 => infinito */
} TipoNodo;

typedef struct Nodo Nodo;
struct Nodo {
    TipoNodo tipo;
    /* Para LITERAL: byte. */
    unsigned char byte_lit;
    /* Para CLASE: bitset de 256 bits. */
    unsigned char mascara[32];  /* 32 * 8 = 256 bits */
    /* Para CONCAT y ALT: array de hijos. */
    Nodo **hijos;
    int n_hijos;
    /* Para QUANT: un solo hijo + límites. */
    Nodo *hijo;
    int min;
    int max;  /* -1 si infinito */
};

/* Arena simple: lista de nodos. Liberar al final con regex_destruir_ast. */
typedef struct {
    Nodo **nodos;
    int cuenta;
    int capacidad;
} Arena;

static Nodo *arena_nuevo(Arena *a, TipoNodo tipo) {
    if (a->cuenta >= a->capacidad) {
        int nueva = a->capacidad ? a->capacidad * 2 : 16;
        Nodo **np = (Nodo **)realloc(a->nodos, sizeof(Nodo *) * (size_t)nueva);
        if (!np) return NULL;
        a->nodos = np;
        a->capacidad = nueva;
    }
    Nodo *n = (Nodo *)calloc(1, sizeof(Nodo));
    if (!n) return NULL;
    n->tipo = tipo;
    a->nodos[a->cuenta++] = n;
    return n;
}

static void arena_destruir(Arena *a) {
    if (!a) return;
    for (int i = 0; i < a->cuenta; i++) {
        Nodo *n = a->nodos[i];
        if (n) {
            free(n->hijos);
            free(n);
        }
    }
    free(a->nodos);
    a->nodos = NULL;
    a->cuenta = 0;
    a->capacidad = 0;
}

/* ────────────────────────────────────────────────────────────────
 * Helpers de máscara para clases.
 * ──────────────────────────────────────────────────────────────── */

static inline void mascara_set(unsigned char *m, unsigned char b) {
    m[b >> 3] |= (unsigned char)(1u << (b & 7));
}

static inline bool mascara_tiene(const unsigned char *m, unsigned char b) {
    return (m[b >> 3] & (1u << (b & 7))) != 0;
}

static void mascara_clear(unsigned char *m) {
    memset(m, 0, 32);
}

static void mascara_invertir(unsigned char *m) {
    for (int i = 0; i < 32; i++) m[i] = (unsigned char)~m[i];
}

static void mascara_rango(unsigned char *m, unsigned char a, unsigned char b) {
    for (int i = a; i <= b; i++) mascara_set(m, (unsigned char)i);
}

/* Llena la máscara según la clase escape: 'd', 'D', 'w', 'W', 's', 'S'.
   Retorna true si reconoció; false si no. */
static bool aplicar_clase_predef(unsigned char *m, char c) {
    switch (c) {
        case 'd':
            mascara_rango(m, '0', '9');
            return true;
        case 'D':
            mascara_rango(m, 0, 255);
            for (int i = '0'; i <= '9'; i++) m[i >> 3] &= (unsigned char)~(1u << (i & 7));
            return true;
        case 'w':
            mascara_rango(m, 'a', 'z');
            mascara_rango(m, 'A', 'Z');
            mascara_rango(m, '0', '9');
            mascara_set(m, '_');
            return true;
        case 'W':
            mascara_rango(m, 0, 255);
            for (int i = 'a'; i <= 'z'; i++) m[i >> 3] &= (unsigned char)~(1u << (i & 7));
            for (int i = 'A'; i <= 'Z'; i++) m[i >> 3] &= (unsigned char)~(1u << (i & 7));
            for (int i = '0'; i <= '9'; i++) m[i >> 3] &= (unsigned char)~(1u << (i & 7));
            m['_' >> 3] &= (unsigned char)~(1u << ('_' & 7));
            return true;
        case 's':
            mascara_set(m, ' ');
            mascara_set(m, '\t');
            mascara_set(m, '\n');
            mascara_set(m, '\r');
            mascara_set(m, '\f');
            mascara_set(m, '\v');
            return true;
        case 'S':
            mascara_rango(m, 0, 255);
            m[' ' >> 3] &= (unsigned char)~(1u << (' ' & 7));
            m['\t' >> 3] &= (unsigned char)~(1u << ('\t' & 7));
            m['\n' >> 3] &= (unsigned char)~(1u << ('\n' & 7));
            m['\r' >> 3] &= (unsigned char)~(1u << ('\r' & 7));
            m['\f' >> 3] &= (unsigned char)~(1u << ('\f' & 7));
            m['\v' >> 3] &= (unsigned char)~(1u << ('\v' & 7));
            return true;
        default:
            return false;
    }
}

/* Convierte un escape literal: \. \n \t etc. → byte real. */
static unsigned char escape_a_byte(char c) {
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case 'f': return '\f';
        case 'v': return '\v';
        case '0': return '\0';
        default:  return (unsigned char)c;  /* literal \. \\ etc. */
    }
}

/* ────────────────────────────────────────────────────────────────
 * Parser recursivo descendente.
 * ──────────────────────────────────────────────────────────────── */

typedef struct {
    const char *p;
    int len;
    int pos;
    Arena *arena;
    char *err;
    size_t err_cap;
    bool fallo;
} Parser;

static void p_error(Parser *p, const char *fmt, ...) {
    if (p->fallo) return;
    p->fallo = true;
    if (!p->err) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->err, p->err_cap, fmt, ap);
    va_end(ap);
}

static Nodo *parse_alt(Parser *p);
static Nodo *parse_concat(Parser *p);
static Nodo *parse_quant(Parser *p);
static Nodo *parse_atomo(Parser *p);

static Nodo *parse_clase(Parser *p) {
    /* `[` ya consumido. */
    Nodo *n = arena_nuevo(p->arena, NODO_CLASE);
    if (!n) { p_error(p, "memoria insuficiente"); return NULL; }
    mascara_clear(n->mascara);
    bool negar = false;
    if (p->pos < p->len && p->p[p->pos] == '^') {
        negar = true;
        p->pos++;
    }
    bool vacia = true;
    while (p->pos < p->len && p->p[p->pos] != ']') {
        unsigned char a;
        if (p->p[p->pos] == '\\' && p->pos + 1 < p->len) {
            char nxt = p->p[p->pos + 1];
            /* Si es clase predefinida, aplicar y avanzar. */
            unsigned char tmp[32] = {0};
            if (aplicar_clase_predef(tmp, nxt)) {
                for (int i = 0; i < 32; i++) n->mascara[i] |= tmp[i];
                p->pos += 2;
                vacia = false;
                continue;
            }
            a = escape_a_byte(nxt);
            p->pos += 2;
        } else {
            a = (unsigned char)p->p[p->pos];
            p->pos++;
        }
        /* Rango: a-b */
        if (p->pos + 1 < p->len
            && p->p[p->pos] == '-' && p->p[p->pos + 1] != ']') {
            p->pos++;  /* consume '-' */
            unsigned char b;
            if (p->p[p->pos] == '\\' && p->pos + 1 < p->len) {
                b = escape_a_byte(p->p[p->pos + 1]);
                p->pos += 2;
            } else {
                b = (unsigned char)p->p[p->pos];
                p->pos++;
            }
            if (a > b) {
                p_error(p, "rango invertido en clase: %c-%c", a, b);
                return NULL;
            }
            mascara_rango(n->mascara, a, b);
        } else {
            mascara_set(n->mascara, a);
        }
        vacia = false;
    }
    if (p->pos >= p->len || p->p[p->pos] != ']') {
        p_error(p, "clase '[...]' sin cerrar en pos %d", p->pos);
        return NULL;
    }
    p->pos++;  /* consume ']' */
    if (vacia) {
        p_error(p, "clase '[]' vacia");
        return NULL;
    }
    if (negar) mascara_invertir(n->mascara);
    return n;
}

static Nodo *parse_atomo(Parser *p) {
    if (p->pos >= p->len) {
        p_error(p, "se esperaba atomo en pos %d", p->pos);
        return NULL;
    }
    char c = p->p[p->pos];
    switch (c) {
        case '(': {
            p->pos++;
            /* Soporta (?: ... ) no-captura. (...) también se ignora
               como grupo (no expongo captura en v1.28.0). */
            if (p->pos + 1 < p->len
                && p->p[p->pos] == '?' && p->p[p->pos + 1] == ':') {
                p->pos += 2;
            }
            Nodo *n = parse_alt(p);
            if (p->fallo) return NULL;
            if (p->pos >= p->len || p->p[p->pos] != ')') {
                p_error(p, "grupo '(' sin cerrar");
                return NULL;
            }
            p->pos++;
            return n;
        }
        case '[':
            p->pos++;
            return parse_clase(p);
        case '.': {
            p->pos++;
            return arena_nuevo(p->arena, NODO_CUALQUIER);
        }
        case '^': {
            p->pos++;
            return arena_nuevo(p->arena, NODO_ANCLA_INICIO);
        }
        case '$': {
            p->pos++;
            return arena_nuevo(p->arena, NODO_ANCLA_FIN);
        }
        case '\\': {
            if (p->pos + 1 >= p->len) {
                p_error(p, "escape '\\' al final");
                return NULL;
            }
            char nxt = p->p[p->pos + 1];
            /* Si es clase predefinida, construir NODO_CLASE. */
            unsigned char tmp[32] = {0};
            if (aplicar_clase_predef(tmp, nxt)) {
                Nodo *n = arena_nuevo(p->arena, NODO_CLASE);
                if (!n) { p_error(p, "memoria insuficiente"); return NULL; }
                memcpy(n->mascara, tmp, 32);
                p->pos += 2;
                return n;
            }
            /* Escape literal. */
            Nodo *n = arena_nuevo(p->arena, NODO_LITERAL);
            if (!n) { p_error(p, "memoria insuficiente"); return NULL; }
            n->byte_lit = escape_a_byte(nxt);
            p->pos += 2;
            return n;
        }
        case '*': case '+': case '?': case ')': case '|': case ']': case '{': case '}':
            p_error(p, "caracter inesperado '%c' en pos %d", c, p->pos);
            return NULL;
        default: {
            Nodo *n = arena_nuevo(p->arena, NODO_LITERAL);
            if (!n) { p_error(p, "memoria insuficiente"); return NULL; }
            n->byte_lit = (unsigned char)c;
            p->pos++;
            return n;
        }
    }
}

static Nodo *parse_quant(Parser *p) {
    Nodo *at = parse_atomo(p);
    if (p->fallo) return NULL;
    if (p->pos < p->len) {
        char c = p->p[p->pos];
        if (c == '*' || c == '+' || c == '?') {
            p->pos++;
            Nodo *q = arena_nuevo(p->arena, NODO_QUANT);
            if (!q) { p_error(p, "memoria insuficiente"); return NULL; }
            q->hijo = at;
            if (c == '*') { q->min = 0; q->max = -1; }
            else if (c == '+') { q->min = 1; q->max = -1; }
            else { q->min = 0; q->max = 1; }
            return q;
        }
    }
    return at;
}

static Nodo *parse_concat(Parser *p) {
    Nodo *primero = NULL;
    Nodo **hijos = NULL;
    int n = 0, cap = 0;
    while (p->pos < p->len) {
        char c = p->p[p->pos];
        if (c == ')' || c == '|') break;
        Nodo *q = parse_quant(p);
        if (p->fallo) {
            free(hijos);
            return NULL;
        }
        if (n == 0) {
            primero = q;
        } else if (n == 1) {
            /* Promover a CONCAT. */
            cap = 8;
            hijos = (Nodo **)malloc(sizeof(Nodo *) * (size_t)cap);
            if (!hijos) { p_error(p, "memoria insuficiente"); return NULL; }
            hijos[0] = primero;
            hijos[1] = q;
        } else {
            if (n >= cap) {
                cap *= 2;
                Nodo **np = (Nodo **)realloc(hijos, sizeof(Nodo *) * (size_t)cap);
                if (!np) { free(hijos); p_error(p, "memoria insuficiente"); return NULL; }
                hijos = np;
            }
            hijos[n] = q;
        }
        n++;
    }
    if (n == 0) {
        /* Concat vacía: nodo CONCAT con 0 hijos (matchea cadena vacía). */
        Nodo *c = arena_nuevo(p->arena, NODO_CONCAT);
        if (!c) { p_error(p, "memoria insuficiente"); return NULL; }
        return c;
    }
    if (n == 1) return primero;
    Nodo *c = arena_nuevo(p->arena, NODO_CONCAT);
    if (!c) { free(hijos); p_error(p, "memoria insuficiente"); return NULL; }
    c->hijos = hijos;
    c->n_hijos = n;
    return c;
}

static Nodo *parse_alt(Parser *p) {
    Nodo *primero = parse_concat(p);
    if (p->fallo) return NULL;
    if (p->pos >= p->len || p->p[p->pos] != '|') return primero;
    /* Hay alternancia. */
    Nodo **hijos = NULL;
    int n = 1, cap = 4;
    hijos = (Nodo **)malloc(sizeof(Nodo *) * (size_t)cap);
    if (!hijos) { p_error(p, "memoria insuficiente"); return NULL; }
    hijos[0] = primero;
    while (p->pos < p->len && p->p[p->pos] == '|') {
        p->pos++;
        Nodo *r = parse_concat(p);
        if (p->fallo) { free(hijos); return NULL; }
        if (n >= cap) {
            cap *= 2;
            Nodo **np = (Nodo **)realloc(hijos, sizeof(Nodo *) * (size_t)cap);
            if (!np) { free(hijos); p_error(p, "memoria insuficiente"); return NULL; }
            hijos = np;
        }
        hijos[n++] = r;
    }
    Nodo *alt = arena_nuevo(p->arena, NODO_ALT);
    if (!alt) { free(hijos); p_error(p, "memoria insuficiente"); return NULL; }
    alt->hijos = hijos;
    alt->n_hijos = n;
    return alt;
}

/* Parsea el patrón completo. Retorna la raíz o NULL si error. */
static Nodo *parsear(Arena *a, const char *patron, int len,
                      char *err, size_t err_cap) {
    Parser p = {0};
    p.p = patron;
    p.len = len;
    p.pos = 0;
    p.arena = a;
    p.err = err;
    p.err_cap = err_cap;
    p.fallo = false;
    Nodo *raiz = parse_alt(&p);
    if (p.fallo) return NULL;
    if (p.pos != len) {
        p_error(&p, "caracter inesperado en pos %d", p.pos);
        return NULL;
    }
    return raiz;
}

/* ────────────────────────────────────────────────────────────────
 * Matcher.
 *
 * `match(nodo, texto, len, pos)` retorna la nueva posición tras
 * matchear el nodo desde `pos`, o -1 si falla. Para alternancia y
 * quantifiers usamos backtracking explícito via la rama "resto".
 *
 * La forma idiomática: `match(nodo, texto, len, pos, resto)` donde
 * `resto` es el "continuation" (qué viene después). El nodo intenta
 * matchearse y luego verifica que el resto también matchee desde la
 * nueva posición. Esto permite implementar greedy con backtracking
 * correcto sin construir un NFA explícito.
 * ──────────────────────────────────────────────────────────────── */

typedef struct Cont Cont;
struct Cont {
    Nodo *nodo;       /* siguiente nodo a matchear (o NULL si fin) */
    const Cont *prev; /* siguiente cont tras `nodo` */
};

/* Forward declarations. */
static int match_resto(const Cont *resto, const char *texto, int len, int pos);
static int match_nodo(Nodo *n, const Cont *resto,
                       const char *texto, int len, int pos);

static int match_resto(const Cont *resto, const char *texto, int len, int pos) {
    if (resto == NULL) return pos;  /* fin: éxito */
    return match_nodo(resto->nodo, resto->prev, texto, len, pos);
}

static int match_nodo(Nodo *n, const Cont *resto,
                       const char *texto, int len, int pos) {
    switch (n->tipo) {
        case NODO_LITERAL:
            if (pos < len && (unsigned char)texto[pos] == n->byte_lit) {
                return match_resto(resto, texto, len, pos + 1);
            }
            return -1;
        case NODO_CUALQUIER:
            if (pos < len && texto[pos] != '\n') {
                return match_resto(resto, texto, len, pos + 1);
            }
            return -1;
        case NODO_CLASE:
            if (pos < len && mascara_tiene(n->mascara, (unsigned char)texto[pos])) {
                return match_resto(resto, texto, len, pos + 1);
            }
            return -1;
        case NODO_ANCLA_INICIO:
            if (pos == 0) return match_resto(resto, texto, len, pos);
            return -1;
        case NODO_ANCLA_FIN:
            if (pos == len) return match_resto(resto, texto, len, pos);
            return -1;
        case NODO_CONCAT: {
            if (n->n_hijos == 0) return match_resto(resto, texto, len, pos);
            /* Construir cadena de continuations: hijos[1..n-1] + resto. */
            Cont locales[16];
            const Cont *cur = resto;
            int n_locales = 0;
            for (int i = n->n_hijos - 1; i >= 1; i--) {
                if (n_locales >= 16) {
                    /* Demasiada profundidad — fallback a heap alloc. */
                    /* Aceptable para v1.28: 16 elementos consecutivos en
                       concat es plenty. */
                    return -1;
                }
                locales[n_locales].nodo = n->hijos[i];
                locales[n_locales].prev = cur;
                cur = &locales[n_locales];
                n_locales++;
            }
            return match_nodo(n->hijos[0], cur, texto, len, pos);
        }
        case NODO_ALT: {
            for (int i = 0; i < n->n_hijos; i++) {
                int r = match_nodo(n->hijos[i], resto, texto, len, pos);
                if (r >= 0) return r;
            }
            return -1;
        }
        case NODO_QUANT: {
            int min = n->min;
            int max = n->max;
            /* Match al menos `min`, luego greedy hasta `max`. */
            int avanzados = 0;
            int last_pos = pos;
            /* Matchear `min` obligatorios. */
            while (avanzados < min) {
                int r = match_nodo(n->hijo, NULL, texto, len, last_pos);
                /* Para hacer el match del hijo aislado, le pasamos resto=NULL.
                   Eso retorna la posición tras matchear el hijo sin
                   considerar lo que viene después. */
                if (r < 0) return -1;
                if (r == last_pos) {
                    /* Hijo matcheó vacío — evitar loop infinito en `(...)*`. */
                    break;
                }
                last_pos = r;
                avanzados++;
            }
            /* Greedy: registrar todas las posiciones donde podemos parar y
               probar el resto desde la última hacia atrás. */
            int posiciones[1024];
            int n_pos = 0;
            posiciones[n_pos++] = last_pos;
            while ((max < 0 || avanzados < max) && n_pos < 1024) {
                int r = match_nodo(n->hijo, NULL, texto, len, last_pos);
                if (r < 0 || r == last_pos) break;
                last_pos = r;
                posiciones[n_pos++] = last_pos;
                avanzados++;
            }
            /* Probar resto desde la posición más avanzada hacia atrás,
               pero solo retroceder hasta `min`. El array fue llenado
               desde min hasta avanzados, así que retrocedemos por
               índice. */
            for (int i = n_pos - 1; i >= 0; i--) {
                int r = match_resto(resto, texto, len, posiciones[i]);
                if (r >= 0) return r;
            }
            return -1;
        }
    }
    return -1;
}

/* ────────────────────────────────────────────────────────────────
 * API pública
 * ──────────────────────────────────────────────────────────────── */

bool regex_coincidir(const char *patron, const char *texto, int texto_len,
                       int *fin_out,
                       char *err_buf, size_t err_cap) {
    Arena a = {0};
    int plen = (int)strlen(patron);
    Nodo *raiz = parsear(&a, patron, plen, err_buf, err_cap);
    if (!raiz) { arena_destruir(&a); return false; }
    int r = match_nodo(raiz, NULL, texto, texto_len, 0);
    arena_destruir(&a);
    if (r < 0) return false;
    if (fin_out) *fin_out = r;
    return true;
}

bool regex_buscar(const char *patron, const char *texto, int texto_len,
                    int *inicio_out, int *fin_out,
                    char *err_buf, size_t err_cap) {
    Arena a = {0};
    int plen = (int)strlen(patron);
    Nodo *raiz = parsear(&a, patron, plen, err_buf, err_cap);
    if (!raiz) { arena_destruir(&a); return false; }
    /* Detecta ancla inicial para optimización: si patrón empieza con ^,
       solo probar pos=0. Si no, probar todas las posiciones. */
    bool anclado_inicio = (raiz->tipo == NODO_ANCLA_INICIO)
        || (raiz->tipo == NODO_CONCAT && raiz->n_hijos > 0
            && raiz->hijos[0]->tipo == NODO_ANCLA_INICIO);
    int pos_max = anclado_inicio ? 0 : texto_len;
    for (int i = 0; i <= pos_max; i++) {
        int r = match_nodo(raiz, NULL, texto, texto_len, i);
        if (r >= 0) {
            if (inicio_out) *inicio_out = i;
            if (fin_out) *fin_out = r;
            arena_destruir(&a);
            return true;
        }
    }
    arena_destruir(&a);
    return false;
}

int regex_todos(const char *patron, const char *texto, int texto_len,
                 RegexCallback cb, void *datos,
                 char *err_buf, size_t err_cap) {
    Arena a = {0};
    int plen = (int)strlen(patron);
    Nodo *raiz = parsear(&a, patron, plen, err_buf, err_cap);
    if (!raiz) { arena_destruir(&a); return -1; }
    int cuenta = 0;
    int i = 0;
    while (i <= texto_len) {
        int r = match_nodo(raiz, NULL, texto, texto_len, i);
        if (r >= 0) {
            if (cb && !cb(i, r, datos)) break;
            cuenta++;
            if (r == i) {
                /* Match vacío: avanzar 1 para no loop infinito. */
                i++;
            } else {
                i = r;
            }
        } else {
            i++;
        }
    }
    arena_destruir(&a);
    return cuenta;
}
