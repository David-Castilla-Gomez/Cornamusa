#ifndef CORNAMUSA_EVALUADOR_H
#define CORNAMUSA_EVALUADOR_H

#include <stdbool.h>
#include <stdint.h>

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

typedef struct EvalError {
    bool tuvo_error;
    char mensaje[EVAL_MENSAJE_MAX];
    int linea;
    int columna;
} EvalError;

/*
 * Estado de control de flujo. El evaluador no usa setjmp ni excep-
 * ciones nativas: las sentencias `romper`, `continuar` y `retornar`
 * dejan una marca en `Evaluador.control` que las construcciones
 * envolventes (bucles, llamadas) interpretan y luego resetean.
 */
typedef enum {
    EJEC_NORMAL = 0,
    EJEC_ROMPER,
    EJEC_CONTINUAR,
    EJEC_RETORNAR,    /* habilitado en sesion 4 con funciones */
} ControlFlujo;

typedef struct Evaluador {
    Entorno *globales;          /* entorno raíz (no es dueño) */
    Entorno *entorno_actual;    /* entorno activo (puede coincidir con globales) */
    EvalError error;
    ControlFlujo control;
    Valor valor_retorno;        /* relleno al ejecutar SENT_RETORNAR (S4) */
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

/*
 * Ejecuta una sentencia. No devuelve valor. Si la sentencia es un
 * `romper`/`continuar`/`retornar`, deja `ev->control` con el estado
 * correspondiente y el llamador (bucle, función) lo gestiona.
 *
 * En caso de error pone `ev->error.tuvo_error` y aborta la ejecución
 * de cualquier bloque externo.
 */
void evaluador_ejecutar_sent(Evaluador *ev, const Sent *s);

/*
 * Aplica un operador binario sobre dos valores ya evaluados, tomando
 * posesión de ambos y devolviendo un Valor nuevo. Reutilizable desde
 * la VM bytecode (Fase 6+) — desacoplado del Evaluador para no obligar
 * a la VM a usar el árbol de sentencias del tree-walking.
 *
 * En caso de error rellena `*err` con línea/columna/mensaje y devuelve
 * nulo. Si `err->tuvo_error` ya estaba activo al entrar, no toca el
 * primer error (preserva el más antiguo).
 */
Valor evaluador_aplicar_binario(EvalError *err, int op_token,
                                 Valor a, Valor b,
                                 int linea, int columna);

/*
 * Aplica un operador unario sobre un valor ya evaluado. Mismo modelo
 * de errores que `evaluador_aplicar_binario`.
 */
Valor evaluador_aplicar_unario(EvalError *err, int op_token,
                                Valor v,
                                int linea, int columna);

/*
 * Helpers de bignum expuestos para los inline caches de F10
 * (OP_SUMAR_INT_INT, etc.). Antes eran static en evaluador.c — los
 * exponemos sin cambiar semántica para que la VM bytecode pueda
 * construir resultados int sin pasar por el dispatch general.
 */
mp_int *evaluador_nuevo_mp(void);
void evaluador_liberar_mp(mp_int *m);

/*
 * Camino rápido SMALL+SMALL para aritmética entera (B9 v0.11).
 *
 * `op_token` es un TipoToken (int para no incluir lexer.h aquí).
 * Operaciones soportadas: TT_MAS, TT_MENOS, TT_ASTERISCO,
 * TT_DOBLE_BARRA, TT_PORCENTAJE.
 *
 * Devuelve un Valor (SMALL o BIG según corresponda) si la operación
 * cabe inline. Si overflow o op no soportada, *aplicable=false y el
 * llamador debe ir al path BIG con mp_int.
 */
Valor evaluador_small_op_small(EvalError *err, int op_token,
                                int64_t a, int64_t b,
                                int linea, int columna,
                                bool *aplicable);

/*
 * Conveniencia: ejecuta secuencialmente un programa (array de
 * sentencias del parser). Para en el primer error o si una sentencia
 * deja control de flujo no normal (lo que normalmente sería un bug
 * en programas top-level — `romper` fuera de bucle, etc.).
 */
void evaluador_ejecutar_programa(Evaluador *ev, Sent **sentencias, int n);

#endif /* CORNAMUSA_EVALUADOR_H */
