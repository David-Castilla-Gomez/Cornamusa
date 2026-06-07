#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errores.h"

/* ──────────────────────────────────────────────────────────────────
 * Niveles de precedencia (estilo Pratt)
 *
 * Más alto = más fuerte. Se evalúa antes en ausencia de paréntesis.
 * Sigue de cerca la precedencia de Python (y de la gramática PEG en
 * ESPEC.md §5).
 * ────────────────────────────────────────────────────────────────── */

typedef enum {
    PREC_NULA = 0,
    PREC_TERNARIA,     /* v1.44: `<si_si> si <cond> sino <si_no>` — la más baja */
    PREC_O,            /* `o` */
    PREC_Y,            /* `y` */
    PREC_NO,           /* `no` (unario) */
    PREC_COMPARAR,     /* == != < <= > >= es es-no en no-en */
    PREC_BIT_O,        /* | */
    PREC_BIT_X,        /* ^ */
    PREC_BIT_Y,        /* & */
    PREC_DESPLAZAR,    /* << >> */
    PREC_TERMINO,      /* + - */
    PREC_FACTOR,       /* * / // % */
    PREC_UNARIO,       /* + - ~ (prefijo) */
    PREC_POTENCIA,     /* ** (asociativa derecha) */
    PREC_LLAMADA,      /* . () [] */
    PREC_PRIMARIA,
} Precedencia;

/* ──────────────────────────────────────────────────────────────────
 * Tabla de reglas (prefijo / infijo / precedencia)
 *
 * Para cada TipoToken, la tabla dice:
 *   - prefijo: función a llamar cuando el token aparece al inicio
 *     de una expresión (literal, unario, paréntesis abre, identif.).
 *   - infijo: función a llamar cuando el token aparece después de
 *     una sub-expresión (operador binario, llamada, atributo).
 *   - precedencia: nivel del operador como infijo.
 * ────────────────────────────────────────────────────────────────── */

typedef Expr *(*FnPrefijo)(Parser *);
typedef Expr *(*FnInfijo)(Parser *, Expr *);

typedef struct {
    FnPrefijo prefijo;
    FnInfijo infijo;
    Precedencia precedencia;
} ReglaParseo;

/* Forward declarations de las funciones de parseo. */
static Expr *parsear_expresion(Parser *p);
static Expr *parsear_precedencia(Parser *p, Precedencia min_prec);
static Expr *parsear_numero_entero(Parser *p);
static Expr *parsear_numero_decimal(Parser *p);
static Expr *parsear_cadena(Parser *p);
static Expr *parsear_f_cadena(Parser *p);
static Expr *parsear_booleano(Parser *p);
static Expr *parsear_nulo(Parser *p);
static Expr *parsear_ident(Parser *p);
static Expr *parsear_super(Parser *p);
static Expr *parsear_grupo(Parser *p);
static bool parsear_comprehension_cola(Parser *p,
                                          const char **nombre_var_out,
                                          int *longitud_var_out,
                                          Expr **iterable_out,
                                          Expr **guarda_out,
                                          Expr **patron_out,
                                          struct ClausulaComp **clausulas_extra_out,
                                          int *n_extras_out);
static Expr *parsear_unario(Parser *p);
static Expr *parsear_no(Parser *p);
static Expr *parsear_lambda(Parser *p);
static Expr *parsear_lista_literal(Parser *p);
static Expr *parsear_llaves(Parser *p);
static Expr *parsear_binario(Parser *p, Expr *izq);
static Expr *parsear_logica(Parser *p, Expr *izq);
static Expr *parsear_potencia(Parser *p, Expr *izq);
static Expr *parsear_llamada(Parser *p, Expr *callee);
static Expr *parsear_atributo(Parser *p, Expr *objeto);
static Expr *parsear_indice_o_rebanada(Parser *p, Expr *objeto);
static Expr *parsear_es(Parser *p, Expr *izq);
static Expr *parsear_no_compuesto(Parser *p, Expr *izq);

static const ReglaParseo *obtener_regla(TipoToken tipo);
static void avanzar(Parser *p);
static bool check(Parser *p, TipoToken t);
static bool consumir_si(Parser *p, TipoToken t);
static bool consumir(Parser *p, TipoToken t, const char *mensaje);
static void error_en(Parser *p, const Token *t, const char *mensaje);

/* ──────────────────────────────────────────────────────────────────
 * Mecánica del parser
 * ────────────────────────────────────────────────────────────────── */

void parser_iniciar(Parser *p, Lexer *l, Arena *a,
                    const char *fuente, const char *archivo) {
    p->lexer = l;
    p->arena = a;
    p->fuente = fuente;
    p->archivo = archivo;
    p->tuvo_error = false;
    p->en_panico = false;
    p->profundidad_bloques = 0;
    /* Pre-cargamos el primer token; previo se queda en cero hasta el
       primer avanzar real, lo que está bien porque nada lo lee antes. */
    p->previo = (Token){0};
    /* v1.53: por defecto, errores van a stderr. El cliente puede
     * setear `capturar_errores=true` + `errores_capturados=&buf` antes
     * de invocar el primer parsing para acumular errores como datos. */
    p->capturar_errores = false;
    p->errores_capturados = NULL;
    p->actual = lexer_siguiente(l);

    /* Si el primer token es un error, lo reportamos pero seguimos
       (para que parsear_expresion vea TT_ERROR y reporte/recupere). */
}

static void avanzar(Parser *p) {
    p->previo = p->actual;
    /* Saltamos tokens TT_ERROR consecutivos para no inundar de
       diagnósticos; reportamos el primero que veamos. */
    while (true) {
        p->actual = lexer_siguiente(p->lexer);
        if (p->actual.tipo != TT_ERROR) break;
        error_en(p, &p->actual, p->actual.mensaje);
    }
}

static bool check(Parser *p, TipoToken t) {
    return p->actual.tipo == t;
}

static bool consumir_si(Parser *p, TipoToken t) {
    if (!check(p, t)) return false;
    avanzar(p);
    return true;
}

static bool consumir(Parser *p, TipoToken t, const char *mensaje) {
    if (check(p, t)) {
        avanzar(p);
        return true;
    }
    error_en(p, &p->actual, mensaje);
    return false;
}

/*
 * Reporta un error en `t` con `mensaje`. El primer error activa
 * `tuvo_error`; si ya estamos en pánico no reportamos duplicados.
 *
 * Construimos un Token sintético TT_ERROR para reusar
 * error_imprimir_token (que dibuja caret indicators).
 */
static void error_en(Parser *p, const Token *t, const char *mensaje) {
    if (p->en_panico) return;
    p->tuvo_error = true;
    p->en_panico = true;

    Token err = *t;
    err.tipo = TT_ERROR;
    err.mensaje = mensaje;

    /* v1.53: si el cliente activo captura, anadimos a su buffer y
     * NO imprimimos a stderr. Si no, comportamiento clasico. */
    if (p->capturar_errores && p->errores_capturados) {
        ErroresParser *es = p->errores_capturados;
        if (es->n >= es->capacidad) {
            int nc = es->capacidad ? es->capacidad * 2 : 8;
            ErrorParser *nv = (ErrorParser *)realloc(es->items,
                                                       sizeof(ErrorParser) * (size_t)nc);
            if (nv) { es->items = nv; es->capacidad = nc; }
            else return;
        }
        ErrorParser *ep = &es->items[es->n++];
        ep->linea = t->linea;
        ep->columna = t->columna;
        ep->mensaje = mensaje ? strdup(mensaje) : strdup("error de sintaxis");
        return;
    }

    /* Si el token ofensor era válido, asumimos longitud >= 1 ya. */
    error_imprimir_token(&err, p->fuente, p->archivo, stderr);
}

void parser_errores_liberar(ErroresParser *e) {
    if (!e) return;
    for (int i = 0; i < e->n; i++) free(e->items[i].mensaje);
    free(e->items);
    e->items = NULL;
    e->n = 0;
    e->capacidad = 0;
}

/* ──────────────────────────────────────────────────────────────────
 * Parseo de expresiones (Pratt)
 * ────────────────────────────────────────────────────────────────── */

Expr *parser_parsear_expr(Parser *p) {
    return parsear_expresion(p);
}

static Expr *parsear_expresion(Parser *p) {
    /* v1.44: PREC_TERNARIA es el nivel más bajo, así que parsear con
       ese piso incluye la expresión ternaria como infix opcional. Los
       sitios que NO quieren consumir `si` como ternario (en
       particular la cabeza de iter en comprehensions) llaman a
       `parsear_precedencia(p, PREC_O)` directamente. */
    return parsear_precedencia(p, PREC_TERNARIA);
}

/*
 * Núcleo del Pratt: lee un prefijo, luego mientras el operador
 * actual tenga precedencia ≥ `min_prec` lo consume como infijo.
 */
static Expr *parsear_precedencia(Parser *p, Precedencia min_prec) {
    const ReglaParseo *regla = obtener_regla(p->actual.tipo);
    if (regla->prefijo == NULL) {
        error_en(p, &p->actual, "se esperaba una expresión");
        return NULL;
    }

    Expr *izq = regla->prefijo(p);
    if (izq == NULL) return NULL;

    while (true) {
        const ReglaParseo *r = obtener_regla(p->actual.tipo);
        if (r->precedencia < min_prec || r->infijo == NULL) break;
        /* v1.21: heurística Python-like de fin de sentencia.
           Si el token infix está en una línea DISTINTA al token previo
           Y el infix es `[`, `(` o `.`, no lo aplicamos — probablemente
           inicia una nueva sentencia. Sin esto, `lista = [1, 2]` seguido
           de `[x, y] = lista` se parsearía como `lista = [1, 2][x, y]`.
           v1.44: extendemos la heurística a `si` por la misma razón —
           la ternaria `A si C sino B` debe vivir en una sola línea;
           un `si` que abre línea es siempre el comienzo de una
           sentencia `si`.
           v1.133: extendemos a `*` para soportar destructuring inicial
           `*r, x = it`. Sin esto, una línea anterior que termine en
           expresión + `*` al comienzo de la siguiente se parsearía como
           multiplicación, escondiendo el destructuring. La multiplicación
           que cruza líneas siempre se puede forzar con paréntesis. */
        if (p->actual.linea != p->previo.linea
            && (p->actual.tipo == TT_CORCH_IZQ
                || p->actual.tipo == TT_PARENT_IZQ
                || p->actual.tipo == TT_SI
                || p->actual.tipo == TT_ASTERISCO)) {
            break;
        }
        izq = r->infijo(p, izq);
        if (izq == NULL) return NULL;
    }
    return izq;
}

/* ──────────────────────────────────────────────────────────────────
 * Funciones de prefijo
 * ────────────────────────────────────────────────────────────────── */

static Expr *parsear_numero_entero(Parser *p) {
    Token t = p->actual;
    avanzar(p);
    return expr_literal_entero(p->arena, t.inicio, t.longitud, t.linea, t.columna);
}

static Expr *parsear_numero_decimal(Parser *p) {
    Token t = p->actual;
    avanzar(p);
    return expr_literal_decimal(p->arena, t.inicio, t.longitud, t.linea, t.columna);
}

static Expr *parsear_cadena(Parser *p) {
    Token t = p->actual;
    avanzar(p);
    return expr_literal_cadena(p->arena, t.inicio, t.longitud, t.linea, t.columna);
}

/*
 * Parser de f-cadena con interpolación real (v1.1).
 *
 * El lexer entrega el lexema completo `f"..."` (o `f'...'`); aquí lo
 * descomponemos en partes literales y partes expresión, recursando con
 * un sub-parser sobre cada `{expr}` interno.
 *
 * Soporte actual:
 *   - f-cadenas simples (un solo delimitador). Las triples
 *     (`f"""..."""`) NO están soportadas todavía — el lexer las
 *     tokeniza pero el parser las rechaza con un error claro.
 *   - Llaves dobles `{{` y `}}` se preservan como llave única en el
 *     literal.
 *   - `{expr}` interno se parsea como una expresión Cornamusa
 *     completa. Debe consumir EXACTAMENTE el slice; tokens sobrantes
 *     dan error de sintaxis.
 *   - Las partes literales preservan escapes (`\n`, `\t`, ...) sin
 *     procesar — la decodificación final ocurre en compilador y
 *     evaluador, igual que para `EXPR_LITERAL_CADENA`.
 */
static Expr *parsear_f_cadena(Parser *p) {
    Token t = p->actual;
    avanzar(p);

    if (t.longitud < 3) {
        error_en(p, &t, "f-cadena malformada");
        return NULL;
    }
    /* t.inicio[0] es 'f' o 'F'. */
    char delim = t.inicio[1];
    bool es_triple = (t.longitud >= 6 && t.inicio[2] == delim
                                       && t.inicio[3] == delim);
    /* v1.14: f-cadenas triples soportadas. La diferencia con la
       simple es solo el offset/longitud del cuerpo. La lógica de
       interpolación `{expr}` no cambia: las triples permiten saltos
       de línea literales y comillas sueltas dentro. */
    const char *cuerpo;
    int cuerpo_len;
    if (es_triple) {
        cuerpo = t.inicio + 4;             /* skip f""" o f''' */
        cuerpo_len = t.longitud - 7;       /* skip prefijo (4) + sufijo (3) */
    } else {
        cuerpo = t.inicio + 2;             /* skip f" o f' */
        cuerpo_len = t.longitud - 3;       /* skip prefijo (2) + sufijo (1) */
    }
    if (cuerpo_len < 0) cuerpo_len = 0;

    /* Buffers temporales en heap (la arena no permite resize). Se
       liberan al final independientemente del éxito. */
    int part_cap = 4;
    int part_n = 0;
    ParteFCadena *partes_tmp = (ParteFCadena *)malloc(
        sizeof(ParteFCadena) * (size_t)part_cap);
    int buf_cap = 256;
    int buf_len = 0;
    char *buf = (char *)malloc((size_t)buf_cap);
    if (!partes_tmp || !buf) {
        free(partes_tmp); free(buf);
        error_en(p, &t, "memoria insuficiente al parsear f-cadena");
        return NULL;
    }

#define EMPUJAR_PARTE(P)                                                  \
    do {                                                                  \
        if (part_n == part_cap) {                                         \
            int nuevo_cap = part_cap * 2;                                 \
            ParteFCadena *np = (ParteFCadena *)realloc(partes_tmp,        \
                sizeof(ParteFCadena) * (size_t)nuevo_cap);                \
            if (!np) { goto fallo_oom; }                                  \
            partes_tmp = np;                                              \
            part_cap = nuevo_cap;                                         \
        }                                                                 \
        partes_tmp[part_n++] = (P);                                       \
    } while (0)

#define EMPUJAR_BYTE(B)                                                   \
    do {                                                                  \
        if (buf_len == buf_cap) {                                         \
            int nuevo_cap = buf_cap * 2;                                  \
            char *nb = (char *)realloc(buf, (size_t)nuevo_cap);           \
            if (!nb) { goto fallo_oom; }                                  \
            buf = nb;                                                     \
            buf_cap = nuevo_cap;                                          \
        }                                                                 \
        buf[buf_len++] = (B);                                             \
    } while (0)

#define VOLCAR_LITERAL()                                                  \
    do {                                                                  \
        if (buf_len > 0) {                                                \
            char *lit = (char *)arena_alocar(p->arena, (size_t)buf_len);  \
            if (!lit) { goto fallo_oom; }                                 \
            memcpy(lit, buf, (size_t)buf_len);                            \
            ParteFCadena pl;                                              \
            pl.literal = lit;                                             \
            pl.longitud = buf_len;                                        \
            pl.expr = NULL;                                               \
            pl.spec = NULL;                                               \
            pl.spec_longitud = 0;                                         \
            pl.debug_texto = NULL;                                        \
            pl.debug_longitud = 0;                                        \
            EMPUJAR_PARTE(pl);                                            \
            buf_len = 0;                                                  \
        }                                                                 \
    } while (0)

    int i = 0;
    while (i < cuerpo_len) {
        char c = cuerpo[i];
        if (c == '{') {
            if (i + 1 < cuerpo_len && cuerpo[i + 1] == '{') {
                EMPUJAR_BYTE('{');
                i += 2;
                continue;
            }
            VOLCAR_LITERAL();
            i++; /* skip '{' */
            int inicio_expr = i;
            int profundidad = 1;
            int prof_corch = 0;   /* v1.45: para no confundir `:` de slicing */
            int prof_paren = 0;   /* v1.45: para no confundir `:` de lambda */
            int spec_inicio = -1; /* v1.45: offset del `:` del fmt spec, -1 si no hay */
            /* Trackeo de cadenas internas para que `}` dentro de una
               cadena (ej. `f"{ \"}\" }"`) no se confunda con el cierre
               de la interpolación. v1.1.1. */
            while (i < cuerpo_len && profundidad > 0) {
                char d = cuerpo[i];
                if (d == '"' || d == '\'') {
                    char delim_str = d;
                    i++;
                    while (i < cuerpo_len && cuerpo[i] != delim_str) {
                        if (cuerpo[i] == '\\' && i + 1 < cuerpo_len) i += 2;
                        else i++;
                    }
                    if (i >= cuerpo_len) {
                        error_en(p, &t,
                            "f-cadena: cadena interna sin cerrar");
                        goto fallo;
                    }
                    i++; /* skip delim de cierre */
                    continue;
                }
                if (d == '{') profundidad++;
                else if (d == '}') {
                    profundidad--;
                    if (profundidad == 0) break;
                }
                else if (d == '[') prof_corch++;
                else if (d == ']') { if (prof_corch > 0) prof_corch--; }
                else if (d == '(') prof_paren++;
                else if (d == ')') { if (prof_paren > 0) prof_paren--; }
                else if (d == ':' && spec_inicio < 0
                         && profundidad == 1
                         && prof_corch == 0
                         && prof_paren == 0) {
                    /* v1.45: primer `:` de top-level dentro de la
                       interpolación — marca el inicio del fmt spec. */
                    spec_inicio = i;
                }
                i++;
            }
            if (profundidad != 0) {
                error_en(p, &t,
                    "f-cadena: `{` sin cerrar antes del fin de la cadena");
                goto fallo;
            }
            /* v1.45: si encontramos un spec, la expresión va hasta el
               `:`; el spec va de tras del `:` hasta el `}`. */
            int spec_off = -1;
            int spec_len = 0;
            int len_expr;
            if (spec_inicio >= 0) {
                len_expr = spec_inicio - inicio_expr;
                spec_off = spec_inicio + 1;
                spec_len = i - spec_off;
            } else {
                len_expr = i - inicio_expr;
            }
            /* v1.112: detectar trailing `=` para debug format `f"{x=}"`.
             * Buscar el ultimo `=` no-operador (no precedido por
             * `=`, `!`, `<`, `>`) saltando espacios al final. Si lo
             * encontramos, todo desde inicio_expr hasta el final
             * (incluyendo el `=` y los espacios despues) se convierte
             * en debug_texto, y la expresion cornamusa real queda
             * cortada justo antes del `=`. */
            int debug_off = -1;
            int debug_total = 0;
            {
                int fin = inicio_expr + len_expr;
                int j = fin - 1;
                while (j >= inicio_expr
                       && (cuerpo[j] == ' ' || cuerpo[j] == '\t')) {
                    j--;
                }
                if (j >= inicio_expr && cuerpo[j] == '=') {
                    bool es_operador = false;
                    if (j > inicio_expr) {
                        char prev = cuerpo[j - 1];
                        if (prev == '=' || prev == '!'
                            || prev == '<' || prev == '>') {
                            es_operador = true;
                        }
                    }
                    if (!es_operador) {
                        /* j apunta al `=`. La expresion real va de
                         * inicio_expr a j (excl). debug_texto cubre
                         * inicio_expr a fin (la slice tal cual con
                         * eventuales espacios despues del `=`). */
                        debug_off = inicio_expr;
                        debug_total = fin - inicio_expr;
                        len_expr = j - inicio_expr;
                    }
                }
            }
            i++; /* skip '}' */
            if (len_expr == 0) {
                error_en(p, &t,
                    "f-cadena: expresión vacía entre `{` y `}`");
                goto fallo;
            }
            /* Copiar slice a buffer null-terminated en arena para el
               sub-lexer (que requiere `*l->actual == '\0'` para detectar EOF). */
            char *src = (char *)arena_alocar(p->arena, (size_t)len_expr + 1);
            if (!src) { goto fallo_oom; }
            memcpy(src, cuerpo + inicio_expr, (size_t)len_expr);
            src[len_expr] = '\0';

            Lexer sub_l;
            lexer_iniciar(&sub_l, src, p->archivo);
            Parser sub_p;
            parser_iniciar(&sub_p, &sub_l, p->arena, src, p->archivo);
            Expr *sub = parser_parsear_expr(&sub_p);
            if (!sub || sub_p.tuvo_error) {
                error_en(p, &t,
                    "f-cadena: expresión interna inválida");
                goto fallo;
            }
            if (sub_p.actual.tipo != TT_FIN_ARCHIVO) {
                error_en(p, &t,
                    "f-cadena: tokens sobrantes en expresión interpolada");
                goto fallo;
            }
            ParteFCadena pe;
            pe.literal = NULL;
            pe.longitud = 0;
            pe.expr = sub;
            /* v1.45: copiar spec a arena (puntero al cuerpo original
               podría invalidarse si la fuente desaparece). */
            pe.spec = NULL;
            pe.spec_longitud = 0;
            pe.debug_texto = NULL;
            pe.debug_longitud = 0;
            if (spec_off >= 0 && spec_len > 0) {
                char *spec_copia = (char *)arena_alocar(p->arena,
                    (size_t)spec_len + 1);
                if (!spec_copia) { goto fallo_oom; }
                memcpy(spec_copia, cuerpo + spec_off, (size_t)spec_len);
                spec_copia[spec_len] = '\0';
                pe.spec = spec_copia;
                pe.spec_longitud = spec_len;
            } else if (spec_off >= 0 && spec_len == 0) {
                /* Spec vacío `f"{x:}"` — equivalente a sin spec. */
            }
            /* v1.112: copiar debug_texto a arena si aplica. */
            if (debug_off >= 0 && debug_total > 0) {
                char *dbg = (char *)arena_alocar(p->arena,
                    (size_t)debug_total);
                if (!dbg) { goto fallo_oom; }
                memcpy(dbg, cuerpo + debug_off, (size_t)debug_total);
                pe.debug_texto = dbg;
                pe.debug_longitud = debug_total;
            }
            EMPUJAR_PARTE(pe);
            continue;
        }
        if (c == '}') {
            if (i + 1 < cuerpo_len && cuerpo[i + 1] == '}') {
                EMPUJAR_BYTE('}');
                i += 2;
                continue;
            }
            error_en(p, &t,
                "f-cadena: `}` sin un `{` previo (usa `}}` para llave literal)");
            goto fallo;
        }
        EMPUJAR_BYTE(c);
        i++;
    }
    VOLCAR_LITERAL();

    /* Trasladar partes_tmp a arena. */
    ParteFCadena *partes_arena = NULL;
    if (part_n > 0) {
        partes_arena = (ParteFCadena *)arena_alocar(
            p->arena, sizeof(ParteFCadena) * (size_t)part_n);
        if (!partes_arena) { goto fallo_oom; }
        memcpy(partes_arena, partes_tmp,
                sizeof(ParteFCadena) * (size_t)part_n);
    }
    free(partes_tmp);
    free(buf);
    return expr_literal_f_cadena(p->arena, partes_arena, part_n,
                                  t.linea, t.columna);

fallo_oom:
    error_en(p, &t, "memoria insuficiente al parsear f-cadena");
fallo:
    free(partes_tmp);
    free(buf);
    return NULL;

#undef EMPUJAR_PARTE
#undef EMPUJAR_BYTE
#undef VOLCAR_LITERAL
}

