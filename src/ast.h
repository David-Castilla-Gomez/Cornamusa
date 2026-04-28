#ifndef CORNAMUSA_AST_H
#define CORNAMUSA_AST_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "arena.h"
#include "lexer.h"

/*
 * Árbol de Sintaxis Abstracta (AST) de Cornamusa.
 *
 * Decisión arquitectónica B2: el AST es infraestructura compartida
 * entre el evaluador tree-walking (Fase 4-5) y el compilador a
 * bytecode (Fase 6+). Ambos backends visitan los mismos nodos.
 *
 * Decisión de implementación: tagged union (`enum` + `struct` con
 * `union como`). Una sola definición por categoría (Expr, Sent),
 * dispatch por `tipo`. Patrón tomado de clox.
 *
 * Los nodos se alocan en una `Arena` que vive más que el AST. La
 * arena se libera de golpe al destruir el AST. Los nodos NO copian
 * cadenas del lexer — apuntan al buffer fuente original, que también
 * debe vivir más que el AST. El cliente del parser es responsable de
 * mantener fuente, lexer y arena vivos durante el uso del AST.
 *
 * Esta versión (Fase 3 sesión 1) define las expresiones. Las
 * sentencias llegan en sesión 2.
 */

/* Forward declarations para auto-referencias entre Expr y Sent. */
struct Expr;
typedef struct Expr Expr;
struct Sent;
typedef struct Sent Sent;
struct RamaSi;
typedef struct RamaSi RamaSi;
struct Parametro;
typedef struct Parametro Parametro;

/* ──────────────────────────────────────────────────────────────────
 * Expresiones
 * ────────────────────────────────────────────────────────────────── */

typedef enum {
    /* Literales */
    EXPR_LITERAL_ENTERO,        /* 42, 0xff (lexema sin parsear todavía) */
    EXPR_LITERAL_DECIMAL,       /* 3.14, 1.5e-3 */
    EXPR_LITERAL_CADENA,        /* "hola", 'mundo' (incluye comillas) */
    EXPR_LITERAL_F_CADENA,      /* f"hola {nombre}" — interp diferida a s5 */
    EXPR_LITERAL_BOOLEANO,      /* verdadero / falso */
    EXPR_LITERAL_NULO,          /* nulo */

    /* Identificador */
    EXPR_IDENT,                 /* nombre, niño, función_principal */

    /* Operaciones */
    EXPR_BINARIO,               /* a OP b — aritméticos, comparaciones, bitwise */
    EXPR_UNARIO,                /* -a, +a, no a, ~a */
    EXPR_LOGICA,                /* a y b, a o b (con short-circuit) */

    /* Acceso */
    EXPR_LLAMADA,               /* f(args) */
    EXPR_ATRIBUTO,              /* obj.atr */

    /* Agrupación / sentinela */
    EXPR_GRUPO,                 /* (expr) — preserva el AST aunque no aporta semántica */

    /* Lambda: cuerpo es una sola expresión (no bloque). */
    EXPR_LAMBDA,                /* lambda x, y: x + y */
} TipoExpr;

struct Expr {
    TipoExpr tipo;
    int linea;                  /* del primer token del nodo */
    int columna;                /* columna del primer token */

    union {
        /* Para todos los literales: guardamos puntero al lexema en
           fuente y su longitud en bytes. La conversión a int64/double/
           bytes se hará en el evaluador o compilador. */
        struct {
            const char *lexema;
            int longitud;
        } literal;

        struct {
            bool valor;         /* verdadero o falso */
        } booleano;

        /* (EXPR_LITERAL_NULO no tiene datos; el `tipo` es suficiente.) */

        struct {
            const char *nombre; /* puntero al buffer fuente */
            int longitud;
        } ident;

        struct {
            Expr *izq;
            Expr *der;
            TipoToken op;       /* TT_MAS, TT_MENOS, TT_IGUAL, etc. */
        } binario;

        struct {
            TipoToken op;       /* TT_MENOS, TT_MAS, TT_NO, TT_TILDE_BIT */
            Expr *operando;
        } unario;

        struct {
            Expr *izq;
            Expr *der;
            bool es_y;          /* true = `y`, false = `o` */
        } logica;

