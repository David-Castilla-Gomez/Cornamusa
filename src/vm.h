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

#define VM_PILA_MAX 256

typedef struct {
    /*
     * Pila de valores. Cada slot es DUEÑO de su Valor — al hacer pop
     * se transfiere ownership al cliente, sin copiar. Capacidad fija
     * por ahora (256); cuando lleguen llamadas con frames se hará
     * dinámica.
     */
    Valor pila[VM_PILA_MAX];
    Valor *tope;            /* apunta al primer slot LIBRE */

    /* Chunk activo + instruction pointer. La VM no posee el chunk —
       el cliente lo crea, lo pasa, y lo destruye después. */
    const Chunk *chunk;
    const uint8_t *ip;

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