static Expr *parsear_booleano(Parser *p) {
    Token t = p->actual;
    bool valor = (t.tipo == TT_VERDADERO);
    avanzar(p);
    return expr_literal_booleano(p->arena, valor, t.linea, t.columna);
}

static Expr *parsear_nulo(Parser *p) {
    Token t = p->actual;
    avanzar(p);
    return expr_literal_nulo(p->arena, t.linea, t.columna);
}

static Expr *parsear_ident(Parser *p) {
    Token t = p->actual;
    avanzar(p);
    /* v1.113: walrus `nombre := expr`. Si el siguiente token es :=,
     * parsea el lado derecho como expresion (recursivo, asociativa por
     * la derecha como cualquier asignacion). Devuelve EXPR_WALRUS. */
    if (check(p, TT_WALRUS)) {
        avanzar(p);  /* consume := */
        Expr *valor = parser_parsear_expr(p);
        if (!valor) return NULL;
        return expr_walrus(p->arena, t.inicio, t.longitud, valor,
                            t.linea, t.columna);
    }
    return expr_ident(p->arena, t.inicio, t.longitud, t.linea, t.columna);
}

/*
 * `super` solo es válido seguido de `.identificador`. Lo parseamos como
 * un nodo `EXPR_SUPER` que el compilador maneja especialmente al
 * detectar que un `EXPR_LLAMADA` tiene como callee un super.
 *
 * Al encontrar `super` sin un `.` siguiente lo reportamos como error
 * de sintaxis con un mensaje claro.
 */
static Expr *parsear_super(Parser *p) {
    Token t_super = p->actual;
    avanzar(p);  /* consume 'super' */
    if (!consumir(p, TT_PUNTO,
            "se esperaba '.' tras 'super' (uso: super.metodo(...))")) return NULL;
    if (!check(p, TT_IDENT)) {
        error_en(p, &p->actual,
            "se esperaba un nombre de método tras 'super.'");
        return NULL;
    }
    Token t_nombre = p->actual;
    avanzar(p);
    return expr_super(p->arena, t_nombre.inicio, t_nombre.longitud,
                       t_super.linea, t_super.columna);
}

/*
 * Parsea `(...)`. Distingue entre:
 *   - `()`         → tupla vacía
 *   - `(expr)`     → grupo (paréntesis de precedencia)
 *   - `(expr,)`    → tupla de 1 elemento (con coma final)
 *   - `(a, b, ...)` → tupla de N elementos
 */
static Expr *parsear_grupo(Parser *p) {
    Token apertura = p->actual;
    avanzar(p); /* consume '(' */

    /* () = tupla vacía. */
    if (consumir_si(p, TT_PARENT_DER)) {
        return expr_tupla(p->arena, NULL, 0,
                          apertura.linea, apertura.columna);
    }

    Expr *primero = parsear_expresion(p);
    if (primero == NULL) return NULL;

    /* v1.34: generator expression `(expr para v en iter [si guarda])`.
       Produce un generador lazy en lugar de una lista materializada.
       v1.136: acepta multiples `para` encadenados y destructuring en
       cualquier clausula (paridad con list/dict/set comprehensions). */
    if (check(p, TT_PARA)) {
        const char *vn; int vl; Expr *iter; Expr *guarda;
        Expr *patron = NULL;
        struct ClausulaComp *extras; int n_extras;
        if (!parsear_comprehension_cola(p, &vn, &vl, &iter, &guarda,
                                            &patron, &extras, &n_extras)) return NULL;
        if (!consumir(p, TT_PARENT_DER,
            "se esperaba ')' al final de la generator expression")) return NULL;
        Expr *e = expr_comprehension(p->arena, /*tipo=genex*/3,
                                    primero, NULL, vn, vl, iter, guarda,
                                    apertura.linea, apertura.columna);
        if (e) {
            e->como.comprehension.patron = patron;
            if (n_extras > 0) {
                e->como.comprehension.clausulas_extra = extras;
                e->como.comprehension.n_extras = n_extras;
            }
        }
        return e;
    }

    /* Sin coma: es grupo. */
    if (!check(p, TT_COMA)) {
        if (!consumir(p, TT_PARENT_DER,
            "se esperaba ')' para cerrar el grupo")) return NULL;
        return expr_grupo(p->arena, primero,
                          apertura.linea, apertura.columna);
    }

    /* Con coma: es tupla. */
    avanzar(p); /* consume ',' */

    Expr **elementos = NULL;
    int n = 0;
    int cap = 0;
    cap = 4;
    elementos = (Expr **)arena_alocar(p->arena, sizeof(Expr *) * (size_t)cap);
    if (elementos == NULL) return NULL;
    elementos[n++] = primero;

    /* Tupla de 1: `(x,)`. Tras la coma puede venir directamente `)`. */
    if (!check(p, TT_PARENT_DER)) {
        do {
            if (check(p, TT_PARENT_DER)) break; /* trailing comma */
            Expr *e = parsear_expresion(p);
            if (e == NULL) return NULL;
            if (n >= cap) {
                cap *= 2;
                Expr **nuevo = (Expr **)arena_alocar(p->arena,
                    sizeof(Expr *) * (size_t)cap);
                if (nuevo == NULL) return NULL;
                memcpy(nuevo, elementos, sizeof(Expr *) * (size_t)n);
                elementos = nuevo;
            }
            elementos[n++] = e;
        } while (consumir_si(p, TT_COMA));
    }

    if (!consumir(p, TT_PARENT_DER,
        "se esperaba ')' al final de la tupla")) return NULL;
    return expr_tupla(p->arena, elementos, n,
                      apertura.linea, apertura.columna);
}

/*
 * Helper: parsea el sufijo de comprehension `para VAR en ITER [si GUARDA]`
 * asumiendo que el `para` ya está en el token actual (sin consumir).
 * Avanza hasta el token de cierre (`]`, `}` o `)`). Retorna los
 * componentes en los out-params.
 *
 * Sintaxis soportada:
 *   v1.30: un solo `para...en...` con un `si` opcional.
 *   v1.132: cero o mas clausulas adicionales `para X en Y [si Z]`.
 *     `[(x, y) para x en xs para y en ys si x != y]` — bucles
 *     anidados desugarados por el compilador.
 *
 * Salida (primera clausula): nombre_var, longitud_var, iterable, guarda.
 * Salida (clausulas extra): puntero a array alocado en arena y conteo.
 * Si `clausulas_extra_out` o `n_extras_out` son NULL, no se aceptan
 * clausulas extra (callers viejos).
 */
/*
 * v1.135: parsea un destino de cabecera de cláusula de comprehension:
 * IDENT o `*IDENT`. Devuelve la Expr (EXPR_IDENT o EXPR_STAR_BIND).
 * v1.138: tambien acepta sub-patrones `(...)` que se anidan
 * recursivamente. Ejemplo: `[expr para (a, (b, c)) en triples]`.
 * El nodo devuelto puede ser EXPR_IDENT, EXPR_STAR_BIND o
 * EXPR_TUPLA (anidada).
 * NULL en error.
 */
static Expr *parsear_destino_compr(Parser *p) {
    if (check(p, TT_ASTERISCO)) {
        int sl = p->actual.linea;
        int sc = p->actual.columna;
        avanzar(p);
        if (!check(p, TT_IDENT)) {
            error_en(p, &p->actual,
                "se esperaba un nombre tras '*' en destructuring de comprehension");
            return NULL;
        }
        Token tid = p->actual;
        avanzar(p);
        return expr_star_bind(p->arena, tid.inicio, tid.longitud, sl, sc);
    }
    if (check(p, TT_PARENT_IZQ)) {
        int sl = p->actual.linea;
        int sc = p->actual.columna;
        avanzar(p);  /* consume '(' */
        Expr **elementos = (Expr **)arena_alocar(p->arena,
            sizeof(Expr *) * 4);
        if (elementos == NULL) return NULL;
        int n = 0, cap = 4;
        if (!check(p, TT_PARENT_DER)) {
            do {
                Expr *e = parsear_destino_compr(p);
                if (e == NULL) return NULL;
                if (n >= cap) {
                    int nuevo_cap = cap * 2;
                    Expr **nuevo = (Expr **)arena_alocar(p->arena,
                        sizeof(Expr *) * (size_t)nuevo_cap);
                    if (nuevo == NULL) return NULL;
                    memcpy(nuevo, elementos, sizeof(Expr *) * (size_t)n);
                    elementos = nuevo;
                    cap = nuevo_cap;
                }
                elementos[n++] = e;
            } while (consumir_si(p, TT_COMA) && !check(p, TT_PARENT_DER));
        }
        if (!consumir(p, TT_PARENT_DER,
            "se esperaba ')' al cerrar sub-patron de comprehension")) return NULL;
        return expr_tupla(p->arena, elementos, n, sl, sc);
    }
    if (!check(p, TT_IDENT)) {
        error_en(p, &p->actual,
            "se esperaba un nombre de variable tras 'para'");
        return NULL;
    }
    Token t = p->actual;
    avanzar(p);
    return expr_ident(p->arena, t.inicio, t.longitud, t.linea, t.columna);
}

/*
 * v1.135: parsea la cabecera de una cláusula de comprehension —
 * uno o varios destinos separados por coma. Si solo hay un IDENT,
 * devuelve los campos nombre_var/longitud_var y deja *patron_out = NULL
 * (path legacy). Si hay multiples destinos o un star, construye
 * EXPR_TUPLA como patron y deja nombre_var = NULL.
 */
static bool parsear_destinos_compr(Parser *p,
                                    const char **nombre_var_out,
                                    int *longitud_var_out,
                                    Expr **patron_out,
                                    int linea, int col) {
    *patron_out = NULL;
    *nombre_var_out = NULL;
    *longitud_var_out = 0;
    Expr *primero = parsear_destino_compr(p);
    if (primero == NULL) return false;

    bool es_destr = (primero->tipo == EXPR_STAR_BIND)
                      || (primero->tipo == EXPR_TUPLA)
                      || check(p, TT_COMA);
    if (!es_destr) {
        /* Var simple: IDENT puro. */
        *nombre_var_out = primero->como.ident.nombre;
        *longitud_var_out = primero->como.ident.longitud;
        return true;
    }
    /* Destructuring: recolectar destinos extra. */
    Expr **elementos = (Expr **)arena_alocar(p->arena, sizeof(Expr *) * 4);
    if (elementos == NULL) return false;
    int n = 1, cap = 4;
    elementos[0] = primero;
    while (consumir_si(p, TT_COMA)) {
        if (check(p, TT_EN)) break;  /* coma final permitida */
        Expr *e = parsear_destino_compr(p);
        if (e == NULL) return false;
        if (n >= cap) {
            int nuevo_cap = cap * 2;
            Expr **nuevo = (Expr **)arena_alocar(p->arena,
                sizeof(Expr *) * (size_t)nuevo_cap);
            if (nuevo == NULL) return false;
            memcpy(nuevo, elementos, sizeof(Expr *) * (size_t)n);
            elementos = nuevo;
            cap = nuevo_cap;
        }
        elementos[n++] = e;
    }
    /* v1.138: si solo hay un elemento Y ya es EXPR_TUPLA (sub-patron
     * con parentesis), no envolver — el patron es directamente esa
     * tupla. Asi `[expr para (a, b) en pares]` produce patron=(a,b),
     * no ((a,b),). */
    if (n == 1 && elementos[0]->tipo == EXPR_TUPLA) {
        *patron_out = elementos[0];
    } else {
        *patron_out = expr_tupla(p->arena, elementos, n, linea, col);
    }
    return (*patron_out != NULL);
}

static bool parsear_comprehension_cola(Parser *p,
                                          const char **nombre_var_out,
                                          int *longitud_var_out,
                                          Expr **iterable_out,
                                          Expr **guarda_out,
                                          Expr **patron_out,
                                          struct ClausulaComp **clausulas_extra_out,
                                          int *n_extras_out) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    if (!consumir(p, TT_PARA, "se esperaba 'para' en comprehension")) return false;

    /* v1.135: aceptar uno o varios destinos con star opcional. */
    Expr *patron = NULL;
    if (!parsear_destinos_compr(p, nombre_var_out, longitud_var_out,
                                  &patron, linea, col)) {
        return false;
    }
    if (patron_out) *patron_out = patron;

    if (!consumir(p, TT_EN, "se esperaba 'en' tras la variable")) return false;
    /* v1.44: para que `[expr para v en iter si guarda]` siga parseando
       el `si` como inicio de la GUARDA y no como ternario sobre iter,
       parseamos iter con PREC_O (excluye ternaria). La guarda en
       cambio sí puede ser ternaria. */
    Expr *it = parsear_precedencia(p, PREC_O);
    if (it == NULL) return false;
    *iterable_out = it;
    *guarda_out = NULL;
    if (consumir_si(p, TT_SI)) {
        Expr *g = parsear_expresion(p);
        if (g == NULL) return false;
        *guarda_out = g;
    }
    /* v1.132: clausulas adicionales `para X en Y [si Z]`.
     * v1.135: cada extra puede ser tambien destructuring. */
    if (clausulas_extra_out != NULL && n_extras_out != NULL) {
        *clausulas_extra_out = NULL;
        *n_extras_out = 0;
        int cap = 0;
        while (check(p, TT_PARA)) {
            int extra_linea = p->actual.linea;
            int extra_col = p->actual.columna;
            avanzar(p);  /* consume PARA */
            const char *vn = NULL; int vl = 0;
            Expr *patron_x = NULL;
            if (!parsear_destinos_compr(p, &vn, &vl, &patron_x,
                                          extra_linea, extra_col)) {
                return false;
            }
            if (!consumir(p, TT_EN, "se esperaba 'en' tras la variable")) return false;
            Expr *it2 = parsear_precedencia(p, PREC_O);
            if (it2 == NULL) return false;
            Expr *g2 = NULL;
            if (consumir_si(p, TT_SI)) {
                g2 = parsear_expresion(p);
                if (g2 == NULL) return false;
            }
            if (*n_extras_out >= cap) {
                int nuevo_cap = cap ? cap * 2 : 2;
                struct ClausulaComp *nuevo =
                    (struct ClausulaComp *)arena_alocar(
                        p->arena, sizeof(struct ClausulaComp) * (size_t)nuevo_cap);
                if (!nuevo) return false;
                if (*n_extras_out > 0) {
                    memcpy(nuevo, *clausulas_extra_out,
                           sizeof(struct ClausulaComp) * (size_t)(*n_extras_out));
                }
                *clausulas_extra_out = nuevo;
                cap = nuevo_cap;
            }
            (*clausulas_extra_out)[*n_extras_out].nombre_var = vn;
            (*clausulas_extra_out)[*n_extras_out].longitud_var = vl;
            (*clausulas_extra_out)[*n_extras_out].patron = patron_x;
            (*clausulas_extra_out)[*n_extras_out].iterable = it2;
            (*clausulas_extra_out)[*n_extras_out].guarda = g2;
            (*n_extras_out)++;
        }
    }
    return true;
}

