#ifndef CORNAMUSA_EVALUADOR_H
#define CORNAMUSA_EVALUADOR_H

#include <stdbool.h>

#include "ast.h"
#include "entorno.h"
#include "valor.h"

/*
 * Evaluador tree-walking de Cornamusa (Fase 4).
 *
 * Visita el AST nodo a nodo, mantiene un Entorno (scope chain) y
 * produce Valores como resultado. Para Fase 4 sesión 2 implementa el
 * núcleo de expresiones: literales, identificadores, operadores
 * aritméticos/comparación/bitwise, lógica con cortocircuito, unarios,
 * agrupación, identidad y operaciones simples sobre cadenas.
 *
 * Aplazado a sesiones siguientes:
 *   - Sesión 3: sentencias (asignación, si/mientras/para).
 *   - Sesión 4: funciones top-level + built-ins.
 *   - F5: colecciones (listas, diccionarios, conjuntos, tuplas) y
 *     EXPR_INDICE / EXPR_REBANADA / EXPR_LLAMADA / EXPR_LAMBDA.
 *
 * Modelo de errores: el evaluador NO usa setjmp/longjmp. Cada función
 * devuelve un Valor; en caso de error rellena `Evaluador.error` y
 * devuelve `valor_nulo()`. El llamador comprueba `evaluador_tiene_error`.
 *
 * Contrato de propiedad:
 *   - Valores devueltos por `evaluador_evaluar_expr` son del cliente,
 *     que debe llamar `valor_destruir` cuando los descarte.
 *   - Los argumentos a operaciones internas se destruyen dentro de las
 *     mismas para evitar fugas en errores intermedios.
 */

#define EVAL_MENSAJE_MAX 512

typedef struct {
    bool tuvo_error;
    char mensaje[EVAL_MENSAJE_MAX];
    int linea;
    int columna;
} EvalError;

typedef struct {
    Entorno *globales;          /* entorno raíz (no es dueño) */
    Entorno *entorno_actual;    /* entorno activo (puede coincidir con globales) */
    EvalError error;
} Evaluador;

/*
 * Inicializa el evaluador con un entorno de globales ya creado por el
 * cliente. El evaluador NO toma posesión del entorno — el cliente lo
 * destruye al terminar.
 */
void evaluador_iniciar(Evaluador *ev, Entorno *globales);

/*
 * Reinicia el flag de error a falso. Útil entre evaluaciones del REPL
 * para no propagar errores antiguos.
 */
void evaluador_limpiar_error(Evaluador *ev);

bool evaluador_tiene_error(const Evaluador *ev);

/*
 * Evalúa una expresión. El cliente es dueño del Valor devuelto.
 *
 * En caso de error rellena `ev->error` y devuelve `valor_nulo()` —
 * el cliente debe consultar `evaluador_tiene_error` antes de usar el
 * resultado.
 */
Valor evaluador_evaluar_expr(Evaluador *ev, const Expr *e);

#endif /* CORNAMUSA_EVALUADOR_H */
