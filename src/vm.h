#ifndef CORNAMUSA_VM_H
#define CORNAMUSA_VM_H

#include <stdbool.h>

#include "chunk.h"
#include "evaluador.h"   /* EvalError */
#include "memoria.h"     /* Memoria (Fase 7 S1) */
#include "valor.h"

/*
 * Máquina virtual stack-based de Cornamusa (Fase 6 sesión 2).
 *
 * Diseño: sigue el modelo clox cap. 15. Una pila de Valores crece
 * dinámicamente. Un instruction pointer (`ip`) apunta al siguiente
 * byte del chunk. Un bucle `for(;;)` despacha cada opcode mediante
 * `switch`. En esta sesión inicial implementa literales, aritmética
 * completa, comparaciones, lógica unaria, identidad/membership,
 * stack management y retorno. Variables globales/locales, control
 * de flujo y llamadas llegan en sesiones siguientes.
 *
 * Reusa la lógica de operadores del evaluador tree-walking
 * (`evaluador_aplicar_binario` y `evaluador_aplicar_unario`) para no
 * duplicar la aritmética bignum y de cadenas. El acoplamiento es
 * unidireccional: la VM depende del evaluador para operadores, el
 * evaluador NO depende de la VM.
 */

typedef enum {
    VM_OK,
    VM_ERROR_RUNTIME,
} ResultadoVM;

#define VM_PILA_MAX 8192
#define VM_FRAMES_MAX 256
#define VM_HANDLERS_MAX 64

/*
 * CallFrame: representa una llamada activa.
 *   - `chunk`: el chunk del que se está ejecutando (top-level del
 *     programa, o el chunk de una función).
 *   - `ip`: puntero al byte siguiente a ejecutar dentro de ese chunk.
 *   - `base_pila`: primer slot del frame en la pila global. Slot 0
 *     contiene el Valor callable (o nulo para el frame top-level);
 *     slots 1..aridad contienen los parámetros; slots posteriores se
 *     usan para variables locales de la función.
 */
typedef struct CallFrame {
    const Chunk *chunk;
    const uint8_t *ip;
    Valor *base_pila;
    /* `closure`: el Closure activo en este frame; NULL en el frame
       top-level (que ejecuta un chunk principal sin closure asociado).
       Usado por OP_GET_UPVALUE / OP_SET_UPVALUE para encontrar los
       upvalues de la función actual. */
    Closure *closure;
    /* `es_constructor`: true si este frame fue creado por una llamada
       `Foo(args)` que invoca `__iniciar__`. Al hacer OP_RETORNAR, la VM
       descarta el valor de retorno y empuja la instancia (slot 1)
       como resultado de la llamada original a la clase. Coincide con
       Python: `Foo()` siempre devuelve la instancia, no lo que el
       constructor retorne (que debe ser nulo/no especificado). */
    bool es_constructor;
} CallFrame;

/*
 * Handler frame: punto al que saltar cuando una excepción se lanza.
 * `frame_idx` y `tope_offset` permiten unwind del stack/frames hasta
 * el estado del `intentar` en cuestión. `ip_handler` apunta al primer
 * byte del bloque `atrapar` correspondiente.
 */
typedef struct HandlerFrame {
    int frame_idx;          /* n_frames cuando se entró al intentar */
    int tope_offset;        /* tope - pila cuando se entró */
    int n_open_upvalues;    /* longitud de open_upvalues al entrar (no usado todavía) */
    const uint8_t *ip_handler;
} HandlerFrame;

typedef struct {
    /*
     * Pila de valores compartida por todos los frames. Cada slot es
     * DUEÑO de su Valor. Capacidad ampliada respecto a S2 (256 →
     * 1024) para acomodar varias llamadas anidadas. Sigue siendo
     * fija; en F7+ se podrá hacer dinámica.
     */
    Valor pila[VM_PILA_MAX];
    Valor *tope;            /* apunta al primer slot LIBRE */

    /*
     * Stack de call frames. El frame[0] es el del chunk top-level
     * (el cuerpo del programa o de la línea del REPL); los demás se
     * crean al ejecutar OP_LLAMAR y se quitan al ejecutar OP_RETORNAR.
     */
    CallFrame frames[VM_FRAMES_MAX];
    int n_frames;

    /*
     * Variables globales: Diccionario de cadena → Valor. La VM es
     * dueña — el `dicc_liberar` se llama en `vm_destruir`. Persiste
     * entre llamadas a `vm_ejecutar` para que el REPL pueda reutilizar
     * el estado.
     */
    Diccionario *globales;

    /*
     * Linked list de upvalues abiertos (que apuntan a slots del stack
     * de algún frame activo). Ordenada por dirección de la posición
     * descendente (estilo clox cap. 25): los más arriba en la pila van
     * primero. Cuando un frame retorna, cerramos los upvalues que
     * apunten al rango del frame que termina.
     */
    Upvalue *open_upvalues;

    /*
     * Pila de handlers de excepción activos. Cada `intentar:` empuja
     * un handler con `OP_INTENTAR_INICIAR`; al salir limpio, el
     * `OP_INTENTAR_FIN` lo descarta. Si una excepción se lanza, la VM
     * busca el handler en el tope, restaura tope/frames a su nivel y
     * salta al `ip_handler`.
     */
    HandlerFrame handlers[VM_HANDLERS_MAX];
    int n_handlers;

    /*
     * Recolector de basura (Fase 7). En S1 solo rastrea objetos
     * heap-alocados; el refcount sigue siendo el liberador efectivo.
     * Sesiones siguientes activan mark+sweep y eliminan refcount.
     */
    Memoria memoria;

    EvalError error;
} VM;

/* Inicializa la VM. La pila empieza vacía, sin error. */
void vm_iniciar(VM *vm);

/*
 * Libera los Valores que queden en la pila si la ejecución fue
 * interrumpida (en éxito normal, el OP_RETORNAR ya las consume).
 */
void vm_destruir(VM *vm);

/*
 * Ejecuta el chunk dado hasta encontrar OP_RETORNAR o un error.
 *
 * Si todo va bien y `resultado_out` no es NULL, recibe el último
 * Valor que la VM dejó en la pila antes del retorno. El cliente
 * toma posesión y debe destruirlo.
 *
 * En caso de error rellena `vm->error` con línea/columna/mensaje
 * extraídos del chunk y devuelve VM_ERROR_RUNTIME.
 */
ResultadoVM vm_ejecutar(VM *vm, const Chunk *chunk, Valor *resultado_out);

/*
 * GC mark phase (Fase 7 sesión 3): marca todas las raíces de la VM
 * (pila, globales, frames, open_upvalues) propagando la marca a todo
 * lo alcanzable. Usable como primitiva del recolector completo en S4.
 */
void gc_marcar_raices(VM *vm);

#endif /* CORNAMUSA_VM_H */
