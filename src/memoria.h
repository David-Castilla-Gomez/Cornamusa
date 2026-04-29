#ifndef CORNAMUSA_MEMORIA_H
#define CORNAMUSA_MEMORIA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Infraestructura para el GC mark-sweep tri-color (Fase 7 sesión 1).
 *
 * Estado v0.8.0:
 *   - **Sesión 1 (esta)**: solo infraestructura. Cada objeto heap-alocado
 *     que el GC va a poder gestionar incluye un `GCObject` como primer
 *     campo (patrón de "herencia" en C). El allocator central
 *     `gc_alocar` aloca con malloc y enlaza el objeto a una linked list
 *     de "todos los objetos vivos" para que la siguiente sesión pueda
 *     barrerlos. **El GC todavía no recoge** — el refcount sigue siendo
 *     el mecanismo de liberación efectivo.
 *   - **Sesión 2**: migrar el resto de tipos heap a este patrón.
 *   - **Sesión 3**: mark phase (recorrer raíces y propagar marcas).
 *   - **Sesión 4**: sweep phase (liberar no marcados) + triggers
 *     automáticos + flag `--gc-stress`.
 *
 * Modelo "Memoria global": el VM (único productor de objetos durante
 * la ejecución de código de usuario) instala su `Memoria` como global
 * en `vm_iniciar` y la des-instala en `vm_destruir`. El allocator
 * `gc_alocar` consulta este global; si es NULL (e.g. tests low-level
 * que construyen Lista directamente sin VM), cae a `malloc` puro sin
 * rastreo. Esto preserva la compatibilidad con tests y código legacy
 * mientras la VM gana rastreo para la futura recolección.
 *
 * Cornamusa es single-thread por diseño (decisión I3 aplazada a F6+).
 * Si en el futuro se introduce concurrencia, este global pasa a
 * thread-local sin cambios visibles en la API.
 */

/*
 * Tag de tipo para que el sweep llame al destructor correcto. Cada
 * tipo heap-alocado del runtime tiene su propio valor en este enum.
 * El orden no es estable a través de versiones del bytecode.
 */
typedef enum {
    GC_TIPO_LISTA = 1,
    GC_TIPO_DICCIONARIO,
    GC_TIPO_CONJUNTO,
    GC_TIPO_TUPLA,
    GC_TIPO_FUNCION_BC,
    GC_TIPO_CLOSURE,
    GC_TIPO_UPVALUE,
    GC_TIPO_ITERADOR,
    GC_TIPO_EXCEPCION,
    GC_TIPO_CLASE,
    GC_TIPO_INSTANCIA,
    GC_TIPO_METODO_LIGADO,
} TipoGC;

/*
 * Header común que cada objeto heap-alocado y rastreable debe incluir
 * como PRIMER campo. Esto permite tratar `Lista *` como `GCObject *`
 * sin cast.
 */
typedef struct GCObject {
    struct GCObject *siguiente;   /* linked list de todos los objetos vivos */
    bool marcado;                 /* usado por mark phase (S3) */
    uint8_t tipo;                 /* TipoGC */
} GCObject;

/*
 * Estado del recolector. Una instancia vive en la VM; el VM la
 * instala como global en `vm_iniciar`.
 */
typedef struct Memoria {
    GCObject *cabeza;             /* head de la linked list de objetos vivos */
    size_t total_alocado;         /* bytes totales alocados (estadística) */
    size_t total_objetos;         /* objetos vivos rastreados */
    size_t umbral_gc;             /* bytes alocados que disparan el siguiente GC */
    bool gc_stress;               /* si true, GC en cada allocation (debug) */
} Memoria;

/* Inicializa la memoria con valores por defecto. No instala globalmente. */
void memoria_iniciar(Memoria *m);

/*
 * Libera TODOS los objetos rastreados. Útil al destruir la VM —
 * cualquier objeto que quedó vivo (por ciclo refcount o por
 * error) se libera aquí. Llama a `gc_destruir_objeto` por cada uno.
 */