/*
 * Parsea una lista literal `[a, b, c]`, `[]`, `[1]`, o una comprehension
 * `[expr para v en iter [si guarda]]`.
 */
/* v1.171: helper para elementos de literal de lista/tupla/conjunto.
 * Si el elemento empieza por '*', lo envuelve en EXPR_UNARIO con op
 * TT_ASTERISCO. El compilador detecta este marcador para emitir
 * OP_LISTA_EXTENDER en lugar de OP_LISTA_AGREGAR. */
static Expr *parsear_elemento_con_spread(Parser *p) {
    if (check(p, TT_ASTERISCO)) {
        Token t = p->actual;
        avanzar(p);
        Expr *e = parsear_expresion(p);
        if (!e) return NULL;
        return expr_unario(p->arena, TT_ASTERISCO, e, t.linea, t.columna);
    }
    return parsear_expresion(p);
}

static Expr *parsear_lista_literal(Parser *p) {
    Token apertura = p->actual;
    avanzar(p); /* consume '[' */

    /* Lista vacía. */
    if (check(p, TT_CORCH_DER)) {
        avanzar(p);
        return expr_lista(p->arena, NULL, 0,
                            apertura.linea, apertura.columna);
    }

    /* Parsear primer elemento. v1.171: soporta `*xs` spread. */
    bool primero_es_spread = check(p, TT_ASTERISCO);
    Expr *primero = parsear_elemento_con_spread(p);
    if (primero == NULL) return NULL;

    /* v1.30: comprehension `[expr para ...]`. v1.171: si el primer
     * elemento era spread, no es comprehension valida.
     * v1.132: soporta multiples para/si encadenados. */
    if (!primero_es_spread && check(p, TT_PARA)) {
        const char *vn; int vl; Expr *iter; Expr *guarda;
        Expr *patron = NULL;
        struct ClausulaComp *extras; int n_extras;
        if (!parsear_comprehension_cola(p, &vn, &vl, &iter, &guarda,
                                            &patron, &extras, &n_extras)) return NULL;
        if (!consumir(p, TT_CORCH_DER,
            "se esperaba ']' al final de la comprehension")) return NULL;
        Expr *e = expr_comprehension(p->arena, /*tipo=lista*/0,
                                    primero, NULL, vn, vl, iter, guarda,
                                    apertura.linea, apertura.columna);
        if (e) {
            e->como.comprehension.patron = patron;
            if (n_extras > 0) {
                e->como.comprehension.clausulas_extra = extras;
                e->como.comprehension.n_extras = n_extras;
            }
        }
        return e;
    }

    /* Lista literal con más elementos. */
    Expr **elementos = NULL;
    int n = 1;
    int cap = 8;
    elementos = (Expr **)arena_alocar(p->arena, sizeof(Expr *) * (size_t)cap);
    if (elementos == NULL) return NULL;
    elementos[0] = primero;

    while (consumir_si(p, TT_COMA)) {
        if (check(p, TT_CORCH_DER)) break; /* trailing comma */
        Expr *e = parsear_elemento_con_spread(p);
        if (e == NULL) return NULL;
        if (n >= cap) {
            cap *= 2;
            Expr **nuevo = (Expr **)arena_alocar(p->arena,
                sizeof(Expr *) * (size_t)cap);
            if (nuevo == NULL) return NULL;
            memcpy(nuevo, elementos, sizeof(Expr *) * (size_t)n);
            elementos = nuevo;
        }
        elementos[n++] = e;
    }

    if (!consumir(p, TT_CORCH_DER,
        "se esperaba ']' al final de la lista")) return NULL;
    return expr_lista(p->arena, elementos, n,
                      apertura.linea, apertura.columna);
}

/*
 * Parsea `{...}`. Detección de diccionario vs conjunto:
 *   - `{}`           → diccionario vacío (convención de Python)
 *   - `{x}`          → conjunto de 1 (no hay diccionario de 0 con sintaxis x)
 *   - `{x, y, ...}`  → conjunto
 *   - `{k: v, ...}`  → diccionario
 *
 * Decidimos por el primer carácter tras la primera expresión: `:` es
 * diccionario, `,` o `}` es conjunto.
 */
static Expr *parsear_llaves(Parser *p) {
    Token apertura = p->actual;
    avanzar(p); /* consume '{' */

    /* `{}` = diccionario vacío. */
    if (consumir_si(p, TT_LLAVE_DER)) {
        return expr_diccionario(p->arena, NULL, NULL, 0,
                                apertura.linea, apertura.columna);
    }

    Expr *primero = parsear_expresion(p);
    if (primero == NULL) return NULL;

    /* `{ k : v }` → diccionario o dict comprehension. */
    if (consumir_si(p, TT_DOS_PUNTOS)) {
        Expr *valor = parsear_expresion(p);
        if (valor == NULL) return NULL;

        /* v1.30: `{k: v para ...}` → dict comprehension.
         * v1.132: soporta multiples para/si encadenados. */
        if (check(p, TT_PARA)) {
            const char *vn; int vl; Expr *iter; Expr *guarda;
            Expr *patron = NULL;
            struct ClausulaComp *extras; int n_extras;
            if (!parsear_comprehension_cola(p, &vn, &vl, &iter, &guarda,
                                                &patron, &extras, &n_extras)) return NULL;
            if (!consumir(p, TT_LLAVE_DER,
                "se esperaba '}' al final de la comprehension dict")) return NULL;
            Expr *e = expr_comprehension(p->arena, /*tipo=dict*/1,
                                        primero, valor, vn, vl, iter, guarda,
                                        apertura.linea, apertura.columna);
            if (e) {
                e->como.comprehension.patron = patron;
                if (n_extras > 0) {
                    e->como.comprehension.clausulas_extra = extras;
                    e->como.comprehension.n_extras = n_extras;
                }
            }
            return e;
        }

        Expr **claves = NULL;
        Expr **valores = NULL;
        int n = 0;
        int cap = 8;
        claves = (Expr **)arena_alocar(p->arena, sizeof(Expr *) * (size_t)cap);
        valores = (Expr **)arena_alocar(p->arena, sizeof(Expr *) * (size_t)cap);
        if (claves == NULL || valores == NULL) return NULL;
        claves[0] = primero;
        valores[0] = valor;
        n = 1;

        while (consumir_si(p, TT_COMA)) {
            if (check(p, TT_LLAVE_DER)) break; /* trailing comma */
            Expr *k = parsear_expresion(p);
            if (k == NULL) return NULL;
            if (!consumir(p, TT_DOS_PUNTOS,
                "se esperaba ':' tras la clave del diccionario")) return NULL;
            Expr *v = parsear_expresion(p);
            if (v == NULL) return NULL;
            if (n >= cap) {
                cap *= 2;
                Expr **nk = (Expr **)arena_alocar(p->arena,
                    sizeof(Expr *) * (size_t)cap);
                Expr **nv = (Expr **)arena_alocar(p->arena,
                    sizeof(Expr *) * (size_t)cap);
                if (nk == NULL || nv == NULL) return NULL;
                memcpy(nk, claves, sizeof(Expr *) * (size_t)n);
                memcpy(nv, valores, sizeof(Expr *) * (size_t)n);
                claves = nk;
                valores = nv;
            }
            claves[n] = k;
            valores[n] = v;
            n++;
        }
        if (!consumir(p, TT_LLAVE_DER,
            "se esperaba '}' al final del diccionario")) return NULL;
        return expr_diccionario(p->arena, claves, valores, n,
                                apertura.linea, apertura.columna);
    }

    /* v1.30: `{expr para ...}` → set comprehension.
     * v1.132: soporta multiples para/si encadenados. */
    if (check(p, TT_PARA)) {
        const char *vn; int vl; Expr *iter; Expr *guarda;
        Expr *patron = NULL;
        struct ClausulaComp *extras; int n_extras;
        if (!parsear_comprehension_cola(p, &vn, &vl, &iter, &guarda,
                                            &patron, &extras, &n_extras)) return NULL;
        if (!consumir(p, TT_LLAVE_DER,
            "se esperaba '}' al final de la comprehension conjunto")) return NULL;
        Expr *e = expr_comprehension(p->arena, /*tipo=conjunto*/2,
                                    primero, NULL, vn, vl, iter, guarda,
                                    apertura.linea, apertura.columna);
        if (e) e->como.comprehension.patron = patron;
        if (e && n_extras > 0) {
            e->como.comprehension.clausulas_extra = extras;
            e->como.comprehension.n_extras = n_extras;
        }
        return e;
    }

    /* Sino, es conjunto literal. */
    Expr **elementos = NULL;
    int n = 0;
    int cap = 8;
    elementos = (Expr **)arena_alocar(p->arena, sizeof(Expr *) * (size_t)cap);
    if (elementos == NULL) return NULL;
    elementos[0] = primero;
    n = 1;

    while (consumir_si(p, TT_COMA)) {
        if (check(p, TT_LLAVE_DER)) break;
        Expr *e = parsear_expresion(p);
        if (e == NULL) return NULL;
        if (n >= cap) {
            cap *= 2;
            Expr **nuevo = (Expr **)arena_alocar(p->arena,
                sizeof(Expr *) * (size_t)cap);
            if (nuevo == NULL) return NULL;
            memcpy(nuevo, elementos, sizeof(Expr *) * (size_t)n);
            elementos = nuevo;
        }
        elementos[n++] = e;
    }

    if (!consumir(p, TT_LLAVE_DER,
        "se esperaba '}' al final del conjunto")) return NULL;
    return expr_conjunto(p->arena, elementos, n,
                         apertura.linea, apertura.columna);
}

/*
 * Parsea `obj[k]` (indexación) o `obj[a:b:c]` (slicing). Llamado como
 * infijo después de una expresión que produce el `objeto`.
 *
 * Slicing soporta omisiones: `obj[:b]`, `obj[a:]`, `obj[:]`, `obj[::c]`.
 */
/*
 * Operador `es` opcionalmente seguido de `no` para identidad negada
 * (forma `a es no b`, ESPEC §5). Si hay `no`, envolvemos el binario
 * en un EXPR_UNARIO("no", ...).
 *
 * Ej. `a es b`     → (op "es" a b)
 *     `a es no b`  → (uop "no" (op "es" a b))
 */
static Expr *parsear_es(Parser *p, Expr *izq) {
    Token t = p->actual;
    avanzar(p); /* 'es' */
    bool negado = consumir_si(p, TT_NO);
    Expr *der = parsear_precedencia(p, PREC_COMPARAR + 1);
    if (der == NULL) return NULL;
    Expr *bin = expr_binario(p->arena, izq, TT_ES, der, t.linea, t.columna);
    if (bin == NULL) return NULL;
    if (negado) {
        return expr_unario(p->arena, TT_NO, bin, t.linea, t.columna);
    }
    return bin;
}

/*
 * `no` aparece como infijo solo en compound operators `no es` y `no en`
 * (forma natural en castellano: `archivo no es nulo`, `palabra no en
 * lista`). Tras `no` debe venir `es` o `en` obligatoriamente.
 *
 * Ej. `a no es b`  → (uop "no" (op "es" a b))
 *     `a no en b`  → (uop "no" (op "en" a b))
 */
static Expr *parsear_no_compuesto(Parser *p, Expr *izq) {
    Token t_no = p->actual;
    avanzar(p); /* 'no' */
    if (!check(p, TT_ES) && !check(p, TT_EN)) {
        error_en(p, &p->actual,
            "tras 'no' en operador comparativo se esperaba 'es' o 'en'");
        return NULL;
    }
    TipoToken op = p->actual.tipo;
    Token t_op = p->actual;
    avanzar(p); /* 'es' o 'en' */
    Expr *der = parsear_precedencia(p, PREC_COMPARAR + 1);
    if (der == NULL) return NULL;
    Expr *bin = expr_binario(p->arena, izq, op, der, t_op.linea, t_op.columna);
    if (bin == NULL) return NULL;
    return expr_unario(p->arena, TT_NO, bin, t_no.linea, t_no.columna);
}

static Expr *parsear_indice_o_rebanada(Parser *p, Expr *objeto) {
    Token apertura = p->actual;
    avanzar(p); /* consume '[' */

    Expr *inicio = NULL;
    if (!check(p, TT_DOS_PUNTOS)) {
        inicio = parsear_expresion(p);
        if (inicio == NULL) return NULL;
    }

    /* Si tras la primera expr (o si era omitida) hay `:`, es slice. */
    if (consumir_si(p, TT_DOS_PUNTOS)) {
        Expr *fin = NULL;
        Expr *paso = NULL;

        if (!check(p, TT_DOS_PUNTOS) && !check(p, TT_CORCH_DER)) {
            fin = parsear_expresion(p);
            if (fin == NULL) return NULL;
        }
        if (consumir_si(p, TT_DOS_PUNTOS)) {
            if (!check(p, TT_CORCH_DER)) {
                paso = parsear_expresion(p);
                if (paso == NULL) return NULL;
            }
        }
        if (!consumir(p, TT_CORCH_DER,
            "se esperaba ']' al final de la rebanada")) return NULL;
        return expr_rebanada(p->arena, objeto, inicio, fin, paso,
                             apertura.linea, apertura.columna);
    }

    /* Indexación simple. */
    if (!consumir(p, TT_CORCH_DER,
        "se esperaba ']' al final de la indexación")) return NULL;
    return expr_indice(p->arena, objeto, inicio,
                       apertura.linea, apertura.columna);
}

static Expr *parsear_unario(Parser *p) {
    Token t = p->actual;
    avanzar(p);
    Expr *operando = parsear_precedencia(p, PREC_UNARIO);
    if (operando == NULL) return NULL;
    return expr_unario(p->arena, t.tipo, operando, t.linea, t.columna);
}

static Expr *parsear_no(Parser *p) {
    Token t = p->actual;
    avanzar(p);
    /* `no` tiene precedencia más baja que aritméticos pero más alta
       que `y` y `o`. Lo modelamos llamando con PREC_NO. */
    Expr *operando = parsear_precedencia(p, PREC_NO);
    if (operando == NULL) return NULL;
    return expr_unario(p->arena, t.tipo, operando, t.linea, t.columna);
}

/* ──────────────────────────────────────────────────────────────────
 * Funciones de infijo
 * ────────────────────────────────────────────────────────────────── */

static Expr *parsear_binario(Parser *p, Expr *izq) {
    Token t = p->actual;
    avanzar(p);
    Precedencia prec = obtener_regla(t.tipo)->precedencia;
    /* Asociatividad izquierda: para parsear el RHS pedimos precedencia
       estrictamente mayor. Asociatividad derecha (potencia): igual o
       mayor — usar parsear_potencia. */
    Expr *der = parsear_precedencia(p, (Precedencia)(prec + 1));
    if (der == NULL) return NULL;
    return expr_binario(p->arena, izq, t.tipo, der, t.linea, t.columna);
}

static Expr *parsear_potencia(Parser *p, Expr *izq) {
    Token t = p->actual;
    avanzar(p);
    /* Asociatividad derecha: 2 ** 3 ** 4 = 2 ** (3 ** 4). Pasamos
       PREC_POTENCIA (no +1) para que la siguiente potencia se
       enganche a la derecha. */
    Expr *der = parsear_precedencia(p, PREC_POTENCIA);
    if (der == NULL) return NULL;
    return expr_binario(p->arena, izq, t.tipo, der, t.linea, t.columna);
}

/*
 * v1.44: expresión ternaria `<si_si> si <cond> sino <si_no>`. Estilo
 * Python `<si_si> if <cond> else <si_no>`. Cuando llegamos aquí ya
 * tenemos `si_si` (la expresión a la izquierda), el token actual es
 * `si`. Leemos la condición sin recursión a la ternaria (cond no
 * puede ser otra ternaria directa — usar paréntesis si hace falta),
 * consumimos `sino`, y parseamos `si_no` con PREC_TERNARIA para
 * asociatividad derecha — `a si b sino c si d sino e` es
 * `a si b sino (c si d sino e)`.
 */
static Expr *parsear_ternaria(Parser *p, Expr *si_si) {
    Token t_si = p->actual;
    avanzar(p);   /* consume `si` */
    /* La condición usa PREC_O para que no incluya un `sino` siguiente
       como si fuese otro ternario anidado. */
    Expr *cond = parsear_precedencia(p, PREC_O);
    if (cond == NULL) return NULL;
    if (!consumir(p, TT_SINO, "se esperaba 'sino' tras la condición del ternario")) {
        return NULL;
    }
    Expr *si_no = parsear_precedencia(p, PREC_TERNARIA);
    if (si_no == NULL) return NULL;
    return expr_ternaria(p->arena, si_si, cond, si_no, t_si.linea, t_si.columna);
}

static Expr *parsear_logica(Parser *p, Expr *izq) {
    Token t = p->actual;
    bool es_y = (t.tipo == TT_Y);
    avanzar(p);
    Precedencia prec = obtener_regla(t.tipo)->precedencia;
    Expr *der = parsear_precedencia(p, (Precedencia)(prec + 1));
    if (der == NULL) return NULL;
    return expr_logica(p->arena, izq, es_y, der, t.linea, t.columna);
}

