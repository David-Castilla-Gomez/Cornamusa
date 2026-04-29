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

/*
 * Estado de un bucle abierto en compilación. Mantiene la información
 * necesaria para que `romper` y `continuar` emitan saltos correctos.
 */
typedef struct {
    int inicio_continuar;
    int *parches_romper;
    int n_parches;
    int cap_parches;
} BucleAbierto;

#define COMPILADOR_BUCLES_MAX 16
#define COMPILADOR_LOCALES_MAX 256

/*
 * Una variable local en el scope actual de compilación. El slot se
 * deriva del orden de declaración: el slot 0 es la propia función
 * (callee), 1..aridad son los parámetros, slots posteriores son
 * locales declaradas dinámicamente al primero asignarse dentro del
 * cuerpo.
 */
typedef struct {
    const char *nombre;
    int longitud_nombre;
} LocalCompilador;

/*
 * Scope de compilación: representa una función en construcción (o
 * el cuerpo principal del programa). Permite anidamiento mediante
 * `padre` para compilar funciones definidas dentro de otras (sin
 * closures todavía: el bytecode emitido no captura locales del
 * padre — sólo lookup de globales).
 */
typedef struct ScopeCompilador {
    Chunk *chunk;             /* chunk donde se emite el código */
    bool es_funcion;          /* false en el scope top-level */

    LocalCompilador locales[COMPILADOR_LOCALES_MAX];
    int n_locales;            /* slots ocupados (incluye slot 0 = callee) */

    BucleAbierto bucles[COMPILADOR_BUCLES_MAX];
    int n_bucles;

    struct ScopeCompilador *padre;
} ScopeCompilador;

typedef struct {
    /*
     * El scope raíz se almacena directamente en el Compilador (más
     * cómodo que heap). Los scopes de función anidada se enlazan con
     * `padre` y viven en stack del compilador.
     */
    ScopeCompilador raiz;
    ScopeCompilador *actual;
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

/*
 * Compila una sentencia. Las sentencias simples (asignación,
 * sentencia-expresión, pasar, bloque) ya están soportadas. Funciones,
 * control de flujo y excepciones llegan en sesiones siguientes.
 */
bool compilador_compilar_sent(Compilador *c, const Sent *s);

/*
 * Compila un programa completo: cada sentencia en orden y emite al
 * final un OP_NULO + OP_RETORNAR para que `vm_ejecutar` retorne nulo.
 */
bool compilador_compilar_programa(Compilador *c, Sent **sents, int n);

#endif /* CORNAMUSA_COMPILADOR_H */
