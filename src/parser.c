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
static Expr *parsear_grupo(Parser *p);
static Expr *parsear_unario(Parser *p);
static Expr *parsear_no(Parser *p);
static Expr *parsear_binario(Parser *p, Expr *izq);
static Expr *parsear_logica(Parser *p, Expr *izq);
static Expr *parsear_potencia(Parser *p, Expr *izq);
static Expr *parsear_llamada(Parser *p, Expr *callee);
static Expr *parsear_atributo(Parser *p, Expr *objeto);

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
    /* Pre-cargamos el primer token; previo se queda en cero hasta el
       primer avanzar real, lo que está bien porque nada lo lee antes. */
    p->previo = (Token){0};
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
    /* Si el token ofensor era válido, asumimos longitud >= 1 ya. */
    error_imprimir_token(&err, p->fuente, p->archivo, stderr);
}

/* ──────────────────────────────────────────────────────────────────
 * Parseo de expresiones (Pratt)
 * ────────────────────────────────────────────────────────────────── */

Expr *parser_parsear_expr(Parser *p) {
    return parsear_expresion(p);
}

static Expr *parsear_expresion(Parser *p) {
    return parsear_precedencia(p, PREC_O);
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

static Expr *parsear_f_cadena(Parser *p) {
    Token t = p->actual;
    avanzar(p);
    /* En sesión 5 parsearemos los {expr} internos. Por ahora el lexema
       completo se almacena tal cual. */
    return expr_literal_f_cadena(p->arena, t.inicio, t.longitud, t.linea, t.columna);
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
    return expr_ident(p->arena, t.inicio, t.longitud, t.linea, t.columna);
}

static Expr *parsear_grupo(Parser *p) {
    Token apertura = p->actual;
    avanzar(p); /* consume '(' */
    Expr *interna = parsear_expresion(p);
    if (!consumir(p, TT_PARENT_DER, "se esperaba ')' para cerrar el grupo")) {
        return NULL;
    }
    return expr_grupo(p->arena, interna, apertura.linea, apertura.columna);
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
    Expr **args = buffer;
    int n = 0;
    int capacidad = 8;

    if (!check(p, TT_PARENT_DER)) {
        do {
            if (n >= capacidad) {
                capacidad *= 2;
                Expr **nuevo = (Expr **)arena_alocar(p->arena,
                    sizeof(Expr *) * (size_t)capacidad);
                if (nuevo == NULL) return NULL;
                memcpy(nuevo, args, sizeof(Expr *) * (size_t)n);
                args = nuevo;
            }
            Expr *arg = parsear_expresion(p);
            if (arg == NULL) return NULL;
            args[n++] = arg;
        } while (consumir_si(p, TT_COMA));
    }

    if (!consumir(p, TT_PARENT_DER, "se esperaba ')' para cerrar la llamada")) {
        return NULL;
    }

    /* Copiar args al arena para que sobrevivan al stack frame. */
    Expr **args_finales = (Expr **)arena_alocar(p->arena, sizeof(Expr *) * (size_t)(n > 0 ? n : 1));
    if (args_finales == NULL) return NULL;
    if (n > 0) memcpy(args_finales, args, sizeof(Expr *) * (size_t)n);

    return expr_llamada(p->arena, callee, args_finales, n,
                        apertura.linea, apertura.columna);
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

    /* Agrupación / llamada / atributo */
    reglas[TT_PARENT_IZQ]  = (ReglaParseo){ parsear_grupo, parsear_llamada, PREC_LLAMADA };
    reglas[TT_PUNTO]       = (ReglaParseo){ NULL,         parsear_atributo, PREC_LLAMADA };

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
    reglas[TT_NO]          = (ReglaParseo){ parsear_no, NULL, PREC_NULA };

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