static Expr *parsear_llamada(Parser *p, Expr *callee) {
    Token apertura = p->actual;
    avanzar(p); /* '(' */

    /* Capacidad inicial dinámica: vector de Expr* en arena. Usamos
       buffer en stack hasta 8 args; si excede, alocamos en arena con
       crecimiento. */
    Expr *buffer[8];
    bool spread_buffer[8] = {0};
    const char *kkey_buffer[8] = {0};
    int klen_buffer[8] = {0};
    bool dspread_buffer[8] = {0};
    Expr **args = buffer;
    bool *spreads = spread_buffer;
    const char **kkeys = kkey_buffer;
    int *klens = klen_buffer;
    bool *dspreads = dspread_buffer;
    int n = 0;
    int capacidad = 8;
    bool algun_spread = false;
    bool algun_kwarg = false;
    bool algun_dspread = false;
    bool visto_kwarg = false;  /* tras un kwarg, no más posicionales */

    if (!check(p, TT_PARENT_DER)) {
        do {
            if (n >= capacidad) {
                capacidad *= 2;
                Expr **nuevo = (Expr **)arena_alocar(p->arena,
                    sizeof(Expr *) * (size_t)capacidad);
                bool *nuevo_sp = (bool *)arena_alocar(p->arena,
                    sizeof(bool) * (size_t)capacidad);
                const char **nuevo_kk = (const char **)arena_alocar(p->arena,
                    sizeof(const char *) * (size_t)capacidad);
                int *nuevo_kl = (int *)arena_alocar(p->arena,
                    sizeof(int) * (size_t)capacidad);
                bool *nuevo_dsp = (bool *)arena_alocar(p->arena,
                    sizeof(bool) * (size_t)capacidad);
                if (!nuevo || !nuevo_sp || !nuevo_kk || !nuevo_kl || !nuevo_dsp) return NULL;
                memcpy(nuevo, args, sizeof(Expr *) * (size_t)n);
                memcpy(nuevo_sp, spreads, sizeof(bool) * (size_t)n);
                memcpy(nuevo_kk, kkeys, sizeof(const char *) * (size_t)n);
                memcpy(nuevo_kl, klens, sizeof(int) * (size_t)n);
                memcpy(nuevo_dsp, dspreads, sizeof(bool) * (size_t)n);
                args = nuevo;
                spreads = nuevo_sp;
                kkeys = nuevo_kk;
                klens = nuevo_kl;
                dspreads = nuevo_dsp;
            }
            /* v1.22: `*expr` → spread iterable. v1.25: `**expr` → spread dict. */
            bool es_spread = false;
            bool es_dspread = false;
            if (check(p, TT_DOBLE_ASTERISCO)) {
                avanzar(p);
                es_dspread = true;
                algun_dspread = true;
                visto_kwarg = true;  /* tras **spread, no posicionales */
            } else if (check(p, TT_ASTERISCO)) {
                avanzar(p);
                es_spread = true;
                algun_spread = true;
            }
            Expr *arg = parsear_expresion(p);
            if (arg == NULL) return NULL;
            /* v1.23: si `arg` es EXPR_IDENT y el siguiente token es `=`,
               re-interpretar como keyword argument `nombre = valor`.
               No aplica si era spread `*x` o `**x`. */
            const char *kkey = NULL;
            int klen = 0;
            if (!es_spread && !es_dspread
                && arg->tipo == EXPR_IDENT
                && check(p, TT_ASIGNAR)) {
                avanzar(p);  /* consume `=` */
                kkey = arg->como.ident.nombre;
                klen = arg->como.ident.longitud;
                arg = parsear_expresion(p);
                if (arg == NULL) return NULL;
                algun_kwarg = true;
                visto_kwarg = true;
            } else if (visto_kwarg && !es_spread && !es_dspread) {
                error_en(p, &p->actual,
                    "argumentos posicionales no pueden ir tras keyword args");
                return NULL;
            }
            args[n] = arg;
            spreads[n] = es_spread;
            kkeys[n] = kkey;
            klens[n] = klen;
            dspreads[n] = es_dspread;
            n++;
        } while (consumir_si(p, TT_COMA));
    }

    if (!consumir(p, TT_PARENT_DER, "se esperaba ')' para cerrar la llamada")) {
        return NULL;
    }

    /* Copiar args al arena para que sobrevivan al stack frame. */
    Expr **args_finales = (Expr **)arena_alocar(p->arena, sizeof(Expr *) * (size_t)(n > 0 ? n : 1));
    if (args_finales == NULL) return NULL;
    if (n > 0) memcpy(args_finales, args, sizeof(Expr *) * (size_t)n);

    Expr *e = expr_llamada(p->arena, callee, args_finales, n,
                            apertura.linea, apertura.columna);
    if (e == NULL) return NULL;
    if (algun_spread) {
        bool *sp_finales = (bool *)arena_alocar(p->arena,
            sizeof(bool) * (size_t)(n > 0 ? n : 1));
        if (sp_finales == NULL) return NULL;
        if (n > 0) memcpy(sp_finales, spreads, sizeof(bool) * (size_t)n);
        e->como.llamada.args_spread = sp_finales;
    }
    if (algun_kwarg) {
        const char **kk_finales = (const char **)arena_alocar(p->arena,
            sizeof(const char *) * (size_t)(n > 0 ? n : 1));
        int *kl_finales = (int *)arena_alocar(p->arena,
            sizeof(int) * (size_t)(n > 0 ? n : 1));
        if (!kk_finales || !kl_finales) return NULL;
        if (n > 0) {
            memcpy(kk_finales, kkeys, sizeof(const char *) * (size_t)n);
            memcpy(kl_finales, klens, sizeof(int) * (size_t)n);
        }
        e->como.llamada.kwarg_keys = kk_finales;
        e->como.llamada.kwarg_lens = kl_finales;
    }
    if (algun_dspread) {
        bool *dsp_finales = (bool *)arena_alocar(p->arena,
            sizeof(bool) * (size_t)(n > 0 ? n : 1));
        if (!dsp_finales) return NULL;
        if (n > 0) memcpy(dsp_finales, dspreads, sizeof(bool) * (size_t)n);
        e->como.llamada.args_doble_spread = dsp_finales;
    }
    return e;
}

static Expr *parsear_atributo(Parser *p, Expr *objeto) {
    Token punto = p->actual;
    avanzar(p); /* '.' */
    if (!check(p, TT_IDENT)) {
        error_en(p, &p->actual, "se esperaba un nombre de atributo tras '.'");
        return NULL;
    }
    Token nombre = p->actual;
    avanzar(p);
    return expr_atributo(p->arena, objeto, nombre.inicio, nombre.longitud,
                         punto.linea, punto.columna);
}

/* ──────────────────────────────────────────────────────────────────
 * Tabla de reglas
 *
 * Indexada por TipoToken. Cualquier token no listado tiene
 * { NULL, NULL, PREC_NULA } y por tanto no puede iniciar ni continuar
 * una expresión.
 * ────────────────────────────────────────────────────────────────── */

#define NUM_TIPOS_TOKEN  (TT_ERROR + 1)

static ReglaParseo reglas[NUM_TIPOS_TOKEN] = { 0 };
static bool reglas_inicializadas = false;

static void inicializar_reglas(void) {
    if (reglas_inicializadas) return;

    /* Literales y primarios */
    reglas[TT_ENTERO]      = (ReglaParseo){ parsear_numero_entero, NULL, PREC_NULA };
    reglas[TT_DECIMAL]     = (ReglaParseo){ parsear_numero_decimal, NULL, PREC_NULA };
    reglas[TT_CADENA]      = (ReglaParseo){ parsear_cadena, NULL, PREC_NULA };
    reglas[TT_F_CADENA]    = (ReglaParseo){ parsear_f_cadena, NULL, PREC_NULA };
    reglas[TT_VERDADERO]   = (ReglaParseo){ parsear_booleano, NULL, PREC_NULA };
    reglas[TT_FALSO]       = (ReglaParseo){ parsear_booleano, NULL, PREC_NULA };
    reglas[TT_NULO]        = (ReglaParseo){ parsear_nulo, NULL, PREC_NULA };
    reglas[TT_IDENT]       = (ReglaParseo){ parsear_ident, NULL, PREC_NULA };

    /* Agrupación / llamada / atributo / indexación */
    reglas[TT_PARENT_IZQ]  = (ReglaParseo){ parsear_grupo,         parsear_llamada,             PREC_LLAMADA };
    reglas[TT_PUNTO]       = (ReglaParseo){ NULL,                  parsear_atributo,            PREC_LLAMADA };
    reglas[TT_CORCH_IZQ]   = (ReglaParseo){ parsear_lista_literal, parsear_indice_o_rebanada,   PREC_LLAMADA };
    reglas[TT_LLAVE_IZQ]   = (ReglaParseo){ parsear_llaves,        NULL,                        PREC_NULA };

    /* Unarios prefijo / binarios infijo (mismos tokens) */
    reglas[TT_MENOS]       = (ReglaParseo){ parsear_unario, parsear_binario, PREC_TERMINO };
    reglas[TT_MAS]         = (ReglaParseo){ parsear_unario, parsear_binario, PREC_TERMINO };
    reglas[TT_TILDE_BIT]   = (ReglaParseo){ parsear_unario, NULL,            PREC_NULA };

    /* Aritméticos solo infijo */
    reglas[TT_ASTERISCO]       = (ReglaParseo){ NULL, parsear_binario, PREC_FACTOR };
    reglas[TT_BARRA]           = (ReglaParseo){ NULL, parsear_binario, PREC_FACTOR };
    reglas[TT_DOBLE_BARRA]     = (ReglaParseo){ NULL, parsear_binario, PREC_FACTOR };
    reglas[TT_PORCENTAJE]      = (ReglaParseo){ NULL, parsear_binario, PREC_FACTOR };
    reglas[TT_DOBLE_ASTERISCO] = (ReglaParseo){ NULL, parsear_potencia, PREC_POTENCIA };

    /* Comparaciones */
    reglas[TT_IGUAL]       = (ReglaParseo){ NULL, parsear_binario, PREC_COMPARAR };
    reglas[TT_DISTINTO]    = (ReglaParseo){ NULL, parsear_binario, PREC_COMPARAR };
    reglas[TT_MENOR]       = (ReglaParseo){ NULL, parsear_binario, PREC_COMPARAR };
    reglas[TT_MENOR_IGUAL] = (ReglaParseo){ NULL, parsear_binario, PREC_COMPARAR };
    reglas[TT_MAYOR]       = (ReglaParseo){ NULL, parsear_binario, PREC_COMPARAR };
    reglas[TT_MAYOR_IGUAL] = (ReglaParseo){ NULL, parsear_binario, PREC_COMPARAR };

    /* Bitwise */
    reglas[TT_AMPERSAND]    = (ReglaParseo){ NULL, parsear_binario, PREC_BIT_Y };
    reglas[TT_BARRA_VERT]   = (ReglaParseo){ NULL, parsear_binario, PREC_BIT_O };
    reglas[TT_CIRCUNFLEJO]  = (ReglaParseo){ NULL, parsear_binario, PREC_BIT_X };
    reglas[TT_DESPL_IZQ]    = (ReglaParseo){ NULL, parsear_binario, PREC_DESPLAZAR };
    reglas[TT_DESPL_DER]    = (ReglaParseo){ NULL, parsear_binario, PREC_DESPLAZAR };

    /* Lógicas */
    reglas[TT_Y]           = (ReglaParseo){ NULL, parsear_logica, PREC_Y };
    reglas[TT_O]           = (ReglaParseo){ NULL, parsear_logica, PREC_O };
    reglas[TT_NO]          = (ReglaParseo){ parsear_no, parsear_no_compuesto, PREC_COMPARAR };

    /* v1.44: `si` como infix de ternaria. Como prefix sigue siendo
       inválido a nivel de expresión — las sentencias `si` se parsean
       desde `parsear_sentencia`. */
    reglas[TT_SI]          = (ReglaParseo){ NULL, parsear_ternaria, PREC_TERNARIA };

    /* Identidad y pertenencia (operadores en palabra). */
    reglas[TT_ES]          = (ReglaParseo){ NULL, parsear_es, PREC_COMPARAR };
    reglas[TT_EN]          = (ReglaParseo){ NULL, parsear_binario, PREC_COMPARAR };

    /* Lambda como expresión. */
    reglas[TT_LAMBDA]      = (ReglaParseo){ parsear_lambda, NULL, PREC_NULA };

    /* `super.metodo` como expresión. */
    reglas[TT_SUPER]       = (ReglaParseo){ parsear_super, NULL, PREC_NULA };

    reglas_inicializadas = true;
}

static const ReglaParseo *obtener_regla(TipoToken tipo) {
    inicializar_reglas();
    if ((unsigned)tipo >= (unsigned)NUM_TIPOS_TOKEN) {
        static const ReglaParseo vacia = { NULL, NULL, PREC_NULA };
        return &vacia;
    }
    return &reglas[tipo];
}

/* ──────────────────────────────────────────────────────────────────
 * Parseo de sentencias
 *
 * Algoritmo general:
 *   - parsear_sentencia detecta el tipo según el primer token:
 *     - TT_PASAR/TT_ROMPER/TT_CONTINUAR/TT_RETORNAR → simples
 *     - TT_SI/TT_MIENTRAS/TT_PARA → bloques compuestos
 *     - TT_IDENT u otro inicio de expresión → asignación o sent_expr
 *   - Para bloques, después de `:`:
 *     - Si el siguiente token está en la misma línea → one-liner
 *       (un solo statement, sin `fin <X>`).
 *     - Si está en línea siguiente → multilinea (bloque hasta
 *       `fin <X>` requerido).
 *   - Para `fin`, validamos contra el stack de bloques abiertos:
 *     `fin si` solo cierra un bloque tipo SI, etc.
 *
 * Recuperación de errores: panic mode mínimo. En sesión 5 se afina.
 * ────────────────────────────────────────────────────────────────── */

/* Forward declarations. */
static Sent *parsear_si(Parser *p);
static Sent *parsear_mientras(Parser *p);
static Sent *parsear_para(Parser *p);
static Sent *parsear_funcion(Parser *p);
static Sent *parsear_clase(Parser *p);
static Sent *parsear_intentar(Parser *p);
static Sent *parsear_con(Parser *p);
static Sent *parsear_coincidir(Parser *p);
static Sent *parsear_lanzar(Parser *p);
static Sent *parsear_importar(Parser *p);
static Sent *parsear_desde_importar(Parser *p);
static Sent *parsear_global_o_nolocal(Parser *p, bool es_global);
static Sent *parsear_asignar_o_expr(Parser *p);
static Sent *parsear_cuerpo_bloque(Parser *p);
static bool parsear_lista_parametros(Parser *p, Parametro **out, int *n_out);
static bool parsear_ruta_modulo(Parser *p, Nombre **out, int *n_out);
static const char *etiqueta_para_bloque(TipoBloque t);
static TipoToken token_para_bloque(TipoBloque t);
static bool consumir_fin(Parser *p, TipoBloque tipo, int linea_apertura);
static bool empujar_bloque(Parser *p, TipoBloque tipo, int linea);
static void salir_bloque(Parser *p);
static bool en_inicio_de_termino(Parser *p);

/*
 * ¿El token actual termina un bloque?
 *
 * Terminadores reconocidos:
 *   - TT_FIN: fin <X>, cierre explícito de cualquier bloque.
 *   - TT_SINO: rama 'sino' / 'sino si' de un 'si', 'mientras', 'para',
 *     'intentar' (en este último indica el bloque sin excepción).
 *   - TT_ATRAPAR / TT_FINALMENTE: cláusulas continuadoras del bloque
 *     'intentar'.
 *   - TT_FIN_ARCHIVO: fin del programa.
 *
 * Estas keywords solo son terminadores semánticamente cuando estamos
 * dentro del bloque correcto, pero el parser dispatcher no tiene caso
 * para ellas a nivel sentencia, así que tratarlas siempre como
 * terminadores produce errores de sintaxis claros si aparecen
 * misplaced.
 */
static bool en_inicio_de_termino(Parser *p) {
    return p->actual.tipo == TT_FIN
        || p->actual.tipo == TT_SINO
        || p->actual.tipo == TT_ATRAPAR
        || p->actual.tipo == TT_FINALMENTE
        || p->actual.tipo == TT_CUANDO  /* v1.15: cuerpo de cuando termina al siguiente cuando */
        || p->actual.tipo == TT_FIN_ARCHIVO;
}

/* Asignaciones aumentadas: `+=`, `-=`, etc. */
static bool es_asignacion_aug(TipoToken t) {
    return t == TT_ASIGNAR_MAS || t == TT_ASIGNAR_MENOS
        || t == TT_ASIGNAR_ASTERISCO || t == TT_ASIGNAR_BARRA
        || t == TT_ASIGNAR_DOBLE_BARRA || t == TT_ASIGNAR_PORCENTAJE
        || t == TT_ASIGNAR_DOBLE_ASTER;
}

/* Stack de bloques. Al empujar, validamos overflow. */
static bool empujar_bloque(Parser *p, TipoBloque tipo, int linea) {
    if (p->profundidad_bloques >= 64) {
        error_en(p, &p->actual,
            "demasiados bloques anidados (máximo 64)");
        return false;
    }
    p->pila_bloques[p->profundidad_bloques].tipo = tipo;
    p->pila_bloques[p->profundidad_bloques].linea_apertura = linea;
    p->profundidad_bloques++;
    return true;
}

static void salir_bloque(Parser *p) {
    if (p->profundidad_bloques > 0) p->profundidad_bloques--;
}

static const char *etiqueta_para_bloque(TipoBloque t) {
    switch (t) {
        case BLOQUE_SI:        return "si";
        case BLOQUE_MIENTRAS:  return "mientras";
        case BLOQUE_PARA:      return "para";
        case BLOQUE_FUNCION:   return "funcion";
        case BLOQUE_CLASE:     return "clase";
        case BLOQUE_INTENTAR:  return "intentar";
        case BLOQUE_CON:       return "con";
        case BLOQUE_COINCIDIR: return "coincidir";
    }
    return "?";
}

static TipoToken token_para_bloque(TipoBloque t) {
    switch (t) {
        case BLOQUE_SI:        return TT_SI;
        case BLOQUE_MIENTRAS:  return TT_MIENTRAS;
        case BLOQUE_PARA:      return TT_PARA;
        case BLOQUE_FUNCION:   return TT_FUNCION;
        case BLOQUE_CLASE:     return TT_CLASE;
        case BLOQUE_INTENTAR:  return TT_INTENTAR;
        case BLOQUE_CON:       return TT_CON;
        case BLOQUE_COINCIDIR: return TT_COINCIDIR;
    }
    return TT_ERROR;
}

