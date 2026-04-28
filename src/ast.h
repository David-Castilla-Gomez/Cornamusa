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

/* Forward declaration para auto-referencias en variantes recursivas. */
struct Expr;
typedef struct Expr Expr;

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

#endif /* CORNAMUSA_AST_H */
