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
static Expr *parsear_lambda(Parser *p);
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
    p->profundidad_bloques = 0;
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

    /* Lambda como expresión. */
    reglas[TT_LAMBDA]      = (ReglaParseo){ parsear_lambda, NULL, PREC_NULA };

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
static Sent *parsear_asignar_o_expr(Parser *p);
static Sent *parsear_cuerpo_bloque(Parser *p);
static bool parsear_lista_parametros(Parser *p, Parametro **out, int *n_out);
static const char *etiqueta_para_bloque(TipoBloque t);
static TipoToken token_para_bloque(TipoBloque t);
static bool consumir_fin(Parser *p, TipoBloque tipo, int linea_apertura);
static bool empujar_bloque(Parser *p, TipoBloque tipo, int linea);
static void salir_bloque(Parser *p);
static bool en_inicio_de_termino(Parser *p);

/* ¿El token actual termina un bloque (es `fin`, `sino`, EOF)? */
static bool en_inicio_de_termino(Parser *p) {
    return p->actual.tipo == TT_FIN
        || p->actual.tipo == TT_SINO
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

static Sent *parsear_para(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;
    avanzar(p); /* 'para' */

    /* Objetivo: por ahora un único identificador. Multi-objetivo
       (`a, b en pares`) llega en sesión 5. */
    if (!check(p, TT_IDENT)) {
        error_en(p, &p->actual,
            "se esperaba un nombre de variable tras 'para'");
        return NULL;
    }
    Token t_obj = p->actual;
    avanzar(p);
    Expr *objetivo = expr_ident(p->arena, t_obj.inicio, t_obj.longitud,
                                 t_obj.linea, t_obj.columna);

    if (!consumir(p, TT_EN, "se esperaba 'en' tras la variable de 'para'")) {
        return NULL;
    }
    Expr *iterable = parser_parsear_expr(p);
    if (iterable == NULL) return NULL;
    if (!consumir(p, TT_DOS_PUNTOS, "se esperaba ':' tras el iterable de 'para'")) {
        return NULL;
    }

    bool one_liner = (p->previo.linea == p->actual.linea);
    if (one_liner) {
        Sent *cuerpo = parsear_cuerpo_tras_dospuntos(p, BLOQUE_PARA, false, linea);
        if (cuerpo == NULL) return NULL;
        return sent_para(p->arena, objetivo, iterable, cuerpo, NULL, linea, col);
    }

    if (!empujar_bloque(p, BLOQUE_PARA, linea)) return NULL;
    Sent *cuerpo = parsear_cuerpo_bloque(p);
    if (cuerpo == NULL) { salir_bloque(p); return NULL; }

    Sent *sino = NULL;
    if (consumir_si(p, TT_SINO)) {
        if (!consumir(p, TT_DOS_PUNTOS, "se esperaba ':' tras 'sino' del 'para'")) {
            salir_bloque(p);
            return NULL;
        }
        sino = parsear_cuerpo_bloque(p);
        if (sino == NULL) { salir_bloque(p); return NULL; }
    }

    salir_bloque(p);
    if (!consumir_fin(p, BLOQUE_PARA, linea)) return NULL;

    return sent_para(p->arena, objetivo, iterable, cuerpo, sino, linea, col);
}

/*
 * Parsea una sentencia que empieza con una expresión: o bien
 * asignación (simple o aumentada), o bien expresión-como-sentencia.
 */
static Sent *parsear_asignar_o_expr(Parser *p) {
    int linea = p->actual.linea;
    int col = p->actual.columna;

    Expr *primero = parser_parsear_expr(p);
    if (primero == NULL) return NULL;

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
        case TT_SI:        return parsear_si(p);
        case TT_MIENTRAS:  return parsear_mientras(p);
        case TT_PARA:      return parsear_para(p);
        case TT_FUNCION:   return parsear_funcion(p);
        case TT_CLASE:     return parsear_clase(p);

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

    if (!check(p, TT_PARENT_DER)) {
        do {
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
       termina la lista de parámetros. Solo `= valor_defecto` opcional. */
    if (!check(p, TT_DOS_PUNTOS)) {
        do {
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
