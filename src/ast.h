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
typedef struct ParteFCadena ParteFCadena;

/* ──────────────────────────────────────────────────────────────────
 * Expresiones
 * ────────────────────────────────────────────────────────────────── */

typedef enum {
    /* Literales */
    EXPR_LITERAL_ENTERO,        /* 42, 0xff (lexema sin parsear todavía) */
    EXPR_LITERAL_DECIMAL,       /* 3.14, 1.5e-3 */
    EXPR_LITERAL_CADENA,        /* "hola", 'mundo' (incluye comillas) */
    EXPR_LITERAL_F_CADENA,      /* f"hola {nombre}" — partes en como.f_cadena */
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

    /* Colecciones literales */
    EXPR_LISTA,                 /* [a, b, c] */
    EXPR_DICCIONARIO,           /* {k: v, ...} */
    EXPR_CONJUNTO,              /* {a, b, c} */
    EXPR_TUPLA,                 /* (a, b) — distinto de grupo (a) */

    /* Indexación / slicing */
    EXPR_INDICE,                /* obj[k] */
    EXPR_REBANADA,              /* obj[a:b:c] (a, b, c todos opcionales) */

    /* `super.metodo` — solo dentro de un método de una subclase. */
    EXPR_SUPER,
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
            bool *args_spread;  /* v1.22: por-arg, true si es `*expr` */
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

        /* Lista, conjunto, tupla: secuencia de elementos. */
        struct {
            Expr **elementos;
            int n_elementos;
        } secuencia;

        /* Diccionario: pares clave-valor en arrays paralelos. */
        struct {
            Expr **claves;
            Expr **valores;
            int n_pares;
        } diccionario;

        /* Indexación obj[k]. */
        struct {
            Expr *objeto;
            Expr *indice;
        } indice;

        /* Slicing obj[inicio:fin:paso]. Cualquier campo puede ser NULL. */
        struct {
            Expr *objeto;
            Expr *inicio;
            Expr *fin;
            Expr *paso;
        } rebanada;

        /* `super.metodo`: guardamos solo el nombre del método. La
           "expresión super" en sí misma no tiene un objeto explícito —
           el receptor `yo` es implícito (slot 1 del frame del método)
           y la superclase se resuelve en runtime via `yo.clase.superclase`. */
        struct {
            const char *nombre;       /* nombre del método tras el punto */
            int longitud;
        } super;

        /*
         * F-cadena descompuesta en partes (v1.1).
         *
         * Cada parte es o bien literal (texto plano entre `{...}`)
         * o expresión (sub-Expr a evaluar y formatear). El array
         * `partes` se aloca en arena.
         *
         * Convenciones:
         *   - parte.expr == NULL  →  parte literal, lee `literal`/`longitud`.
         *   - parte.expr != NULL  →  parte expresión, ignora literal.
         */
        struct {
            ParteFCadena *partes;
            int n_partes;
        } f_cadena;
    } como;
};

/*
 * Una parte de una f-cadena: literal o expresión.
 *
 * Para parte literal `expr` es NULL y `literal`/`longitud` apuntan al
 * texto (alocado en arena, ya procesado: `{{` se ha colapsado a `{`,
 * `}}` a `}`, escapes `\n` resueltos).
 *
 * Para parte expresión `expr` es no-NULL y los demás campos se ignoran.
 */
struct ParteFCadena {
    const char *literal;
    int longitud;
    Expr *expr;
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
Expr *expr_literal_f_cadena(Arena *a, ParteFCadena *partes, int n_partes,
                              int linea, int col);
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
Expr *expr_lista(Arena *a, Expr **elementos, int n, int linea, int col);
Expr *expr_diccionario(Arena *a, Expr **claves, Expr **valores, int n,
                       int linea, int col);
Expr *expr_conjunto(Arena *a, Expr **elementos, int n, int linea, int col);
Expr *expr_tupla(Arena *a, Expr **elementos, int n, int linea, int col);
Expr *expr_indice(Arena *a, Expr *objeto, Expr *indice, int linea, int col);
Expr *expr_rebanada(Arena *a, Expr *objeto, Expr *inicio, Expr *fin, Expr *paso,
                    int linea, int col);
Expr *expr_super(Arena *a, const char *nombre, int len, int linea, int col);

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
    SENT_COINCIDIR,      /* `coincidir expr: cuando ... fin coincidir` (v1.15) */
} TipoSent;