        struct {
            Expr *callee;       /* la expresión que produce la función */
            Expr **args;        /* array alocado en arena */
            int n_args;
        } llamada;

        struct {
            Expr *objeto;
            const char *nombre; /* nombre del atributo (apunta a fuente) */
            int longitud;
        } atributo;

        struct {
            Expr *interna;
        } grupo;

        struct {
            Parametro *parametros;
            int n_parametros;
            Expr *cuerpo;       /* una sola expresión tras `:` */
        } lambda;
    } como;
};

/* ──────────────────────────────────────────────────────────────────
 * Constructores
 *
 * Cada constructor toma una arena, los datos del nodo, y la posición
 * (línea/columna). Devuelven el puntero al nodo recién alocado, o NULL
 * si la arena no pudo alocar (raro fuera de OOM).
 *
 * Estos constructores no hacen validación semántica — el parser
 * decide qué nodos crear.
 * ────────────────────────────────────────────────────────────────── */

Expr *expr_literal_entero(Arena *a, const char *lexema, int len, int linea, int col);
Expr *expr_literal_decimal(Arena *a, const char *lexema, int len, int linea, int col);
Expr *expr_literal_cadena(Arena *a, const char *lexema, int len, int linea, int col);
Expr *expr_literal_f_cadena(Arena *a, const char *lexema, int len, int linea, int col);
Expr *expr_literal_booleano(Arena *a, bool valor, int linea, int col);
Expr *expr_literal_nulo(Arena *a, int linea, int col);
Expr *expr_ident(Arena *a, const char *nombre, int len, int linea, int col);
Expr *expr_binario(Arena *a, Expr *izq, TipoToken op, Expr *der, int linea, int col);
Expr *expr_unario(Arena *a, TipoToken op, Expr *operando, int linea, int col);
Expr *expr_logica(Arena *a, Expr *izq, bool es_y, Expr *der, int linea, int col);
Expr *expr_llamada(Arena *a, Expr *callee, Expr **args, int n_args, int linea, int col);
Expr *expr_atributo(Arena *a, Expr *objeto, const char *nombre, int len, int linea, int col);
Expr *expr_grupo(Arena *a, Expr *interna, int linea, int col);
Expr *expr_lambda(Arena *a, Parametro *params, int n_params, Expr *cuerpo,
                  int linea, int col);

/* ──────────────────────────────────────────────────────────────────
 * Pretty-printer
 *
 * Imprime el AST en formato S-expression compacto, una línea por defecto.
 * Útil para tests (cadenas comparables) y depuración.
 *
 * Ejemplos:
 *   expr_imprimir(suma_3_4)        →  "(+ 3 4)"
 *   expr_imprimir(if_else)         →  "(si (== x 0) ...)"   (no en s1)
 *   expr_imprimir(llamada_print_x) →  "(llamada imprimir x)"
 * ────────────────────────────────────────────────────────────────── */

void expr_imprimir(const Expr *e, FILE *salida);

/*
 * Variante que imprime a un buffer dado, devolviendo el número de
 * bytes escritos (sin contar '\0' final). Útil para tests comparando
 * strings exactos. Si el buffer no es suficiente, trunca silencio-
 * samente — el cliente debe reservar buffer grande.
 */
int expr_a_cadena(const Expr *e, char *buffer, int capacidad);

/* ──────────────────────────────────────────────────────────────────
 * Sentencias
 *
 * Una sentencia es una unidad ejecutable: asignación, control de
 * flujo, expresión-como-sentencia. Cornamusa no es expresión-como-
 * todo (Rust style); las sentencias son distintas de las expresiones.
 *
 * Esta versión (Fase 3 sesión 2) define las sentencias simples y
 * de control de flujo. Funciones, clases, excepciones y módulos
 * llegan en sesiones 3-4.
 * ────────────────────────────────────────────────────────────────── */

