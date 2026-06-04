#ifndef CORNAMUSA_VM_H
#define CORNAMUSA_VM_H

#include <stdbool.h>

#include "chunk.h"
#include "coverage.h"    /* Coverage tracker (v1.75) */
#include "depurador.h"   /* Debugger interactivo (v1.76) */
#include "evaluador.h"   /* EvalError */
#include "memoria.h"     /* Memoria (Fase 7 S1) */
#include "profiler.h"    /* Profiler determinista (v1.71) */
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
    /* v1.31: solo retornados desde dispatch en modo generador. */
    VM_OK_YIELD,         /* OP_PRODUCIR; valor en vm->valor_yield_pendiente */
    VM_OK_GEN_AGOTADO,   /* OP_RETORNAR del frame del generador */
    /* v1.42: solo retornado desde dispatch en modo sub-call síncrono.
       OP_RETORNAR del frame del dunder despachado por hash_valor /
       valor_iguales. El valor de retorno queda en el TOS del caller. */
    VM_OK_SUB_RETURN,
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

    /*
     * Módulos (Fase 9): si este frame fue creado por OP_IMPORTAR, este
     * campo apunta al `Modulo` en construcción. Al hacer OP_RETORNAR, la
     * VM captura `vm->globales` (el dicc del módulo) en
     * `modulo_en_carga->atributos`, restaura el dicc anterior desde
     * `globales_pre_modulo`, y registra el Modulo como global del
     * importador. NULL si el frame no es un import.
     */
    Modulo *modulo_en_carga;
    Diccionario *globales_pre_modulo;
    /* Chunk creado para el módulo — el frame es dueño y debe destruirlo
       (no lo gestiona el GC porque Chunk no es heap-rastreado). */
    Chunk *chunk_modulo;
    /*
     * Nombre con el que el módulo será registrado como global del
     * importador (v0.9.1). Para `importar X` es igual al nombre del
     * módulo. Para `importar X como Y` es `Y`. Heap-duplicated; el
     * frame es dueño y se libera al finalizar el módulo en OP_RETORNAR.
     * NULL si no es un frame de importación.
     */
    char *modulo_binding_name;
    int modulo_binding_len;
    /*
     * v0.9.1: si true, este frame es de OP_IMPORTAR_PARA_DESDE — al
     * finalizar (OP_RETORNAR detecta modulo_en_carga), NO se registra
     * el módulo como global; se empuja al tope del stack como "valor
     * de retorno" del importar para que el código siguiente lea
     * atributos via OP_OBTENER_ATRIBUTO.
     */
    bool desde_import;
    /*
     * Si la closure que disparó este frame tenía `globales_definicion`
     * != vm->globales, guardamos el dicc anterior aquí y lo restauramos
     * al hacer OP_RETORNAR. Permite que funciones de un módulo vean
     * sus propias globales aunque sean invocadas desde el importador.
     * NULL si la llamada no cambió globales (caso típico).
     */
    Diccionario *globales_pre_llamada;
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
     *
     * Durante la carga de un módulo (Fase 9), apunta temporalmente al
     * Diccionario del módulo. El frame del módulo guarda el dicc
     * principal en `globales_pre_modulo` y lo restaura al retornar.
     */
    Diccionario *globales;

    /*
     * Cache de módulos cargados (Fase 9): nombre → VAL_MODULO. El primer
     * `importar X` carga y cachea; subsiguientes solo asignan la global.
     */
    Diccionario *cache_modulos;

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

    /* v1.31: cooperación con generadores. Cuando `modo_yield` es true,
       OP_PRODUCIR pop su valor a `valor_yield_pendiente` y retorna
       VM_OK_YIELD. OP_RETORNAR retorna VM_OK_GEN_AGOTADO si tras pop
       el n_frames cae por debajo de `frame_techo`. */
    int frame_techo;
    bool modo_yield;
    Valor valor_yield_pendiente;

    /* v1.42: cooperación con sub-call síncrono. Cuando `modo_sub_call`
       es true, OP_RETORNAR del frame que cae por debajo de
       `frame_techo` retorna VM_OK_SUB_RETURN preservando el valor de
       retorno en el TOS del caller. Usado por `hash_valor` y
       `valor_iguales` para despachar `__hash__` / `__igual__` en
       instancias sin abandonar la operación principal.

       `handler_techo` limita la búsqueda de OP_LANZAR a los handlers
       instalados DESDE que comenzó el sub-call. Sin esto, una
       excepción dentro del dunder se desenroscaría más allá del
       sub-VM y dejaría el C-stack inconsistente. */
    bool modo_sub_call;
    int handler_techo;

    /* v1.38: traceback de la cadena de llamadas, capturado cuando un
       error de runtime fatal sale del dispatch. Vacío ("") si no hay
       error o el error es solo de top-level. */
    char traceback[1024];

    /* v1.71: profiler determinista. Inactivo por defecto; el subcomando
       `cornamusa prof` lo activa antes de ejecutar. Cuando inactivo, los
       hooks son no-op (un branch). */
    Profiler profiler;

    /* v1.75: coverage tracker. Inactivo por defecto; el subcomando
       `cornamusa cov` lo activa antes de ejecutar. Hook al inicio del
       dispatch (mismo punto que profiler). */
    CovTracker cov;

    /* v1.76: debugger interactivo. Inactivo por defecto; subcomando
       `cornamusa depurar` lo activa. */
    Depurador dep;
} VM;

/* Inicializa la VM. La pila empieza vacía, sin error. */
void vm_iniciar(VM *vm);

/* v1.122: registra el path absoluto (o relativo) del binario para que
 * `cargar_modulo_desde_archivo` pueda buscar stdlib relativa al
 * ejecutable y permitir `cornamusa X.cor` desde cualquier cwd. Llamar
 * desde main() con argv[0]. Idempotente; reaccepta NULL/""  para
 * limpiar. Independiente del estado de una VM concreta. */
void vm_set_ruta_binario(const char *path);

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

/* v1.31: reanuda un generador hasta el próximo OP_PRODUCIR/OP_RETORNAR.
   Retorna true si produjo (en *out_valor); false si terminó. */
bool vm_generador_paso(VM *vm, Generador *gen, Valor *out_valor);

/*
 * GC mark phase (Fase 7 sesión 3): marca todas las raíces de la VM
 * (pila, globales, frames, open_upvalues) propagando la marca a todo
 * lo alcanzable. Usable como primitiva del recolector completo en S4.
 */
void gc_marcar_raices(VM *vm);

#endif /* CORNAMUSA_VM_H */
