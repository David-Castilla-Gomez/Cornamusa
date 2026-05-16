#ifndef CORNAMUSA_LINTER_H
#define CORNAMUSA_LINTER_H

#include <stdbool.h>
#include <stddef.h>

#include "ast.h"

/*
 * Linter de Cornamusa (v1.49 - Fase 5 tooling).
 *
 * Recibe un programa parseado (`Sent **sents`, `n_sents`) y produce una
 * lista de avisos. No modifica el AST. No reporta errores de sintaxis
 * — eso es trabajo del parser; el linter solo opera sobre programas que
 * parsearon correctamente.
 *
 * Categorias en v1.49:
 *   - UNREACHABLE      : codigo tras `retornar`/`romper`/`continuar`/`lanzar`.
 *   - REDUNDANT_PASAR  : `pasar` en un bloque con otras sentencias.
 *   - EQ_NULO          : `x == nulo` / `x != nulo` (prefiere `es nulo` / `no es nulo`).
 *   - UNUSED_IMPORT    : importacion no referenciada en el programa.
 *
 * Anadido en v1.50 (scope analysis para funciones y lambdas):
 *   - UNUSED_LOCAL     : variable local asignada en body de funcion nunca leida.
 *   - UNUSED_PARAM     : parametro de funcion nunca leido.
 *     Skip rule: `yo`, nombres que empiezan con `_`, `*args`/`**kwargs`.
 *
 * Lo que NO chequea (queda para v1.51+):
 *   - Variables sombra entre scopes (shadowing).
 *   - Loop var `para X` no usado (idiom: usar `_`).
 *   - Aridades incorrectas (built-ins ya validan en runtime).
 *   - Tipos (Cornamusa es dinamico — el tipado opcional es trabajo lejano).
 */

typedef enum {
    LINT_UNREACHABLE,
    LINT_REDUNDANT_PASAR,
    LINT_EQ_NULO,
    LINT_UNUSED_IMPORT,
    LINT_UNUSED_LOCAL,    /* v1.50: variable local de funcion no usada */
    LINT_UNUSED_PARAM,    /* v1.50: parametro de funcion no usado */
} TipoWarning;

typedef struct {
    TipoWarning tipo;
    int linea;
    int columna;
    char *mensaje;        /* malloc'd, owned por el resultado */
} Warning;

typedef struct {
    Warning *avisos;      /* malloc'd vector, ordenado por linea/columna */
    int n;
    int capacidad;
} LinterResultado;

LinterResultado linter_analizar(Sent **sents, int n);

void linter_resultado_destruir(LinterResultado *r);

/* Devuelve el nombre corto de la categoria, p.ej. "unreachable". */
const char *linter_tipo_nombre(TipoWarning t);

#endif /* CORNAMUSA_LINTER_H */