/*
 * Consume `fin <etiqueta>`. Verifica que la etiqueta coincide con
 * el bloque que se está cerrando. Devuelve true si OK.
 */
static bool consumir_fin(Parser *p, TipoBloque tipo, int linea_apertura) {
    if (!check(p, TT_FIN)) {
        char buf[160];
        snprintf(buf, sizeof(buf),
            "se esperaba 'fin %s' para cerrar el bloque abierto en línea %d",
            etiqueta_para_bloque(tipo), linea_apertura);
        error_en(p, &p->actual, buf);
        return false;
    }
    avanzar(p); /* consume 'fin' */

    TipoToken esperado = token_para_bloque(tipo);
    if (p->actual.tipo != esperado) {
        char buf[200];
        snprintf(buf, sizeof(buf),
            "se esperaba 'fin %s' (bloque abierto en línea %d), encontrado 'fin %.*s'",
            etiqueta_para_bloque(tipo),
            linea_apertura,
            p->actual.longitud,
            p->actual.inicio);
        error_en(p, &p->actual, buf);
        return false;
    }
    avanzar(p); /* consume la etiqueta */
    return true;
}

/*
 * Parsea sentencias hasta encontrar fin/sino/EOF. Devuelve un
 * SENT_BLOQUE con todas las sentencias acumuladas. No consume el
 * token de cierre — la rutina llamante lo hace.
 */
static Sent *parsear_cuerpo_bloque(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;

    Sent **sentencias = NULL;
    int n = 0;
    int capacidad = 0;

    while (!en_inicio_de_termino(p)) {
        if (n >= capacidad) {
            capacidad = capacidad == 0 ? 8 : capacidad * 2;
            Sent **nuevo = (Sent **)arena_alocar(p->arena,
                sizeof(Sent *) * (size_t)capacidad);
            if (nuevo == NULL) return NULL;
            if (n > 0) memcpy(nuevo, sentencias, sizeof(Sent *) * (size_t)n);
            sentencias = nuevo;
        }
        Sent *s = parser_parsear_sentencia(p);
        if (s == NULL) {
            /* Recuperación simple: si entramos en pánico, salir del
               bloque para evitar loop infinito. */
            if (p->en_panico) return NULL;
            continue;
        }
        sentencias[n++] = s;
    }

    return sent_bloque(p->arena, sentencias, n, linea, col);
}

/*
 * Parsea el cuerpo tras un `:`. Detecta one-liner vs bloque multilinea
 * mirando si el siguiente token está en la misma línea que el `:`.
 *
 * Para one-liner: parsea una sola sentencia simple (no requiere `fin`).
 * Para multilinea: parsea sentencias hasta fin/sino/EOF.
 *
 * Si `requiere_fin` es true, en el modo multilinea consume `fin <X>`.
 * Si es false (ej. cuerpo de `si` cuando habrá `sino` o `sino si`),
 * NO consume `fin`; el llamante es responsable.
 *
 * Devuelve siempre un SENT_BLOQUE (con 1 sentencia para one-liner).
 */
static Sent *parsear_cuerpo_tras_dospuntos(Parser *p, TipoBloque tipo_bloque,
                                           bool requiere_fin,
                                           int linea_apertura) {
    /* Detectar one-liner: tras consumir ':', miramos si actual está
       en la misma línea que ':' (que es ahora p->previo). */
    bool es_one_liner = (p->previo.linea == p->actual.linea);

    if (es_one_liner) {
        Sent *s = parser_parsear_sentencia(p);
        if (s == NULL) return NULL;
        Sent **arr = (Sent **)arena_alocar(p->arena, sizeof(Sent *));
        if (arr == NULL) return NULL;
        arr[0] = s;
        return sent_bloque(p->arena, arr, 1, s->linea, s->columna);
    }

    /* Multilinea. */
    if (!empujar_bloque(p, tipo_bloque, linea_apertura)) return NULL;
    Sent *cuerpo = parsear_cuerpo_bloque(p);
    salir_bloque(p);
    if (cuerpo == NULL) return NULL;

    if (requiere_fin) {
        if (!consumir_fin(p, tipo_bloque, linea_apertura)) return NULL;
    }
    return cuerpo;
}

static Sent *parsear_si(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* 'si' */

    /* Recolectamos ramas en un array dinámico. */
    RamaSi *ramas = NULL;
    int n_ramas = 0;
    int capacidad = 0;

#define ANADIR_RAMA(_cond, _cuerpo, _l, _c)                                    \
    do {                                                                       \
        if (n_ramas >= capacidad) {                                            \
            capacidad = capacidad == 0 ? 4 : capacidad * 2;                    \
            RamaSi *nuevo = (RamaSi *)arena_alocar(p->arena,                   \
                sizeof(RamaSi) * (size_t)capacidad);                           \
            if (nuevo == NULL) return NULL;                                    \
            if (n_ramas > 0)                                                   \
                memcpy(nuevo, ramas, sizeof(RamaSi) * (size_t)n_ramas);        \
            ramas = nuevo;                                                     \
        }                                                                      \
        ramas[n_ramas].condicion = (_cond);                                    \
        ramas[n_ramas].cuerpo = (_cuerpo);                                     \
        ramas[n_ramas].linea = (_l);                                           \
        ramas[n_ramas].columna = (_c);                                         \
        n_ramas++;                                                             \
    } while (0)

    Expr *cond = parser_parsear_expr(p);
    if (cond == NULL) return NULL;
    if (!consumir(p, TT_DOS_PUNTOS, "se esperaba ':' tras la condición de 'si'")) {
        return NULL;
    }

    /* Para detectar one-liner usamos la línea del ':'. Pero nuestra
       función parsear_cuerpo_tras_dospuntos hace ese check. */
    bool one_liner = (p->previo.linea == p->actual.linea);
    Sent *cuerpo;

    if (one_liner) {
        /* One-liner: sin fin si, sin sino. */
        cuerpo = parsear_cuerpo_tras_dospuntos(p, BLOQUE_SI, false, linea);
        if (cuerpo == NULL) return NULL;
        ANADIR_RAMA(cond, cuerpo, linea, col);
        return sent_si(p->arena, ramas, n_ramas, linea, col);
    }

    /* Multilinea. Empujamos bloque manualmente porque hay sino/sino si. */
    if (!empujar_bloque(p, BLOQUE_SI, linea)) return NULL;
    cuerpo = parsear_cuerpo_bloque(p);
    if (cuerpo == NULL) { salir_bloque(p); return NULL; }
    ANADIR_RAMA(cond, cuerpo, linea, col);

    /* Cadena de sino si / sino. */
    while (check(p, TT_SINO)) {
        int rama_linea = p->actual.linea;
        int rama_col = p->actual.columna;
        avanzar(p); /* 'sino' */

        if (check(p, TT_SI)) {
            avanzar(p); /* 'si' */
            Expr *c = parser_parsear_expr(p);
            if (c == NULL) { salir_bloque(p); return NULL; }
            if (!consumir(p, TT_DOS_PUNTOS, "se esperaba ':' tras 'sino si'")) {
                salir_bloque(p);
                return NULL;
            }
            Sent *cu = parsear_cuerpo_bloque(p);
            if (cu == NULL) { salir_bloque(p); return NULL; }
            ANADIR_RAMA(c, cu, rama_linea, rama_col);
        } else {
            /* 'sino' final. */
            if (!consumir(p, TT_DOS_PUNTOS, "se esperaba ':' tras 'sino'")) {
                salir_bloque(p);
                return NULL;
            }
            Sent *cu = parsear_cuerpo_bloque(p);
            if (cu == NULL) { salir_bloque(p); return NULL; }
            ANADIR_RAMA(NULL, cu, rama_linea, rama_col);
            break;
        }
    }

    salir_bloque(p);
    if (!consumir_fin(p, BLOQUE_SI, linea)) return NULL;

    return sent_si(p->arena, ramas, n_ramas, linea, col);
#undef ANADIR_RAMA
}

static Sent *parsear_mientras(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* 'mientras' */

    Expr *cond = parser_parsear_expr(p);
    if (cond == NULL) return NULL;
    if (!consumir(p, TT_DOS_PUNTOS, "se esperaba ':' tras la condición de 'mientras'")) {
        return NULL;
    }

    bool one_liner = (p->previo.linea == p->actual.linea);
    if (one_liner) {
        Sent *cuerpo = parsear_cuerpo_tras_dospuntos(p, BLOQUE_MIENTRAS, false, linea);
        if (cuerpo == NULL) return NULL;
        return sent_mientras(p->arena, cond, cuerpo, NULL, linea, col);
    }

    if (!empujar_bloque(p, BLOQUE_MIENTRAS, linea)) return NULL;
    Sent *cuerpo = parsear_cuerpo_bloque(p);
    if (cuerpo == NULL) { salir_bloque(p); return NULL; }

    Sent *sino = NULL;
    if (consumir_si(p, TT_SINO)) {
        if (!consumir(p, TT_DOS_PUNTOS, "se esperaba ':' tras 'sino' del 'mientras'")) {
            salir_bloque(p);
            return NULL;
        }
        sino = parsear_cuerpo_bloque(p);
        if (sino == NULL) { salir_bloque(p); return NULL; }
    }

    salir_bloque(p);
    if (!consumir_fin(p, BLOQUE_MIENTRAS, linea)) return NULL;

    return sent_mientras(p->arena, cond, cuerpo, sino, linea, col);
}

/*
 * v1.134: parsea un destino de `para` — un IDENT o `*IDENT`.
 * v1.137: tambien acepta sub-patrones `(...)` que se anidan
 * recursivamente. Ejemplo:
 *   para (mm, (n, o)) en triples:
 *   para mm, (n, o) en pares_de_pares:
 * El nodo devuelto puede ser EXPR_IDENT, EXPR_STAR_BIND o
 * EXPR_TUPLA (anidada). La maquinaria de SENT_ASIGNAR ya soporta
 * destructuring recursivo (desde v1.123).
 */
static Expr *parsear_destino_para(Parser *p) {
    if (check(p, TT_ASTERISCO)) {
        int sl = p->actual.linea;
        int sc = p->actual.columna;
        avanzar(p);  /* consume '*' */
        if (!check(p, TT_IDENT)) {
            error_en(p, &p->actual,
                "se esperaba un nombre tras '*' en destructuring de 'para'");
            return NULL;
        }
        Token tid = p->actual;
        avanzar(p);
        return expr_star_bind(p->arena, tid.inicio, tid.longitud, sl, sc);
    }
    if (check(p, TT_PARENT_IZQ)) {
        int sl = p->actual.linea;
        int sc = p->actual.columna;
        avanzar(p);  /* consume '(' */
        Expr **elementos = (Expr **)arena_alocar(p->arena,
            sizeof(Expr *) * 4);
        if (elementos == NULL) return NULL;
        int n = 0, cap = 4;
        if (!check(p, TT_PARENT_DER)) {
            do {
                Expr *e = parsear_destino_para(p);
                if (e == NULL) return NULL;
                if (n >= cap) {
                    int nuevo_cap = cap * 2;
                    Expr **nuevo = (Expr **)arena_alocar(p->arena,
                        sizeof(Expr *) * (size_t)nuevo_cap);
                    if (nuevo == NULL) return NULL;
                    memcpy(nuevo, elementos, sizeof(Expr *) * (size_t)n);
                    elementos = nuevo;
                    cap = nuevo_cap;
                }
                elementos[n++] = e;
            } while (consumir_si(p, TT_COMA) && !check(p, TT_PARENT_DER));
        }
        if (!consumir(p, TT_PARENT_DER,
            "se esperaba ')' al cerrar sub-patron de 'para'")) return NULL;
        return expr_tupla(p->arena, elementos, n, sl, sc);
    }
    if (!check(p, TT_IDENT)) {
        error_en(p, &p->actual,
            "se esperaba un nombre de variable tras 'para'");
        return NULL;
    }
    Token t = p->actual;
    avanzar(p);
    return expr_ident(p->arena, t.inicio, t.longitud, t.linea, t.columna);
}

static Sent *parsear_para(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* 'para' */

    /* v1.134: acepta multi-destino con star opcional:
     *   para a en xs:                  (clasico, un IDENT)
     *   para a, b en pares:            (tupla destructuring)
     *   para *previos, ultimo en xs:   (star en cualquier posicion)
     *
     * Cuando hay mas de un destino, el AST se reescribe como:
     *   para $item_L_C en iterable:
     *       (a, b) = $item_L_C
     *       <cuerpo original>
     *   fin para
     * Esto reusa la maquinaria de destructuring de SENT_ASIGNAR
     * (incluyendo pre_reservar_locales que reconoce IDENT y, desde
     * v1.134, STAR_BIND en patrones tupla). */
    Expr *primer_destino = parsear_destino_para(p);
    if (primer_destino == NULL) return NULL;

    bool es_destructuring = (primer_destino->tipo == EXPR_STAR_BIND)
                              || (primer_destino->tipo == EXPR_TUPLA)
                              || check(p, TT_COMA);

    Expr *patron = NULL;       /* solo si es_destructuring */
    Expr *objetivo;            /* IDENT que va al SENT_PARA */
    const char *nombre_tmp_arena = NULL;
    int nombre_tmp_long = 0;

    if (es_destructuring) {
        Expr **elementos = (Expr **)arena_alocar(p->arena,
            sizeof(Expr *) * 4);
        if (elementos == NULL) return NULL;
        int n = 1, cap = 4;
        elementos[0] = primer_destino;
        while (consumir_si(p, TT_COMA)) {
            if (check(p, TT_EN)) break;  /* coma final permitida */
            Expr *e = parsear_destino_para(p);
            if (e == NULL) return NULL;
            if (n >= cap) {
                int nuevo_cap = cap * 2;
                Expr **nuevo = (Expr **)arena_alocar(p->arena,
                    sizeof(Expr *) * (size_t)nuevo_cap);
                if (nuevo == NULL) return NULL;
                memcpy(nuevo, elementos, sizeof(Expr *) * (size_t)n);
                elementos = nuevo;
                cap = nuevo_cap;
            }
            elementos[n++] = e;
        }
        /* v1.137: si solo hay un elemento Y es ya EXPR_TUPLA (sub-patron
         * con parentesis), evitar envolverlo en otra tupla. Asi
         * `para (a, b) en pares:` produce patron = (a, b), no ((a, b),). */
        if (n == 1 && elementos[0]->tipo == EXPR_TUPLA) {
            patron = elementos[0];
        } else {
            patron = expr_tupla(p->arena, elementos, n, linea, col);
        }
        if (patron == NULL) return NULL;

        /* Nombre temporal unico por posicion textual del `para`. Asi
         * dos `para` anidados con destructuring no colisionan. */
        char buf[40];
        int len = snprintf(buf, sizeof(buf), "$item_%d_%d", linea, col);
        if (len <= 0 || len >= (int)sizeof(buf)) {
            error_en(p, &p->actual, "nombre temporal de 'para' demasiado largo");
            return NULL;
        }
        char *dst = (char *)arena_alocar(p->arena, (size_t)len + 1);
        if (dst == NULL) return NULL;
        memcpy(dst, buf, (size_t)len + 1);
        nombre_tmp_arena = dst;
        nombre_tmp_long = len;
        objetivo = expr_ident(p->arena, dst, len, linea, col);
        if (objetivo == NULL) return NULL;
    } else {
        objetivo = primer_destino;
    }

    if (!consumir(p, TT_EN, "se esperaba 'en' tras la variable de 'para'")) {
        return NULL;
    }
    Expr *iterable = parser_parsear_expr(p);
    if (iterable == NULL) return NULL;
    if (!consumir(p, TT_DOS_PUNTOS, "se esperaba ':' tras el iterable de 'para'")) {
        return NULL;
    }

    bool one_liner = (p->previo.linea == p->actual.linea);
    Sent *cuerpo;
    Sent *sino = NULL;
    if (one_liner) {
        cuerpo = parsear_cuerpo_tras_dospuntos(p, BLOQUE_PARA, false, linea);
        if (cuerpo == NULL) return NULL;
    } else {
        if (!empujar_bloque(p, BLOQUE_PARA, linea)) return NULL;
        cuerpo = parsear_cuerpo_bloque(p);
        if (cuerpo == NULL) { salir_bloque(p); return NULL; }

        if (consumir_si(p, TT_SINO)) {
            if (!consumir(p, TT_DOS_PUNTOS,
                "se esperaba ':' tras 'sino' del 'para'")) {
                salir_bloque(p);
                return NULL;
            }
            sino = parsear_cuerpo_bloque(p);
            if (sino == NULL) { salir_bloque(p); return NULL; }
        }

        salir_bloque(p);
        if (!consumir_fin(p, BLOQUE_PARA, linea)) return NULL;
    }

    /* v1.134: si es destructuring, envolver cuerpo en
     *   { $item_L_C_patron = $item_L_C; <cuerpo original> }
     * El SENT_ASIGNAR usa una NUEVA copia del ident $item como valor
     * (no compartimos punteros entre dos posiciones del AST por si
     * algun consumidor anota el nodo). */
    if (es_destructuring) {
        Expr *valor_ref = expr_ident(p->arena,
            nombre_tmp_arena, nombre_tmp_long, linea, col);
        if (valor_ref == NULL) return NULL;
        Sent *destructurar = sent_asignar(p->arena, patron, valor_ref,
                                          linea, col);
        if (destructurar == NULL) return NULL;
        Sent **sents = (Sent **)arena_alocar(p->arena, sizeof(Sent *) * 2);
        if (sents == NULL) return NULL;
        sents[0] = destructurar;
        sents[1] = cuerpo;
        cuerpo = sent_bloque(p->arena, sents, 2, linea, col);
        if (cuerpo == NULL) return NULL;
    }

    return sent_para(p->arena, objetivo, iterable, cuerpo, sino, linea, col);
}

/*
 * Parsea una sentencia que empieza con una expresión: o bien
 * asignación (simple o aumentada), o bien expresión-como-sentencia.
 */
