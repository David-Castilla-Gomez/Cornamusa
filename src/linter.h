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
 * Lo que NO chequea en v1.49 (queda para v1.50+):
 *   - Variables locales no usadas (requiere analisis de scope completo).
 *   - Sombras entre scopes.
 *   - Aridades incorrectas (built-ins ya validan en runtime).
 *   - Tipos (Cornamusa es dinamico — el tipado opcional es trabajo lejano).
 */

typedef enum {
    LINT_UNREACHABLE,
    LINT_REDUNDANT_PASAR,
    LINT_EQ_NULO,
    LINT_UNUSED_IMPORT,
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
