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
    /* `capturado`: true si una función anidada captura este local en
       un upvalue. El compilador emite OP_CERRAR_UPVALUE al salir del
       scope para mover el valor del stack al heap. (Reservado para
       futuro; actualmente confiamos en que OP_RETORNAR cierra todos
       los upvalues abiertos del frame.) */
    bool capturado;
} LocalCompilador;

/*
 * Upvalue tracked en el compilador: cuando un scope captura una
 * variable de un padre, se registra aquí con `es_local` (verdadero si
 * es local del padre directo) e `indice` (slot en el padre o índice
 * en el upvalue del padre).
 */
typedef struct {
    bool es_local;
    uint8_t indice;
} UpvalueCompilador;

/*
 * Scope de compilación: representa una función en construcción (o
 * el cuerpo principal del programa). Permite anidamiento mediante
 * `padre` para compilar funciones definidas dentro de otras (sin
 * closures todavía: el bytecode emitido no captura locales del
 * padre — sólo lookup de globales).
 */
#define COMPILADOR_UPVALUES_MAX 256

/*
 * Marcador de declaración `nolocal x` en el scope actual (v1.4).
 * Cuando una sentencia `nolocal x, y` se compila, registra cada
 * nombre aquí. La asignación posterior a `x` resolverá vía
 * OP_ASIGNAR_UPVALUE (buscando la variable en scopes envolventes)
 * en lugar de crear una local nueva.
 */
#define COMPILADOR_NOLOCALES_MAX 64

typedef struct {
    const char *nombre;
    int longitud_nombre;
} NolocalMarker;

typedef struct ScopeCompilador {
    Chunk *chunk;             /* chunk donde se emite el código */
    bool es_funcion;          /* false en el scope top-level */

    LocalCompilador locales[COMPILADOR_LOCALES_MAX];
    int n_locales;            /* slots ocupados (incluye slot 0 = callee) */

    UpvalueCompilador upvalues[COMPILADOR_UPVALUES_MAX];
    int n_upvalues;

    BucleAbierto bucles[COMPILADOR_BUCLES_MAX];
    int n_bucles;

    /* v1.4: nombres declarados como `nolocal` en este scope. Antes de
       crear una local nueva por asignación implícita, el compilador
       consulta esta lista. */
    NolocalMarker nolocales[COMPILADOR_NOLOCALES_MAX];
    int n_nolocales;

    /* Pointer a la `FuncionBC` que estamos compilando (para llenar la
       metadata `info_upvalues` a medida que el scope captura). NULL en
       el scope raíz. */
    FuncionBC *funcion;

    struct ScopeCompilador *padre;
} ScopeCompilador;

/*
 * Pila de aliases de atrapadores anidados activos en el sitio de
 * compilación actual. Cada entrada es el slot del local que contiene
 * la excepción atrapada. `lanzar` sin valor (re-raise) emite
 * `OP_OBTENER_LOCAL [slot top] + OP_LANZAR`. Si `n_atrapadores_activos
 * == 0`, `lanzar` sin valor es error de compilación.
 */
#define COMPILADOR_ATRAPADORES_MAX 16

typedef struct {
    /*
     * El scope raíz se almacena directamente en el Compilador (más
     * cómodo que heap). Los scopes de función anidada se enlazan con
     * `padre` y viven en stack del compilador.
     */
    ScopeCompilador raiz;
    ScopeCompilador *actual;
    EvalError error;
    /* Aliases de atrapadores activos para `lanzar` re-raise (v0.8.3). */
    int atrapador_alias_slots[COMPILADOR_ATRAPADORES_MAX];
    int n_atrapadores_activos;
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