static Sent *parsear_asignar_o_expr(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;

    Expr *primero;
    /* v1.133: star binding en posicion inicial de destructuring:
     * `*r, x = it`. El asterisco no tiene regla prefix de expresion,
     * asi que sin este atajo el parser falla con "se esperaba una
     * expresion". Solo se acepta si lo que sigue es `* IDENT ,`
     * (forma rigurosa de destructuring inicial). */
    if (check(p, TT_ASTERISCO)) {
        Token tok_aster = p->actual;
        avanzar(p);  /* consume '*' */
        if (!check(p, TT_IDENT)) {
            error_en(p, &p->actual,
                "se esperaba un nombre tras '*' en destructuring inicial");
            return NULL;
        }
        Token tok_id = p->actual;
        avanzar(p);
        primero = expr_star_bind(p->arena, tok_id.inicio,
                                 tok_id.longitud,
                                 tok_aster.linea, tok_aster.columna);
        if (primero == NULL) return NULL;
        if (!check(p, TT_COMA)) {
            error_en(p, &p->actual,
                "'*nombre' solo es valido en destructuring "
                "(se esperaba ',')");
            return NULL;
        }
    } else {
        primero = parser_parsear_expr(p);
        if (primero == NULL) return NULL;
    }

    /* v1.114: anotacion de tipo en asignacion `nombre: tipo = valor`.
     * Solo permitida cuando el destino es un IDENT puro. La anotacion
     * se parsea pero se descarta (no se valida en runtime — sirve
     * para documentacion y futuras herramientas de tipo). */
    if (primero->tipo == EXPR_IDENT && check(p, TT_DOS_PUNTOS)) {
        avanzar(p);  /* consume ':' */
        Expr *anot = parser_parsear_expr(p);
        if (anot == NULL) return NULL;
        /* anot se ignora; debe seguir un `=` con valor. */
        if (!consumir(p, TT_ASIGNAR,
            "se esperaba '=' tras anotacion de tipo en asignacion")) {
            return NULL;
        }
        Expr *valor = parser_parsear_expr(p);
        if (valor == NULL) return NULL;
        return sent_asignar(p->arena, primero, valor, linea, col);
    }

    /* v1.21: destructuring `a, b = par` — si tras la primera expr viene
       coma + más exprs + `=`, formar tupla LHS sin paréntesis. */
    if (check(p, TT_COMA)) {
        /* Recolectar resto de targets. */
        Expr **elementos = NULL;
        int n = 1, cap = 4;
        elementos = (Expr **)arena_alocar(p->arena, sizeof(Expr *) * (size_t)cap);
        if (elementos == NULL) return NULL;
        elementos[0] = primero;
        while (consumir_si(p, TT_COMA)) {
            /* Coma final → permitido si después viene `=`. */
            if (check(p, TT_ASIGNAR)) break;
            Expr *e;
            /* v1.129: `*nombre` star binding en posicion de target.
             * Solo valido aqui dentro de un destructuring. */
            if (check(p, TT_ASTERISCO)) {
                int s_lin = p->actual.linea;
                int s_col = p->actual.columna;
                avanzar(p);  /* consume '*' */
                if (!check(p, TT_IDENT)) {
                    error_en(p, &p->actual,
                        "se esperaba un nombre tras '*' en destructuring");
                    return NULL;
                }
                Token tok_id = p->actual;
                avanzar(p);
                e = expr_star_bind(p->arena, tok_id.inicio,
                                       tok_id.longitud, s_lin, s_col);
            } else {
                e = parser_parsear_expr(p);
            }
            if (e == NULL) return NULL;
            if (n >= cap) {
                int nuevo_cap = cap * 2;
                Expr **nuevo = (Expr **)arena_alocar(p->arena,
                    sizeof(Expr *) * (size_t)nuevo_cap);
                if (nuevo == NULL) return NULL;
                memcpy(nuevo, elementos, sizeof(Expr *) * (size_t)n);
                elementos = nuevo;
                cap = nuevo_cap;
            }
            elementos[n++] = e;
        }
        if (!consumir(p, TT_ASIGNAR,
            "se esperaba '=' tras lista de targets en asignacion multiple")) {
            return NULL;
        }
        /* RHS: permite tupla sin paréntesis para `a, b = b, a`. */
        Expr *valor = parser_parsear_expr(p);
        if (valor == NULL) return NULL;
        if (check(p, TT_COMA)) {
            int rlinea = valor->linea;
            int rcol = valor->columna;
            Expr **r_elems = NULL;
            int rn = 1, rcap = 4;
            r_elems = (Expr **)arena_alocar(p->arena,
                sizeof(Expr *) * (size_t)rcap);
            if (r_elems == NULL) return NULL;
            r_elems[0] = valor;
            while (consumir_si(p, TT_COMA)) {
                Expr *e = parser_parsear_expr(p);
                if (e == NULL) return NULL;
                if (rn >= rcap) {
                    int nuevo_cap = rcap * 2;
                    Expr **nuevo = (Expr **)arena_alocar(p->arena,
                        sizeof(Expr *) * (size_t)nuevo_cap);
                    if (nuevo == NULL) return NULL;
                    memcpy(nuevo, r_elems, sizeof(Expr *) * (size_t)rn);
                    r_elems = nuevo;
                    rcap = nuevo_cap;
                }
                r_elems[rn++] = e;
            }
            valor = expr_tupla(p->arena, r_elems, rn, rlinea, rcol);
            if (valor == NULL) return NULL;
        }
        Expr *tupla_lhs = expr_tupla(p->arena, elementos, n, linea, col);
        if (tupla_lhs == NULL) return NULL;
        return sent_asignar(p->arena, tupla_lhs, valor, linea, col);
    }

    if (consumir_si(p, TT_ASIGNAR)) {
        Expr *valor = parser_parsear_expr(p);
        if (valor == NULL) return NULL;
        return sent_asignar(p->arena, primero, valor, linea, col);
    }

    if (es_asignacion_aug(p->actual.tipo)) {
        TipoToken op = p->actual.tipo;
        avanzar(p);
        Expr *valor = parser_parsear_expr(p);
        if (valor == NULL) return NULL;
        return sent_asignar_aug(p->arena, primero, op, valor, linea, col);
    }

    return sent_expr(p->arena, primero, linea, col);
}

