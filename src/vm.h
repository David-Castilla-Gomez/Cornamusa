#ifndef CORNAMUSA_VM_H
#define CORNAMUSA_VM_H

#include <stdbool.h>

#include "chunk.h"
#include "evaluador.h"   /* EvalError */
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
} CallFrame;

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

#endif /* CORNAMUSA_VM_H */