typedef enum {
    SENT_EXPR,           /* `imprimir(x)` — expresión usada como sentencia */
    SENT_ASIGNAR,        /* `x = expr` */
    SENT_ASIGNAR_AUG,    /* `x += expr`, `x -= expr`, etc. */
    SENT_PASAR,          /* `pasar` */
    SENT_ROMPER,         /* `romper` */
    SENT_CONTINUAR,      /* `continuar` */
    SENT_RETORNAR,       /* `retornar [expr]` */
    SENT_SI,             /* `si ... sino si ... sino ... fin si` */
    SENT_MIENTRAS,       /* `mientras ... [sino ...] fin mientras` */
    SENT_PARA,           /* `para X en Y: ... [sino ...] fin para` */
    SENT_BLOQUE,         /* secuencia de sentencias (cuerpo de bloque) */
    SENT_FUNCION,        /* `funcion nombre(params): ... fin funcion` */
    SENT_CLASE,          /* `clase Nombre [extiende ...]: ... fin clase` */
    SENT_INTENTAR,       /* `intentar: ... atrapar...: ... [sino:...] [finalmente:...] fin intentar` */
    SENT_LANZAR,         /* `lanzar [expr]` (sin expr = re-raise) */
    SENT_IMPORTAR,       /* `importar X.Y.Z [como W]` */
    SENT_DESDE_IMPORTAR, /* `desde X.Y importar A [como A2], B, * ` */
    SENT_GLOBAL,         /* `global a, b, c` */
    SENT_NOLOCAL,        /* `nolocal a, b, c` */
} TipoSent;

/*
 * Un parámetro de función o lambda. Por ESPEC §5:
 *   parametro ← IDENT (":" expr)? ("=" expr)?
 *
 * Ej. `nombre`, `nombre: cadena`, `idioma="es"`, `n: entero=0`.
 *
 * El AST guarda anotacion_tipo aunque Cornamusa no la enforce
 * (tipado dinámico). Se reserva para análisis estático opcional
 * en versiones futuras.
 */
struct Parametro {
    const char *nombre;
    int longitud_nombre;
    Expr *anotacion_tipo;       /* NULL si no hay */
    Expr *valor_defecto;        /* NULL si no hay */
    int linea, columna;
};

/*
 * Una rama de un `si`. Para la rama `sino` final, `condicion` es NULL.
 * Para las ramas `si` y `sino si`, `condicion` está presente.
 */
struct RamaSi {
    Expr *condicion;     /* NULL en la rama 'sino' final */
    Sent *cuerpo;        /* siempre un SENT_BLOQUE */
    int linea, columna;  /* del 'si'/'sino si'/'sino' */
};

/*
 * Nombre simbólico que apunta al buffer fuente. Usado para listas
 * de identificadores en `global`/`nolocal`/`importar`/etc., evitando
 * crear muchos pequeños nodos Expr.
 */
typedef struct {
    const char *texto;
    int longitud;
} Nombre;

/*
 * Item importado en `desde X importar Y como Z, W, ...`. Si no hay
 * alias, `alias.texto` es NULL.
 */
typedef struct {
    Nombre nombre;
    Nombre alias;
    int linea, columna;
} ItemImportado;

/*
 * Una cláusula `atrapar` de un bloque `intentar`. Tres formas:
 *   atrapar:                    → tipo=NULL, alias.texto=NULL
 *   atrapar TipoExc:            → tipo=Expr, alias.texto=NULL
 *   atrapar TipoExc como e:     → tipo=Expr, alias=Nombre
 */
typedef struct {
    Expr *tipo;             /* NULL si bare 'atrapar:' */
    Nombre alias;           /* alias.texto NULL si no hay 'como' */
    Sent *cuerpo;           /* SENT_BLOQUE */
    int linea, columna;
} ClausulaAtrapar;

struct Sent {
    TipoSent tipo;
    int linea, columna;

    union {
        struct { Expr *expr; } expr;

        struct {
            Expr *destino;
            Expr *valor;
        } asignar;

        struct {
            Expr *destino;
            TipoToken op;       /* TT_ASIGNAR_MAS, TT_ASIGNAR_MENOS, etc. */
            Expr *valor;
        } asignar_aug;

        struct {
            Expr *valor;        /* NULL si `retornar` sin expresión */
        } retornar;

        struct {
            RamaSi *ramas;
            int n_ramas;
        } si;

        struct {
            Expr *condicion;
            Sent *cuerpo;
            Sent *sino;         /* NULL si no hay cláusula sino */
        } mientras;

        struct {
            Expr *objetivo;     /* identificador (en sesión 5: tupla) */
            Expr *iterable;
            Sent *cuerpo;
            Sent *sino;
        } para;