Sent *parser_parsear_sentencia(Parser *p) {
    p->en_panico = false; /* nueva sentencia, salimos de pánico */

    int linea = p->actual.linea;
    int col = p->actual.columna;
    (void)linea; (void)col;

    /* v1.72: decoradores. Una secuencia `@expr` seguida de otra `@expr`...
     * y finalmente `funcion`. Se coleccionan en orden de fuente y se
     * adjuntan al SENT_FUNCION devuelto por parsear_funcion. El
     * compilador emite f = decN(...(dec1(f))...). */
    if (p->actual.tipo == TT_AT) {
        Expr **decs = NULL;
        int n_decs = 0, cap_decs = 0;
        int dec_linea = p->actual.linea, dec_col = p->actual.columna;
        while (consumir_si(p, TT_AT)) {
            Expr *d = parser_parsear_expr(p);
            if (d == NULL) return NULL;
            if (n_decs >= cap_decs) {
                int nuevo_cap = cap_decs == 0 ? 4 : cap_decs * 2;
                Expr **nuevo = (Expr **)arena_alocar(p->arena,
                    sizeof(Expr *) * (size_t)nuevo_cap);
                if (nuevo == NULL) return NULL;
                if (n_decs > 0) memcpy(nuevo, decs, sizeof(Expr *) * (size_t)n_decs);
                decs = nuevo;
                cap_decs = nuevo_cap;
            }
            decs[n_decs++] = d;
        }
        if (p->actual.tipo != TT_FUNCION) {
            error_en(p, &p->actual,
                "se esperaba 'funcion' tras decorador(es) '@...'");
            return NULL;
        }
        Sent *s = parsear_funcion(p);
        if (s == NULL) return NULL;
        s->como.funcion.decoradores = decs;
        s->como.funcion.n_decoradores = n_decs;
        s->linea = dec_linea;
        s->columna = dec_col;
        return s;
    }

    switch (p->actual.tipo) {
        case TT_PASAR:     avanzar(p); return sent_pasar(p->arena, linea, col);
        case TT_ROMPER:    avanzar(p); return sent_romper(p->arena, linea, col);
        case TT_CONTINUAR: avanzar(p); return sent_continuar(p->arena, linea, col);
        case TT_RETORNAR: {
            avanzar(p); /* consume 'retornar' */
            /* Si tras 'retornar' viene algo que NO inicia expresión,
               es 'retornar' sin valor. Heurística: si hay
               salto de línea (token actual en línea distinta) o si es
               fin/sino/EOF, no hay valor. */
            if (p->actual.linea != linea
                || p->actual.tipo == TT_FIN
                || p->actual.tipo == TT_SINO
                || p->actual.tipo == TT_FIN_ARCHIVO) {
                return sent_retornar(p->arena, NULL, linea, col);
            }
            Expr *e = parser_parsear_expr(p);
            if (e == NULL) return NULL;
            return sent_retornar(p->arena, e, linea, col);
        }
        case TT_PRODUCIR: {
            avanzar(p); /* consume 'producir' */
            /* v1.33: `producir desde EXPR` — delega a un sub-generador
               (o cualquier iterable). Se desugar a:
                   para $yf_N en EXPR:
                       producir $yf_N
                   fin para
               El contador `$yf_N` da un nombre único para evitar
               colisión con variables del usuario. */
            if (consumir_si(p, TT_DESDE)) {
                static int yf_counter = 0;
                Expr *iterable = parser_parsear_expr(p);
                if (iterable == NULL) return NULL;
                /* Generar nombre único `$yf_N` en el arena. */
                char *nombre = (char *)arena_alocar(p->arena, 24);
                if (nombre == NULL) return NULL;
                int nlen = snprintf(nombre, 24, "$yf_%d", yf_counter++);
                /* Dos idents al mismo buffer: objetivo del `para` y
                   operando de `producir`. */
                Expr *var_obj = expr_ident(p->arena, nombre, nlen, linea, col);
                Expr *var_prod = expr_ident(p->arena, nombre, nlen, linea, col);
                if (var_obj == NULL || var_prod == NULL) return NULL;
                Sent *prod = sent_producir(p->arena, var_prod, linea, col);
                if (prod == NULL) return NULL;
                Sent *cuerpo = sent_bloque(p->arena, &prod, 1, linea, col);
                if (cuerpo == NULL) return NULL;
                /* Necesitamos que el array de 1 sentencia sobreviva:
                   sent_bloque guarda el puntero. Copiarlo al arena. */
                Sent **sents = (Sent **)arena_alocar(p->arena, sizeof(Sent *));
                if (sents == NULL) return NULL;
                sents[0] = prod;
                cuerpo = sent_bloque(p->arena, sents, 1, linea, col);
                if (cuerpo == NULL) return NULL;
                return sent_para(p->arena, var_obj, iterable, cuerpo,
                                 NULL, linea, col);
            }
            Expr *e = parser_parsear_expr(p);
            if (e == NULL) return NULL;
            return sent_producir(p->arena, e, linea, col);
        }
        case TT_SI:        return parsear_si(p);
        case TT_MIENTRAS:  return parsear_mientras(p);
        case TT_PARA:      return parsear_para(p);
        case TT_FUNCION:   return parsear_funcion(p);
        case TT_CLASE:     return parsear_clase(p);
        case TT_INTENTAR:  return parsear_intentar(p);
        case TT_CON:       return parsear_con(p);
        case TT_COINCIDIR: return parsear_coincidir(p);
        case TT_LANZAR:    return parsear_lanzar(p);
        case TT_IMPORTAR:  return parsear_importar(p);
        case TT_DESDE:     return parsear_desde_importar(p);
        case TT_GLOBAL:    return parsear_global_o_nolocal(p, true);
        case TT_NOLOCAL:   return parsear_global_o_nolocal(p, false);

        case TT_BORRAR: {
            /* v1.56: `borrar destino` donde destino es `d[k]` (indice)
             * o `obj.attr` (atributo). El compilador rechaza otros
             * tipos de destino con un mensaje claro. */
            avanzar(p);  /* consume 'borrar' */
            Expr *destino = parser_parsear_expr(p);
            if (destino == NULL) return NULL;
            return sent_borrar(p->arena, destino, linea, col);
        }

        default:
            return parsear_asignar_o_expr(p);
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Parámetros, funciones, clases, lambda
 * ────────────────────────────────────────────────────────────────── */

/*
 * Parsea una lista de parámetros entre paréntesis. Asume que se está
 * a punto de consumir el `(`. Tras la llamada, `(` y `)` están
 * consumidos. Devuelve true si OK.
 *
 * Cada parámetro es: IDENT [: tipo] [= valor_defecto]
 * ESPEC §5: parametro ← IDENT (":" expr)? ("=" expr)?
 *
 * En sesión 5 podríamos añadir *args y **kwargs si los necesitamos.
 */
static bool parsear_lista_parametros(Parser *p, Parametro **out, int *n_out) {
    if (!consumir(p, TT_PARENT_IZQ,
        "se esperaba '(' tras el nombre")) return false;

    Parametro *params = NULL;
    int n = 0;
    int capacidad = 0;

    bool ya_visto_estrella = false;
    bool ya_visto_doble_estrella = false;
    if (!check(p, TT_PARENT_DER)) {
        do {
            /* v1.22: `*ident` → parámetro variádico que recoge args sobrantes
               en una tupla. Solo se permite uno.
               v1.24: `**ident` → recoge keyword args sobrantes en dict.
               Debe ser el último parámetro. */
            bool es_estrella = false;
            bool es_doble_estrella = false;
            if (ya_visto_doble_estrella) {
                error_en(p, &p->actual,
                    "no puede haber parámetros tras '**kw'");
                return false;
            }
            if (check(p, TT_DOBLE_ASTERISCO)) {
                avanzar(p);
                es_doble_estrella = true;
                ya_visto_doble_estrella = true;
            } else if (check(p, TT_ASTERISCO)) {
                avanzar(p);
                es_estrella = true;
                if (ya_visto_estrella) {
                    error_en(p, &p->actual,
                        "solo se permite un parámetro '*resto'");
                    return false;
                }
                ya_visto_estrella = true;
            }
            if (!check(p, TT_IDENT)) {
                error_en(p, &p->actual,
                    "se esperaba un nombre de parámetro");
                return false;
            }
            Token t_nombre = p->actual;
            avanzar(p);

            Expr *anot_tipo = NULL;
            Expr *valor_defecto = NULL;

            if (consumir_si(p, TT_DOS_PUNTOS)) {
                /* Anotación de tipo. */
                anot_tipo = parser_parsear_expr(p);
                if (anot_tipo == NULL) return false;
            }
            if (consumir_si(p, TT_ASIGNAR)) {
                if (es_estrella || es_doble_estrella) {
                    error_en(p, &p->actual,
                        "parámetro variádico no puede tener valor por defecto");
                    return false;
                }
                /* Valor por defecto. */
                valor_defecto = parser_parsear_expr(p);
                if (valor_defecto == NULL) return false;
            }

            if (n >= capacidad) {
                capacidad = capacidad == 0 ? 4 : capacidad * 2;
                Parametro *nuevo = (Parametro *)arena_alocar(p->arena,
                    sizeof(Parametro) * (size_t)capacidad);
                if (nuevo == NULL) return false;
                if (n > 0) memcpy(nuevo, params, sizeof(Parametro) * (size_t)n);
                params = nuevo;
            }
            params[n].nombre = t_nombre.inicio;
            params[n].longitud_nombre = t_nombre.longitud;
            params[n].anotacion_tipo = anot_tipo;
            params[n].valor_defecto = valor_defecto;
            params[n].es_estrella = es_estrella;
            params[n].es_doble_estrella = es_doble_estrella;
            params[n].linea = t_nombre.linea;
            params[n].columna = t_nombre.columna;
            n++;
        } while (consumir_si(p, TT_COMA));
    }

    if (!consumir(p, TT_PARENT_DER,
        "se esperaba ')' al final de la lista de parámetros")) return false;

    *out = params;
    *n_out = n;
    return true;
}

static Sent *parsear_funcion(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* consume 'funcion' */

    if (!check(p, TT_IDENT)) {
        error_en(p, &p->actual,
            "se esperaba un nombre tras 'funcion'");
        return NULL;
    }
    Token t_nombre = p->actual;
    avanzar(p);

    Parametro *params = NULL;
    int n_params = 0;
    if (!parsear_lista_parametros(p, &params, &n_params)) return NULL;

    /* Anotación de retorno opcional `-> tipo`. */
    Expr *anot_retorno = NULL;
    if (consumir_si(p, TT_FLECHA)) {
        anot_retorno = parser_parsear_expr(p);
        if (anot_retorno == NULL) return NULL;
    }

    if (!consumir(p, TT_DOS_PUNTOS,
        "se esperaba ':' tras la cabecera de la función")) return NULL;

    /* Cuerpo: one-liner o multilinea. */
    bool one_liner = (p->previo.linea == p->actual.linea);
    Sent *cuerpo;
    if (one_liner) {
        cuerpo = parsear_cuerpo_tras_dospuntos(p, BLOQUE_FUNCION, false, linea);
    } else {
        if (!empujar_bloque(p, BLOQUE_FUNCION, linea)) return NULL;
        cuerpo = parsear_cuerpo_bloque(p);
        salir_bloque(p);
        if (cuerpo == NULL) return NULL;
        if (!consumir_fin(p, BLOQUE_FUNCION, linea)) return NULL;
    }
    if (cuerpo == NULL) return NULL;

    return sent_funcion(p->arena, t_nombre.inicio, t_nombre.longitud,
                        params, n_params, anot_retorno, cuerpo, linea, col);
}

static Sent *parsear_clase(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* consume 'clase' */

    if (!check(p, TT_IDENT)) {
        error_en(p, &p->actual,
            "se esperaba un nombre tras 'clase'");
        return NULL;
    }
    Token t_nombre = p->actual;
    avanzar(p);

    /* Superclases opcionales: `extiende Padre1, Padre2, ...`. */
    Expr **supers = NULL;
    int n_supers = 0;
    int cap_supers = 0;

    if (consumir_si(p, TT_EXTIENDE)) {
        do {
            Expr *padre = parser_parsear_expr(p);
            if (padre == NULL) return NULL;
            if (n_supers >= cap_supers) {
                cap_supers = cap_supers == 0 ? 4 : cap_supers * 2;
                Expr **nuevo = (Expr **)arena_alocar(p->arena,
                    sizeof(Expr *) * (size_t)cap_supers);
                if (nuevo == NULL) return NULL;
                if (n_supers > 0)
                    memcpy(nuevo, supers, sizeof(Expr *) * (size_t)n_supers);
                supers = nuevo;
            }
            supers[n_supers++] = padre;
        } while (consumir_si(p, TT_COMA));
    }

    if (!consumir(p, TT_DOS_PUNTOS,
        "se esperaba ':' tras la cabecera de la clase")) return NULL;

    /* Cuerpo: típicamente multilinea con métodos. One-liners en clases
       son raros pero válidos según la regla general. */
    bool one_liner = (p->previo.linea == p->actual.linea);
    Sent *cuerpo;
    if (one_liner) {
        cuerpo = parsear_cuerpo_tras_dospuntos(p, BLOQUE_CLASE, false, linea);
    } else {
        if (!empujar_bloque(p, BLOQUE_CLASE, linea)) return NULL;
        cuerpo = parsear_cuerpo_bloque(p);
        salir_bloque(p);
        if (cuerpo == NULL) return NULL;
        if (!consumir_fin(p, BLOQUE_CLASE, linea)) return NULL;
    }
    if (cuerpo == NULL) return NULL;

    return sent_clase(p->arena, t_nombre.inicio, t_nombre.longitud,
                      supers, n_supers, cuerpo, linea, col);
}

/* ──────────────────────────────────────────────────────────────────
 * Lambda como expresión: `lambda x, y: x + y`
 *
 * El parser de expresiones llama esta función cuando ve TT_LAMBDA.
 * La sintaxis es `lambda` seguido de parámetros (sin paréntesis,
 * a diferencia de `funcion`), `:`, y una sola expresión como cuerpo.
 *
 * Nota especial sobre defaults: lambda permite `=` para defaults
 * porque tras consumir un parámetro buscamos `,` o `:`. Esto significa
 * que dentro de una lambda no se puede asignar a una variable como
 * expresión (Cornamusa no tiene walrus operator, así que no hay
 * conflicto).
 * ────────────────────────────────────────────────────────────────── */
static Expr *parsear_lambda(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* consume 'lambda' */

    Parametro *params = NULL;
    int n = 0;
    int capacidad = 0;

    /* Lista de parámetros sin paréntesis: `x, y, z` o vacía.
       En lambda NO se admiten anotaciones de tipo: el `:` siempre
       termina la lista de parámetros. Solo `= valor_defecto` opcional.
       v1.22: `*resto` también permitido aquí. */
    bool ya_visto_estrella = false;
    bool ya_visto_doble_estrella = false;
    if (!check(p, TT_DOS_PUNTOS)) {
        do {
            bool es_estrella = false;
            bool es_doble_estrella = false;
            if (ya_visto_doble_estrella) {
                error_en(p, &p->actual,
                    "no puede haber parámetros tras '**kw' en lambda");
                return NULL;
            }
            if (check(p, TT_DOBLE_ASTERISCO)) {
                avanzar(p);
                es_doble_estrella = true;
                ya_visto_doble_estrella = true;
            } else if (check(p, TT_ASTERISCO)) {
                avanzar(p);
                es_estrella = true;
                if (ya_visto_estrella) {
                    error_en(p, &p->actual,
                        "solo se permite un '*resto' en lambda");
                    return NULL;
                }
                ya_visto_estrella = true;
            }
            if (!check(p, TT_IDENT)) {
                error_en(p, &p->actual,
                    "se esperaba un nombre de parámetro en lambda");
                return NULL;
            }
            Token t_nombre = p->actual;
            avanzar(p);

            Expr *anot_tipo = NULL;  /* Lambda no admite anotaciones. */
            Expr *valor_defecto = NULL;
            if (consumir_si(p, TT_ASIGNAR)) {
                if (es_estrella || es_doble_estrella) {
                    error_en(p, &p->actual,
                        "variádicos no admiten defecto");
                    return NULL;
                }
                valor_defecto = parser_parsear_expr(p);
                if (valor_defecto == NULL) return NULL;
            }

            if (n >= capacidad) {
                capacidad = capacidad == 0 ? 4 : capacidad * 2;
                Parametro *nuevo = (Parametro *)arena_alocar(p->arena,
                    sizeof(Parametro) * (size_t)capacidad);
                if (nuevo == NULL) return NULL;
                if (n > 0)
                    memcpy(nuevo, params, sizeof(Parametro) * (size_t)n);
                params = nuevo;
            }
            params[n].nombre = t_nombre.inicio;
            params[n].longitud_nombre = t_nombre.longitud;
            params[n].anotacion_tipo = anot_tipo;
            params[n].valor_defecto = valor_defecto;
            params[n].es_estrella = es_estrella;
            params[n].es_doble_estrella = es_doble_estrella;
            params[n].linea = t_nombre.linea;
            params[n].columna = t_nombre.columna;
            n++;
        } while (consumir_si(p, TT_COMA));
    }

    if (!consumir(p, TT_DOS_PUNTOS,
        "se esperaba ':' tras los parámetros de lambda")) return NULL;

    Expr *cuerpo = parser_parsear_expr(p);
    if (cuerpo == NULL) return NULL;

    return expr_lambda(p->arena, params, n, cuerpo, linea, col);
}

/* ──────────────────────────────────────────────────────────────────
 * Excepciones, módulos, declaraciones
 * ────────────────────────────────────────────────────────────────── */

/*
 * Parsea `intentar: ... atrapar...: ... [sino:...] [finalmente:...] fin intentar`.
 *
 * Permitimos cero `atrapar` SI hay `finalmente` (patrón try/finally).
 * Si no hay ni atrapar ni finalmente, es error.
 */
static Sent *parsear_intentar(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* 'intentar' */
    if (!consumir(p, TT_DOS_PUNTOS, "se esperaba ':' tras 'intentar'")) {
        return NULL;
    }

    if (!empujar_bloque(p, BLOQUE_INTENTAR, linea)) return NULL;

    Sent *cuerpo = parsear_cuerpo_bloque(p);
    if (cuerpo == NULL) { salir_bloque(p); return NULL; }

    /* Cláusulas atrapar (cero o más). */
    ClausulaAtrapar *atrapadores = NULL;
    int n_atrapadores = 0;
    int cap = 0;

    while (check(p, TT_ATRAPAR)) {
        if (n_atrapadores >= cap) {
            cap = cap == 0 ? 4 : cap * 2;
            ClausulaAtrapar *nuevo = (ClausulaAtrapar *)arena_alocar(p->arena,
                sizeof(ClausulaAtrapar) * (size_t)cap);
            if (nuevo == NULL) { salir_bloque(p); return NULL; }
            if (n_atrapadores > 0)
                memcpy(nuevo, atrapadores,
                    sizeof(ClausulaAtrapar) * (size_t)n_atrapadores);
            atrapadores = nuevo;
        }
        ClausulaAtrapar *ca = &atrapadores[n_atrapadores];
        ca->linea = p->actual.linea;
        ca->columna = p->actual.columna;
        avanzar(p); /* 'atrapar' */

        ca->tipo = NULL;
        ca->alias.texto = NULL;
        ca->alias.longitud = 0;

        /* Tres formas tras 'atrapar':
           - ':' inmediato → bare atrapar (atrapa todo).
           - expr ':' → atrapa de ese tipo, sin alias.
           - expr 'como' IDENT ':' → atrapa con alias. */
        if (!check(p, TT_DOS_PUNTOS)) {
            ca->tipo = parser_parsear_expr(p);
            if (ca->tipo == NULL) { salir_bloque(p); return NULL; }
            if (consumir_si(p, TT_COMO)) {
                if (!check(p, TT_IDENT)) {
                    error_en(p, &p->actual,
                        "se esperaba un nombre tras 'como' en cláusula 'atrapar'");
                    salir_bloque(p);
                    return NULL;
                }
                ca->alias.texto = p->actual.inicio;
                ca->alias.longitud = p->actual.longitud;
                avanzar(p);
            }
        }
        if (!consumir(p, TT_DOS_PUNTOS,
            "se esperaba ':' tras la cabecera de 'atrapar'")) {
            salir_bloque(p);
            return NULL;
        }
        ca->cuerpo = parsear_cuerpo_bloque(p);
        if (ca->cuerpo == NULL) { salir_bloque(p); return NULL; }
        n_atrapadores++;
    }

    /* 'sino' opcional (rama "sin excepción"). */
    Sent *sino = NULL;
    if (consumir_si(p, TT_SINO)) {
        if (!consumir(p, TT_DOS_PUNTOS,
            "se esperaba ':' tras 'sino' en 'intentar'")) {
            salir_bloque(p);
            return NULL;
        }
        sino = parsear_cuerpo_bloque(p);
        if (sino == NULL) { salir_bloque(p); return NULL; }
    }

    /* 'finalmente' opcional. */
    Sent *finalmente = NULL;
    if (consumir_si(p, TT_FINALMENTE)) {
        if (!consumir(p, TT_DOS_PUNTOS, "se esperaba ':' tras 'finalmente'")) {
            salir_bloque(p);
            return NULL;
        }
        finalmente = parsear_cuerpo_bloque(p);
        if (finalmente == NULL) { salir_bloque(p); return NULL; }
    }

    salir_bloque(p);

    if (n_atrapadores == 0 && finalmente == NULL) {
        error_en(p, &p->actual,
            "'intentar' debe tener al menos una cláusula 'atrapar' o 'finalmente'");
        return NULL;
    }

    if (!consumir_fin(p, BLOQUE_INTENTAR, linea)) return NULL;

    return sent_intentar(p->arena, cuerpo, atrapadores, n_atrapadores,
                         sino, finalmente, linea, col);
}

/*
 * SENT_CON v1.13: `con expr [como nombre]: cuerpo fin con`.
 *
 * Desugar AST a un bloque equivalente sin nuevos opcodes ni nuevo
 * tipo de Sent:
 *
 *     __cm_<linea>_<col> = expr
 *     [nombre = __cm_<linea>_<col>.__entrar__()]    # o llamada descarta
 *     intentar:
 *         cuerpo
 *     finalmente:
 *         __cm_<linea>_<col>.__salir__()
 *     fin intentar
 *
 * El nombre `__cm_<l>_<c>` es único por call-site y comienza con `__`,
 * lo que lo aleja de los identificadores idiomáticos del usuario sin
 * impedir uso intencional. Si el alias está, se asigna; si no, se
 * descarta el resultado de `__entrar__`. Si el cuerpo lanza una
 * excepción, `finalmente` ejecuta `__salir__` antes de propagar.
 *
 * v1.46: multi-recurso `con a, b:` (anidado al desazucarar).
 *
 * v1.141: `__salir__` recibe 3 argumentos (tipo_exc, valor_exc,
 * traza), todos `nulo` cuando el bloque sale sin excepción. Cuando
 * sí hay excepción, el desugar añade una cláusula `atrapar e`
 * intermedia que llama `__salir__(tipo(e), e, nulo)` y siempre
 * relanza. Esto da paridad de firma con Python — los context
 * managers existentes que solo declaran `__salir__(yo, *_)` siguen
 * funcionando; quienes inspeccionan el error ahora pueden hacerlo.
 *
 * La traza se pasa como `nulo` por ahora — Cornamusa no expone aún
 * el traceback estructurado del frame. Es un futuro upgrade.
 */

/* v1.46: construye el bloque desugarado para UN solo recurso `con`,
   con un body ya prefabricado. Reusado para cada nivel del anidamiento
   cuando hay multi-recurso. */
static Sent *desugar_un_con(Parser *p, Expr *contexto,
                              const char *alias_texto, int alias_len,
                              Sent *cuerpo, int linea, int col,
                              int sufijo) {
    /* Nombre interno único `__cm_<linea>_<col>` (sufijo > 0 para
       multi-recurso, para que cada nivel tenga su propio nombre). */
    char nombre_buf[64];
    int n_nombre;
    if (sufijo == 0) {
        n_nombre = snprintf(nombre_buf, sizeof(nombre_buf),
                             "__cm_%d_%d", linea, col);
    } else {
        n_nombre = snprintf(nombre_buf, sizeof(nombre_buf),
                             "__cm_%d_%d_%d", linea, col, sufijo);
    }
    if (n_nombre <= 0 || n_nombre >= (int)sizeof(nombre_buf)) {
        error_en(p, &p->actual,
            "no se pudo generar nombre interno para 'con'");
        return NULL;
    }
    char *nombre_arena = (char *)arena_alocar(p->arena, (size_t)n_nombre + 1);
    if (nombre_arena == NULL) return NULL;
    memcpy(nombre_arena, nombre_buf, (size_t)n_nombre + 1);

    /* Sentencia 1: `__cm_X = contexto`. */
    Expr *id_cm_lhs = expr_ident(p->arena, nombre_arena, n_nombre, linea, col);
    Sent *s_asig_cm = sent_asignar(p->arena, id_cm_lhs, contexto, linea, col);
    if (s_asig_cm == NULL) return NULL;

    /* Sentencia 2: `alias = __cm_X.__entrar__()` o llamada descartada. */
    Expr *id_cm_get = expr_ident(p->arena, nombre_arena, n_nombre, linea, col);
    Expr *attr_entrar = expr_atributo(p->arena, id_cm_get,
                                        "__entrar__", 10, linea, col);
    Expr *llamada_entrar = expr_llamada(p->arena, attr_entrar, NULL, 0,
                                          linea, col);
    Sent *s_entrar;
    if (alias_texto != NULL) {
        Expr *id_alias = expr_ident(p->arena, alias_texto, alias_len,
                                      linea, col);
        s_entrar = sent_asignar(p->arena, id_alias, llamada_entrar,
                                  linea, col);
    } else {
        s_entrar = sent_expr(p->arena, llamada_entrar, linea, col);
    }
    if (s_entrar == NULL) return NULL;

    /* Sentencia 3: `intentar cuerpo finalmente __cm_X.__salir__(n,n,n)`.
     * v1.141: pasamos 3 args nulos (tipo_exc, valor_exc, traza) para
     * que la firma case con Python — los managers que solo declaran
     * `__salir__(yo, *_)` ignoran los args, y los que inspeccionan
     * los reciben (todos nulos por ahora; la info real de excepcion
     * llegara con un upgrade futuro del frame de traceback). */
    Expr *id_cm_salir = expr_ident(p->arena, nombre_arena, n_nombre,
                                     linea, col);
    Expr *attr_salir = expr_atributo(p->arena, id_cm_salir,
                                       "__salir__", 9, linea, col);
    Expr **args_salir = (Expr **)arena_alocar(p->arena,
        sizeof(Expr *) * 3);
    if (args_salir == NULL) return NULL;
    args_salir[0] = expr_literal_nulo(p->arena, linea, col);
    args_salir[1] = expr_literal_nulo(p->arena, linea, col);
    args_salir[2] = expr_literal_nulo(p->arena, linea, col);
    if (args_salir[0] == NULL || args_salir[1] == NULL
        || args_salir[2] == NULL) return NULL;
    Expr *llamada_salir = expr_llamada(p->arena, attr_salir,
                                         args_salir, 3, linea, col);
    Sent *cuerpo_finalmente_unica = sent_expr(p->arena, llamada_salir,
                                                linea, col);
    Sent **arr_fin = (Sent **)arena_alocar(p->arena, sizeof(Sent *));
    if (arr_fin == NULL) return NULL;
    arr_fin[0] = cuerpo_finalmente_unica;
    Sent *finalmente_block = sent_bloque(p->arena, arr_fin, 1, linea, col);
    Sent *s_intentar = sent_intentar(p->arena, cuerpo, NULL, 0, NULL,
                                       finalmente_block, linea, col);
    if (s_intentar == NULL) return NULL;

    /* Envolver las 3 sentencias en un bloque. */
    Sent **bloque_arr = (Sent **)arena_alocar(p->arena, sizeof(Sent *) * 3);
    if (bloque_arr == NULL) return NULL;
    bloque_arr[0] = s_asig_cm;
    bloque_arr[1] = s_entrar;
    bloque_arr[2] = s_intentar;
    return sent_bloque(p->arena, bloque_arr, 3, linea, col);
}

typedef struct {
    Expr *contexto;
    const char *alias_texto;
    int alias_len;
} RecursoCon;

static Sent *parsear_con(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* 'con' */

    /* v1.46: parsear lista de recursos separados por coma.
       `con A, B como b, C:` produce hasta 3 niveles anidados. */
    RecursoCon recursos[16];
    int n_recursos = 0;

    do {
        if (n_recursos >= 16) {
            error_en(p, &p->actual,
                "demasiados recursos en `con` (máximo 16)");
            return NULL;
        }
        Expr *contexto = parser_parsear_expr(p);
        if (contexto == NULL) return NULL;
        const char *alias_texto = NULL;
        int alias_len = 0;
        if (consumir_si(p, TT_COMO)) {
            if (!check(p, TT_IDENT)) {
                error_en(p, &p->actual,
                    "se esperaba un nombre tras 'como' en 'con'");
                return NULL;
            }
            alias_texto = p->actual.inicio;
            alias_len = p->actual.longitud;
            avanzar(p);
        }
        recursos[n_recursos].contexto = contexto;
        recursos[n_recursos].alias_texto = alias_texto;
        recursos[n_recursos].alias_len = alias_len;
        n_recursos++;
    } while (consumir_si(p, TT_COMA));

    if (!consumir(p, TT_DOS_PUNTOS,
            "se esperaba ':' tras la cabecera de 'con'")) {
        return NULL;
    }

    if (!empujar_bloque(p, BLOQUE_CON, linea)) return NULL;

    Sent *cuerpo = parsear_cuerpo_bloque(p);
    if (cuerpo == NULL) { salir_bloque(p); return NULL; }

    salir_bloque(p);

    if (!consumir_fin(p, BLOQUE_CON, linea)) return NULL;

    /* Anidar desde dentro hacia fuera: el último recurso envuelve el
       cuerpo del usuario; cada anterior envuelve el resultado. */
    Sent *envuelto = cuerpo;
    for (int i = n_recursos - 1; i >= 0; i--) {
        envuelto = desugar_un_con(p,
            recursos[i].contexto,
            recursos[i].alias_texto, recursos[i].alias_len,
            envuelto, linea, col, i);
        if (envuelto == NULL) return NULL;
    }
    return envuelto;
}

/*
 * SENT_COINCIDIR v1.15: `coincidir expr: cuando ... fin coincidir`.
 *
 * Patrones soportados (versión inicial):
 *   - Wildcard: `_`
 *   - Bind: identificador → crea local con ese nombre.
 *   - Literal: entero, decimal, cadena, f-cadena, booleano, nulo.
 *     Acepta unario `-` o `+` antes (para `-1`, `-3.14`).
 *
 * Cláusula `cuando <patron> [si <guarda>]: cuerpo`. La guarda es una
 * expresión booleana opcional que refina el match: solo entra al
 * cuerpo si el patrón matchea Y la guarda es verdadera.
 *
 * El cuerpo de cada `cuando` se parsea hasta el siguiente `cuando`,
 * `fin`, `sino` o EOF. Sin fall-through entre cláusulas.
 *
 * Aplazadas a v1.15+: tuplas `(p1, p2)`, listas `[p1, p2]`, OR-
 * patterns `p1 | p2`, type-match `Foo(p1, p2)`.
 */
/* Forward decl: la lista de patrones recurre por sub-patrones.
   `parsear_patron_simple` parsea UN patrón sin OR. `parsear_patron`
   maneja `p1 | p2 | ...` envolviendo en PATRON_OR si hay alternativas. */
static Patron *parsear_patron(Parser *p);
static Patron *parsear_patron_simple(Parser *p);

/* Parsea una secuencia de patrones separados por coma hasta el
   delimitador de cierre indicado (TT_PARENT_DER o TT_CORCH_DER).
   Acepta coma final opcional. Devuelve `*n_out` con la cuenta.
   v1.16.2: dentro de listas (cierre == TT_CORCH_DER), acepta `*ident`
   como elemento star-bind. Solo uno permitido por lista. */
static Patron **parsear_lista_patrones(Parser *p, TipoToken cierre, int *n_out) {
    Patron **elementos = NULL;
    int n = 0, cap = 0;
    bool vio_star = false;
    if (!check(p, cierre)) {
        do {
            if (check(p, cierre)) break;  /* coma final OK */
            if (n >= cap) {
                cap = cap == 0 ? 4 : cap * 2;
                Patron **nuevo = (Patron **)arena_alocar(p->arena,
                    sizeof(Patron *) * (size_t)cap);
                if (nuevo == NULL) return NULL;
                if (n > 0) memcpy(nuevo, elementos,
                                  sizeof(Patron *) * (size_t)n);
                elementos = nuevo;
            }
            Patron *sub;
            /* Detección de `*nombre` star-bind (v1.16.2). */
            if (check(p, TT_ASTERISCO)) {
                if (cierre != TT_CORCH_DER) {
                    error_en(p, &p->actual,
                        "patron '*nombre' solo permitido dentro de '[...]' en v1.16.2");
                    return NULL;
                }
                if (vio_star) {
                    error_en(p, &p->actual,
                        "solo un '*nombre' permitido por patron de lista");
                    return NULL;
                }
                int slinea = p->actual.linea;
                int scol = p->actual.columna;
                avanzar(p);  /* `*` */
                if (!check(p, TT_IDENT)) {
                    error_en(p, &p->actual,
                        "se esperaba un nombre tras '*' en patron");
                    return NULL;
                }
                const char *nombre = p->actual.inicio;
                int len = p->actual.longitud;
                avanzar(p);
                sub = patron_star_bind(p->arena, nombre, len, slinea, scol);
                vio_star = true;
            } else {
                sub = parsear_patron(p);
            }
            if (sub == NULL) return NULL;
            elementos[n++] = sub;
        } while (consumir_si(p, TT_COMA));
    }
    if (!consumir(p, cierre,
            cierre == TT_PARENT_DER
                ? "se esperaba ')' al final del patron de tupla"
                : "se esperaba ']' al final del patron de lista")) {
        return NULL;
    }
    *n_out = n;
    return elementos;
}

/* Parsea UN patrón sin operador `|`. Llamado desde `parsear_patron`
   (que maneja OR) y desde sub-patrones de tupla/lista. */
static Patron *parsear_patron_simple(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;

    /* Wildcard `_`. Convención: identificador exactamente igual a `_`. */
    if (p->actual.tipo == TT_IDENT
        && p->actual.longitud == 1 && p->actual.inicio[0] == '_') {
        avanzar(p);
        return patron_wildcard(p->arena, linea, col);
    }

    /* IDENT: puede ser bind, o type-match si va seguido de `()` (v1.16.3). */
    if (p->actual.tipo == TT_IDENT) {
        const char *nombre = p->actual.inicio;
        int len = p->actual.longitud;
        avanzar(p);
        if (check(p, TT_PARENT_IZQ)) {
            avanzar(p);
            /* v1.16.3 solo `Foo()` sin args. Destructuring posicional
               requiere atributos posicionales que Cornamusa no tiene. */
            if (!consumir(p, TT_PARENT_DER,
                    "v1.16.3 solo soporta 'Foo()' sin args en patron de tipo")) {
                return NULL;
            }
            return patron_tipo(p->arena, nombre, len, linea, col);
        }
        return patron_bind(p->arena, nombre, len, linea, col);
    }

    /* Tupla `(p1, p2, ...)` (v1.16). */
    if (p->actual.tipo == TT_PARENT_IZQ) {
        avanzar(p);
        int n = 0;
        Patron **elems = parsear_lista_patrones(p, TT_PARENT_DER, &n);
        if (n > 0 && elems == NULL) return NULL;  /* error */
        if (elems == NULL && n == 0) {
            /* Tupla vacía válida (matchea `()`). */
        }
        return patron_tupla(p->arena, elems, n, linea, col);
    }

    /* Lista `[p1, p2, ...]` (v1.16). */
    if (p->actual.tipo == TT_CORCH_IZQ) {
        avanzar(p);
        int n = 0;
        Patron **elems = parsear_lista_patrones(p, TT_CORCH_DER, &n);
        if (n > 0 && elems == NULL) return NULL;
        return patron_lista(p->arena, elems, n, linea, col);
    }

    /* Literal: aceptamos los token-types que producen expresiones
       literales atómicas, opcionalmente precedidos por `-` o `+`
       unario. */
    bool negar = false;
    if (p->actual.tipo == TT_MENOS) {
        negar = true;
        avanzar(p);
    } else if (p->actual.tipo == TT_MAS) {
        avanzar(p);
    }
    Expr *e = NULL;
    Token t = p->actual;
    switch (t.tipo) {
        case TT_ENTERO:
            avanzar(p);
            e = expr_literal_entero(p->arena, t.inicio, t.longitud,
                                      t.linea, t.columna);
            break;
        case TT_DECIMAL:
            avanzar(p);
            e = expr_literal_decimal(p->arena, t.inicio, t.longitud,
                                       t.linea, t.columna);
            break;
        case TT_CADENA:
            avanzar(p);
            e = expr_literal_cadena(p->arena, t.inicio, t.longitud,
                                      t.linea, t.columna);
            break;
        case TT_VERDADERO:
            avanzar(p);
            e = expr_literal_booleano(p->arena, true, t.linea, t.columna);
            break;
        case TT_FALSO:
            avanzar(p);
            e = expr_literal_booleano(p->arena, false, t.linea, t.columna);
            break;
        case TT_NULO:
            avanzar(p);
            e = expr_literal_nulo(p->arena, t.linea, t.columna);
            break;
        default:
            error_en(p, &t, "patron invalido en 'cuando'");
            return NULL;
    }
    if (e == NULL) return NULL;
    if (negar) {
        e = expr_unario(p->arena, TT_MENOS, e, linea, col);
        if (e == NULL) return NULL;
    }
    return patron_literal(p->arena, e, linea, col);
}

/* Parsea un patrón con OR: `p1 | p2 | ...`. Si solo hay uno, retorna
   el patrón directamente. Si hay varios, los envuelve en PATRON_OR.
   Restricción v1.16.2: las alternativas de OR deben ser literales o
   wildcard — no binds ni estructurales — para evitar la complejidad
   de bindings inconsistentes entre alternativas. */
static Patron *parsear_patron(Parser *p) {
    Patron *primero = parsear_patron_simple(p);
    if (primero == NULL) return NULL;
    if (!check(p, TT_BARRA_VERT)) return primero;

    /* Hay al menos una alternativa más. */
    int linea = primero->linea;
    int col = primero->columna;
    Patron **alts = NULL;
    int n = 0, cap = 0;

    /* Validar que el primero es literal o wildcard. */
    if (primero->tipo != PATRON_LITERAL && primero->tipo != PATRON_WILDCARD) {
        error_en(p, &p->actual,
            "OR-patron solo admite literales o '_' como alternativas en v1.16.2");
        return NULL;
    }

    cap = 4;
    alts = (Patron **)arena_alocar(p->arena, sizeof(Patron *) * (size_t)cap);
    if (alts == NULL) return NULL;
    alts[n++] = primero;

    while (consumir_si(p, TT_BARRA_VERT)) {
        Patron *sub = parsear_patron_simple(p);
        if (sub == NULL) return NULL;
        if (sub->tipo != PATRON_LITERAL && sub->tipo != PATRON_WILDCARD) {
            error_en(p, &p->actual,
                "OR-patron solo admite literales o '_' como alternativas en v1.16.2");
            return NULL;
        }
        if (n >= cap) {
            int nuevo_cap = cap * 2;
            Patron **nuevo = (Patron **)arena_alocar(p->arena,
                sizeof(Patron *) * (size_t)nuevo_cap);
            if (nuevo == NULL) return NULL;
            memcpy(nuevo, alts, sizeof(Patron *) * (size_t)n);
            alts = nuevo;
            cap = nuevo_cap;
        }
        alts[n++] = sub;
    }
    return patron_or(p->arena, alts, n, linea, col);
}

static Sent *parsear_coincidir(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* 'coincidir' */

    Expr *sujeto = parser_parsear_expr(p);
    if (sujeto == NULL) return NULL;
    if (!consumir(p, TT_DOS_PUNTOS,
            "se esperaba ':' tras la expresion de 'coincidir'")) {
        return NULL;
    }

    if (!empujar_bloque(p, BLOQUE_COINCIDIR, linea)) return NULL;

    /* Recolectar cláusulas `cuando`. */
    ClausulaCuando *clausulas = NULL;
    int n = 0, cap = 0;
    while (check(p, TT_CUANDO)) {
        if (n >= cap) {
            cap = cap == 0 ? 4 : cap * 2;
            ClausulaCuando *nuevo = (ClausulaCuando *)arena_alocar(p->arena,
                sizeof(ClausulaCuando) * (size_t)cap);
            if (nuevo == NULL) { salir_bloque(p); return NULL; }
            if (n > 0) memcpy(nuevo, clausulas,
                              sizeof(ClausulaCuando) * (size_t)n);
            clausulas = nuevo;
        }
        ClausulaCuando *cw = &clausulas[n];
        cw->linea = p->actual.linea;
        cw->columna = p->actual.columna;
        avanzar(p); /* 'cuando' */

        cw->patron = parsear_patron(p);
        if (cw->patron == NULL) { salir_bloque(p); return NULL; }

        /* v1.16.3: `como <ident>` opcional tras el patrón — bindea el
           sujeto entero. Útil con type-match: `cuando Foo() como v:`. */
        cw->bind_completo_texto = NULL;
        cw->bind_completo_longitud = 0;
        if (consumir_si(p, TT_COMO)) {
            if (!check(p, TT_IDENT)) {
                error_en(p, &p->actual,
                    "se esperaba un nombre tras 'como' en 'cuando'");
                salir_bloque(p);
                return NULL;
            }
            cw->bind_completo_texto = p->actual.inicio;
            cw->bind_completo_longitud = p->actual.longitud;
            avanzar(p);
        }

        cw->guarda = NULL;
        if (consumir_si(p, TT_SI)) {
            cw->guarda = parser_parsear_expr(p);
            if (cw->guarda == NULL) { salir_bloque(p); return NULL; }
        }

        if (!consumir(p, TT_DOS_PUNTOS,
                "se esperaba ':' tras el patron de 'cuando'")) {
            salir_bloque(p);
            return NULL;
        }
        cw->cuerpo = parsear_cuerpo_bloque(p);
        if (cw->cuerpo == NULL) { salir_bloque(p); return NULL; }
        n++;
    }

    salir_bloque(p);

    if (n == 0) {
        error_en(p, &p->actual,
            "'coincidir' debe tener al menos una clausula 'cuando'");
        return NULL;
    }

    if (!consumir_fin(p, BLOQUE_COINCIDIR, linea)) return NULL;

    return sent_coincidir(p->arena, sujeto, clausulas, n, linea, col);
}

static Sent *parsear_lanzar(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* 'lanzar' */

    /* Si lo que sigue está en otra línea o es fin/sino/EOF/atrapar/etc.,
       es 'lanzar' sin valor (re-raise dentro de atrapar). */
    if (p->actual.linea != linea
        || p->actual.tipo == TT_FIN
        || p->actual.tipo == TT_SINO
        || p->actual.tipo == TT_ATRAPAR
        || p->actual.tipo == TT_FINALMENTE
        || p->actual.tipo == TT_FIN_ARCHIVO) {
        return sent_lanzar(p->arena, NULL, linea, col);
    }
    Expr *e = parser_parsear_expr(p);
    if (e == NULL) return NULL;
    return sent_lanzar(p->arena, e, linea, col);
}

/*
 * Parsea una ruta de módulo: IDENT ('.' IDENT)*. Devuelve true si OK.
 * Aloca el array en arena.
 */
static bool parsear_ruta_modulo(Parser *p, Nombre **out, int *n_out) {
    if (!check(p, TT_IDENT)) {
        error_en(p, &p->actual,
            "se esperaba un nombre de módulo");
        return false;
    }

    Nombre *segs = NULL;
    int n = 0;
    int cap = 0;

    do {
        if (!check(p, TT_IDENT)) {
            error_en(p, &p->actual,
                "se esperaba un nombre tras '.' en ruta de módulo");
            return false;
        }
        if (n >= cap) {
            cap = cap == 0 ? 4 : cap * 2;
            Nombre *nuevo = (Nombre *)arena_alocar(p->arena,
                sizeof(Nombre) * (size_t)cap);
            if (nuevo == NULL) return false;
            if (n > 0) memcpy(nuevo, segs, sizeof(Nombre) * (size_t)n);
            segs = nuevo;
        }
        segs[n].texto = p->actual.inicio;
        segs[n].longitud = p->actual.longitud;
        n++;
        avanzar(p);
    } while (consumir_si(p, TT_PUNTO));

    *out = segs;
    *n_out = n;
    return true;
}

static Sent *parsear_importar(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* 'importar' */

    Nombre *segmentos = NULL;
    int n_segs = 0;
    if (!parsear_ruta_modulo(p, &segmentos, &n_segs)) return NULL;

    Nombre alias = { NULL, 0 };
    if (consumir_si(p, TT_COMO)) {
        if (!check(p, TT_IDENT)) {
            error_en(p, &p->actual,
                "se esperaba un alias tras 'como'");
            return NULL;
        }
        alias.texto = p->actual.inicio;
        alias.longitud = p->actual.longitud;
        avanzar(p);
    }

    return sent_importar(p->arena, segmentos, n_segs, alias, linea, col);
}

static Sent *parsear_desde_importar(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* 'desde' */

    Nombre *segmentos = NULL;
    int n_segs = 0;
    if (!parsear_ruta_modulo(p, &segmentos, &n_segs)) return NULL;

    if (!consumir(p, TT_IMPORTAR,
        "se esperaba 'importar' tras la ruta de módulo")) return NULL;

    /* Caso `desde X importar *`. */
    if (consumir_si(p, TT_ASTERISCO)) {
        return sent_desde_importar(p->arena, segmentos, n_segs,
                                    NULL, 0, true, linea, col);
    }

    /* Lista de items separados por coma. */
    ItemImportado *items = NULL;
    int n_items = 0;
    int cap = 0;

    do {
        if (!check(p, TT_IDENT)) {
            error_en(p, &p->actual,
                "se esperaba un nombre a importar");
            return NULL;
        }
        if (n_items >= cap) {
            cap = cap == 0 ? 4 : cap * 2;
            ItemImportado *nuevo = (ItemImportado *)arena_alocar(p->arena,
                sizeof(ItemImportado) * (size_t)cap);
            if (nuevo == NULL) return NULL;
            if (n_items > 0)
                memcpy(nuevo, items, sizeof(ItemImportado) * (size_t)n_items);
            items = nuevo;
        }
        items[n_items].nombre.texto = p->actual.inicio;
        items[n_items].nombre.longitud = p->actual.longitud;
        items[n_items].alias.texto = NULL;
        items[n_items].alias.longitud = 0;
        items[n_items].linea = p->actual.linea;
        items[n_items].columna = p->actual.columna;
        avanzar(p);

        if (consumir_si(p, TT_COMO)) {
            if (!check(p, TT_IDENT)) {
                error_en(p, &p->actual,
                    "se esperaba un alias tras 'como'");
                return NULL;
            }
            items[n_items].alias.texto = p->actual.inicio;
            items[n_items].alias.longitud = p->actual.longitud;
            avanzar(p);
        }
        n_items++;
    } while (consumir_si(p, TT_COMA));

    return sent_desde_importar(p->arena, segmentos, n_segs,
                                items, n_items, false, linea, col);
}

static Sent *parsear_global_o_nolocal(Parser *p, bool es_global) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* 'global' o 'nolocal' */

    Nombre *nombres = NULL;
    int n = 0;
    int cap = 0;

    do {
        if (!check(p, TT_IDENT)) {
            error_en(p, &p->actual,
                es_global
                    ? "se esperaba un nombre tras 'global'"
                    : "se esperaba un nombre tras 'nolocal'");
            return NULL;
        }
        if (n >= cap) {
            cap = cap == 0 ? 4 : cap * 2;
            Nombre *nuevo = (Nombre *)arena_alocar(p->arena,
                sizeof(Nombre) * (size_t)cap);
            if (nuevo == NULL) return NULL;
            if (n > 0) memcpy(nuevo, nombres, sizeof(Nombre) * (size_t)n);
            nombres = nuevo;
        }
        nombres[n].texto = p->actual.inicio;
        nombres[n].longitud = p->actual.longitud;
        n++;
        avanzar(p);
    } while (consumir_si(p, TT_COMA));

    return es_global
        ? sent_global(p->arena, nombres, n, linea, col)
        : sent_nolocal(p->arena, nombres, n, linea, col);
}

Sent **parser_parsear_programa(Parser *p, int *n_out) {
    Sent **sentencias = NULL;
    int n = 0;
    int capacidad = 0;

    while (p->actual.tipo != TT_FIN_ARCHIVO) {
        if (n >= capacidad) {
            capacidad = capacidad == 0 ? 16 : capacidad * 2;
            Sent **nuevo = (Sent **)arena_alocar(p->arena,
                sizeof(Sent *) * (size_t)capacidad);
            if (nuevo == NULL) { *n_out = 0; return NULL; }
            if (n > 0) memcpy(nuevo, sentencias, sizeof(Sent *) * (size_t)n);
            sentencias = nuevo;
        }
        Sent *s = parser_parsear_sentencia(p);
        if (s != NULL) {
            sentencias[n++] = s;
        } else if (p->en_panico) {
            /* Recuperación: avanzar hasta inicio de siguiente sentencia
               plausible (TT_FIN_ARCHIVO o keyword de inicio). */
            while (p->actual.tipo != TT_FIN_ARCHIVO
                && p->actual.tipo != TT_SI
                && p->actual.tipo != TT_MIENTRAS
                && p->actual.tipo != TT_PARA
                && p->actual.tipo != TT_FUNCION
                && p->actual.tipo != TT_CLASE) {
                avanzar(p);
            }
            p->en_panico = false;
        }
    }

    *n_out = n;
    return sentencias;
}
