#ifndef CORNAMUSA_COMPILADOR_H
#define CORNAMUSA_COMPILADOR_H

#include <stdbool.h>

#include "ast.h"
#include "chunk.h"
#include "evaluador.h"   /* EvalError para reportar errores de compilación */

/*
 * Compilador AST → bytecode (Fase 6 sesión 2).
 *
 * Visita el árbol producido por el parser y emite instrucciones a
 * un `Chunk` que la VM ejecutará. En esta sesión inicial soporta:
 *
 *   - Literales: entero, decimal, cadena, booleano, nulo.
 *   - Operadores binarios aritméticos, comparaciones, identidad,
 *     membership, bitwise.
 *   - Operadores unarios: `-x`, `+x`, `no x`, `~x`.
 *   - `EXPR_GRUPO` (passthrough).
 *
 * Aplazadas (devuelven error explícito): identificadores, llamadas,
 * lambda, asignación, control de flujo, lógica con cortocircuito.
 * Llegan en sesiones 3-5.
 *
 * El compilador es de un único pase — no construye AST nuevo, solo
 * emite bytes. La precedencia ya está resuelta por el parser, así
 * que aquí solo se elige el opcode adecuado.
 */

typedef struct {
    Chunk *chunk;
    EvalError error;
} Compilador;

void compilador_iniciar(Compilador *c, Chunk *chunk);

/*
 * Compila una expresión emitiéndo el bytecode que la evalúa y deja
 * un único Valor en el tope de la pila al ejecutarse. Devuelve true
 * si OK; false si hubo error (ya reportado en `c->error`).
 */
bool compilador_compilar_expr(Compilador *c, const Expr *e);

/*
 * Compila una expresión y emite OP_RETORNAR al final, dejando el
 * chunk listo para ejecutarse con `vm_ejecutar`.
 */
bool compilador_compilar_expr_top(Compilador *c, const Expr *e);

#endif /* CORNAMUSA_COMPILADOR_H */