void memoria_destruir(Memoria *m);

/*
 * Instala/des-instala la `Memoria` actual del proceso. La VM las llama
 * en `vm_iniciar` / `vm_destruir`. Un valor NULL significa "ningún
 * GC activo" — `gc_alocar` cae a malloc directo en ese caso.
 */
void gc_instalar(Memoria *m);
void gc_desinstalar(void);

/*
 * Devuelve la `Memoria` actualmente instalada (o NULL si no hay).
 * Solo útil para tests/diagnóstico.
 */
Memoria *gc_actual(void);

/*
 * Aloca `size` bytes para un objeto heap-alocado del tipo indicado.
 * Si hay una Memoria instalada:
 *   - Aloca con malloc.
 *   - Inicializa el GCObject embebido al inicio (siguiente, marcado=false, tipo).
 *   - Inserta el objeto en la linked list `cabeza`.
 *   - Acumula estadísticas.
 * Si NO hay Memoria instalada (caso tests low-level):
 *   - Aloca con malloc.
 *   - Inicializa el GCObject embebido (siguiente=NULL, marcado=false, tipo)
 *     para que el resto del código no lea basura, pero NO se rastrea.
 *
 * Devuelve el puntero al inicio de la región alocada (que coincide
 * con el inicio del GCObject) o NULL si malloc falla.
 *
 * `size` debe incluir el espacio del `GCObject` y de los campos
 * propios del tipo (típicamente `sizeof(MiTipo)` con `GCObject obj` como
 * primer campo).
 */
void *gc_alocar(size_t size, TipoGC tipo);

/*
 * Notifica al GC que un objeto ha sido liberado fuera del flujo
 * normal del sweep (e.g. refcount->0 en S1 antes de que el sweep
 * exista). Lo desenlaza de la linked list y resta de las estadísticas.
 *
 * Solo necesario mientras refcount + GC coexisten (S1-S3). En S4 con
 * sweep activo, los objetos solo se liberan via sweep.
 */
void gc_desenlazar(GCObject *obj);

/*
 * ──────────────────────────────────────────────────────────────────
 * Mark phase (Fase 7 sesión 3).
 *
 * Tri-color simplificado: usa solo dos colores (white/black) en S3
 * mediante recursión profunda. Estructuras anidadas profundas pueden
 * agotar el stack del proceso — aceptable para programas Cornamusa
 * típicos. Una versión iterativa con worklist gris explícita llegará
 * post-v0.8.0 si surge el problema.
 *
 * Estas funciones son primitivas: la VM las invoca desde
 * `gc_marcar_raices` (en vm.c) para marcar todo lo alcanzable desde
 * sus raíces (stack, globales, frames, open_upvalues, handlers, etc.).
 * ──────────────────────────────────────────────────────────────────
 */

/* Forward decl: valor.h incluye memoria.h, así que aquí no podemos
   incluirlo. La implementación en memoria.c sí incluye valor.h. */
struct Valor;

/*
 * Marca un Valor: si su tipo apunta a un objeto heap-rastreado, marca
 * recursivamente. Tipos planos (entero, decimal, booleano, cadena
 * referencia, función AST) se ignoran.
 *
 * Idempotente: re-marcar un objeto ya marcado es no-op (corta ciclos).
 */
void gc_marcar_valor(const struct Valor *v);

/*
 * Marca un objeto heap directamente. Si ya estaba marcado, no-op.
 * Si no, lo marca y propaga la marca a sus hijos según el `tipo` del
 * GCObject.
 */
void gc_marcar_objeto(GCObject *obj);

/*
 * Desmarca TODOS los objetos rastreados — preparación para el
 * siguiente ciclo de marcado o cuando solo se necesita inspeccionar
 * el estado. O(N) en objetos vivos.
 */
void gc_desmarcar_todos(Memoria *m);

/* Cuenta objetos marcados (solo para tests/diagnóstico). */
size_t gc_contar_marcados(const Memoria *m);

#endif /* CORNAMUSA_MEMORIA_H */
