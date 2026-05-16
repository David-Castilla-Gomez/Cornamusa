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
 * Anadido en v1.55:
 *   - SHADOW           : local sombrea nombre de scope exterior (excluyendo
 *                        nolocal/global, que son intencionales).
 *   - UNUSED_LOOP_VAR  : `para X en ...:` cuyo cuerpo no usa X.
 *                        Skip rule: nombres con `_` inicial.
 *   - MUTABLE_DEFAULT  : `funcion f(x=[])` o `=={}` o `=set()`. Default mutable
 *                        que se comparte entre llamadas (bug clasico Python).
 *
 * Anadido en v1.63:
 *   - CONCAT_IN_LOOP   : `x = x + ...` o `x += ...` dentro de `mientras`/`para`.
 *                        Patron O(n^2) para cadenas; usar lista + `cadena_unir`.
 *                        Skip rule: RHS literal numerico (`+= 1`, contadores).
 *
 * Supresion selectiva (v1.64): `# noqa: <categoria>` al final de
 * cualquier linea silencia el warning de esa categoria en esa linea.
 * Multiples categorias: `# noqa: cat1, cat2`. Bare `# noqa` silencia
 * todas las categorias. Util para casos didacticos que ilustran
 * intencionalmente un anti-patron.
 *
 * Lo que NO chequea:
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
    LINT_SHADOW,          /* v1.55: local sombrea variable de scope exterior */
    LINT_UNUSED_LOOP_VAR, /* v1.55: `para X en ...:` con X no usado en body */
    LINT_MUTABLE_DEFAULT, /* v1.55: default mutable (lista/dict/conjunto literal) */
    LINT_CONCAT_IN_LOOP,  /* v1.63: `x = x + ...` o `x += ...` en mientras/para */
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

/*
 * `fuente` (opcional, NULL para no consultar noqa) habilita la
 * directiva `# noqa: <categoria>` a final de linea para silenciar
 * warnings selectivamente. Si NULL, todos los warnings se emiten.
 */
LinterResultado linter_analizar(Sent **sents, int n, const char *fuente);

void linter_resultado_destruir(LinterResultado *r);

/* Devuelve el nombre corto de la categoria, p.ej. "unreachable". */
const char *linter_tipo_nombre(TipoWarning t);

#endif /* CORNAMUSA_LINTER_H */