/*
 * Patrones de `coincidir` (v1.15+).
 *
 * v1.15: literal, bind, wildcard.
 * v1.16: tupla, lista (estructurales con anidación).
 */
typedef enum {
    PATRON_WILDCARD,     /* `_`: matches todo, no bindea */
    PATRON_LITERAL,      /* literal entero/decimal/cadena/booleano/nulo */
    PATRON_BIND,         /* identificador: bindea el valor a un local */
    PATRON_TUPLA,        /* `(p1, p2, ...)`: matchea tupla con misma aridad y patrones */
    PATRON_LISTA,        /* `[p1, p2, ...]`: matchea lista con misma longitud y patrones */
    PATRON_OR,           /* `p1 | p2 | ...`: matchea si alguno coincide (v1.16.2) */
    PATRON_STAR_BIND,    /* `*nombre` dentro de una lista; captura el resto (v1.16.2) */
    PATRON_TIPO,         /* `Foo()`: matchea si sujeto es instancia de Foo (v1.16.3) */
} TipoPatron;

typedef struct Patron {
    TipoPatron tipo;
    int linea, columna;
    union {
        Expr *literal;             /* PATRON_LITERAL */
        struct {
            const char *nombre;
            int longitud;
        } bind;                    /* PATRON_BIND */
        struct {
            struct Patron **elementos;
            int n;
        } estructural;             /* PATRON_TUPLA, PATRON_LISTA, PATRON_OR */
    } como;
} Patron;

/*
 * Una cláusula `cuando` de un `coincidir`:
 *   cuando <patron> [como <nombre>] [si <guarda>]: <cuerpo>
 *
 * `bind_completo_texto/longitud` reservan un binding del sujeto entero,
 * útil con type-match: `cuando Vector() como v: ... v.x ...`.
 * texto NULL si no hay `como`.
 */
typedef struct {
    Patron *patron;
    const char *bind_completo_texto;
    int bind_completo_longitud;
    Expr *guarda;          /* NULL si no hay `si <guarda>` */
    Sent *cuerpo;          /* SENT_BLOQUE */
    int linea, columna;
} ClausulaCuando;

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
    bool es_estrella;           /* v1.22: `*resto` recoge args sobrantes */
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

        struct {
            Expr *sujeto;
            ClausulaCuando *clausulas;
            int n_clausulas;
        } coincidir;
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
Sent *sent_coincidir(Arena *a, Expr *sujeto,
                     ClausulaCuando *clausulas, int n_clausulas,
                     int linea, int col);

/* Constructores de patrones (v1.15-v1.16). */
Patron *patron_wildcard(Arena *a, int linea, int col);
Patron *patron_literal(Arena *a, Expr *lit, int linea, int col);
Patron *patron_bind(Arena *a, const char *nombre, int len, int linea, int col);
Patron *patron_tupla(Arena *a, Patron **elementos, int n, int linea, int col);
Patron *patron_lista(Arena *a, Patron **elementos, int n, int linea, int col);
Patron *patron_or(Arena *a, Patron **alternativas, int n, int linea, int col);
Patron *patron_star_bind(Arena *a, const char *nombre, int len, int linea, int col);
Patron *patron_tipo(Arena *a, const char *nombre, int len, int linea, int col);

/* Pretty-printer para sentencias. Formato S-expression. */
void sent_imprimir(const Sent *s, FILE *salida);
int sent_a_cadena(const Sent *s, char *buffer, int capacidad);

#endif /* CORNAMUSA_AST_H */