        struct {
            Sent **sentencias;
            int n_sentencias;
        } bloque;

        struct {
            const char *nombre;
            int longitud_nombre;
            Parametro *parametros;
            int n_parametros;
            Expr *anotacion_retorno;    /* `-> tipo`, NULL si no hay */
            Sent *cuerpo;               /* SENT_BLOQUE */
        } funcion;

        struct {
            const char *nombre;
            int longitud_nombre;
            Expr **superclases;         /* lista; vacía si no hay `extiende` */
            int n_superclases;
            Sent *cuerpo;               /* SENT_BLOQUE de métodos/asignaciones */
        } clase;

        struct {
            Sent *cuerpo;               /* SENT_BLOQUE del intentar */
            ClausulaAtrapar *atrapadores;
            int n_atrapadores;
            Sent *sino;                 /* NULL si no hay 'sino' (no excepción) */
            Sent *finalmente;           /* NULL si no hay */
        } intentar;

        struct {
            Expr *valor;                /* NULL = re-raise */
        } lanzar;

        struct {
            Nombre *segmentos;          /* `mat.geometria` → [mat, geometria] */
            int n_segmentos;
            Nombre alias;               /* 'como X', alias.texto NULL si no hay */
        } importar;

        struct {
            Nombre *segmentos_modulo;
            int n_segmentos_modulo;
            ItemImportado *items;       /* lista de items importados */
            int n_items;
            bool importa_todo;          /* `desde X importar *` */
        } desde_importar;

        struct {
            Nombre *nombres;            /* lista de identificadores */
            int n_nombres;
        } global_o_nolocal;             /* compartido por SENT_GLOBAL y SENT_NOLOCAL */
    } como;
};

/* Constructores de sentencias. */
Sent *sent_expr(Arena *a, Expr *e, int linea, int col);
Sent *sent_asignar(Arena *a, Expr *destino, Expr *valor, int linea, int col);
Sent *sent_asignar_aug(Arena *a, Expr *destino, TipoToken op, Expr *valor,
                       int linea, int col);
Sent *sent_pasar(Arena *a, int linea, int col);
Sent *sent_romper(Arena *a, int linea, int col);
Sent *sent_continuar(Arena *a, int linea, int col);
Sent *sent_retornar(Arena *a, Expr *valor, int linea, int col);
Sent *sent_si(Arena *a, RamaSi *ramas, int n_ramas, int linea, int col);
Sent *sent_mientras(Arena *a, Expr *cond, Sent *cuerpo, Sent *sino,
                    int linea, int col);
Sent *sent_para(Arena *a, Expr *objetivo, Expr *iterable, Sent *cuerpo,
                Sent *sino, int linea, int col);
Sent *sent_bloque(Arena *a, Sent **sentencias, int n, int linea, int col);
Sent *sent_funcion(Arena *a, const char *nombre, int len_nombre,
                   Parametro *params, int n_params,
                   Expr *anot_retorno, Sent *cuerpo,
                   int linea, int col);
Sent *sent_clase(Arena *a, const char *nombre, int len_nombre,
                 Expr **supers, int n_supers, Sent *cuerpo,
                 int linea, int col);
Sent *sent_intentar(Arena *a, Sent *cuerpo,
                    ClausulaAtrapar *atrapadores, int n_atrapadores,
                    Sent *sino, Sent *finalmente,
                    int linea, int col);
Sent *sent_lanzar(Arena *a, Expr *valor, int linea, int col);
Sent *sent_importar(Arena *a, Nombre *segmentos, int n_segmentos,
                    Nombre alias, int linea, int col);
Sent *sent_desde_importar(Arena *a, Nombre *segmentos_modulo, int n_seg,
                           ItemImportado *items, int n_items,
                           bool importa_todo, int linea, int col);
Sent *sent_global(Arena *a, Nombre *nombres, int n_nombres, int linea, int col);
Sent *sent_nolocal(Arena *a, Nombre *nombres, int n_nombres, int linea, int col);

/* Pretty-printer para sentencias. Formato S-expression. */
void sent_imprimir(const Sent *s, FILE *salida);
int sent_a_cadena(const Sent *s, char *buffer, int capacidad);

#endif /* CORNAMUSA_AST_H */
