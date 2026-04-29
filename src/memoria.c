#include "memoria.h"

#include <stdlib.h>

/*
 * Memoria global del proceso. La VM la instala/des-instala. Si NULL,
 * `gc_alocar` se comporta como malloc puro (no rastrea).
 *
 * Single-thread por ahora (decisión I3 aplazada). Si llega
 * concurrencia, esto se vuelve thread-local sin tocar la API.
 */
static Memoria *g_memoria_actual = NULL;

#define UMBRAL_GC_INICIAL (1024 * 1024)   /* 1 MiB antes del primer ciclo GC */

void memoria_iniciar(Memoria *m) {
    if (!m) return;
    m->cabeza = NULL;
    m->total_alocado = 0;
    m->total_objetos = 0;
    m->umbral_gc = UMBRAL_GC_INICIAL;
    m->gc_stress = false;
}

void memoria_destruir(Memoria *m) {
    if (!m) return;
    /*
     * En S1 con refcount activo, normalmente todos los objetos
     * rastreados ya fueron liberados via refcount cuando llegamos aquí
     * (el VM los descartó vía `valor_destruir` durante `vm_destruir`).
     * Cualquier objeto que sobreviva indica un ciclo refcount o un
     * leak — los liberamos aquí para evitar el reporte de leak en
     * herramientas como valgrind/ASan.
     *
     * Como en S1 los destructores específicos de cada tipo todavía
     * están en sus respectivos archivos (lista_liberar, dicc_liberar,
     * etc.), aquí simplemente liberamos la memoria cruda sin invocar
     * destructores. Eso significa que ESTA ruta sí filtra recursos
     * internos (mp_int, char*, sub-Listas no liberadas...) — pero solo
     * se ejecuta si el refcount falló, que indica un bug. En S4 cuando
     * el sweep esté completo, esta función llamará al destructor
     * correcto vía el tag.
     */
    GCObject *o = m->cabeza;
    while (o) {
        GCObject *next = o->siguiente;
        free(o);
        o = next;
    }
    m->cabeza = NULL;
    m->total_alocado = 0;
    m->total_objetos = 0;
}

void gc_instalar(Memoria *m) {
    g_memoria_actual = m;
}

void gc_desinstalar(void) {
    g_memoria_actual = NULL;
}

Memoria *gc_actual(void) {
    return g_memoria_actual;
}

void *gc_alocar(size_t size, TipoGC tipo) {
    void *p = malloc(size);
    if (!p) return NULL;
    GCObject *obj = (GCObject *)p;
    obj->siguiente = NULL;
    obj->marcado = false;
    obj->tipo = (uint8_t)tipo;

    Memoria *m = g_memoria_actual;
    if (m) {
        obj->siguiente = m->cabeza;
        m->cabeza = obj;
        m->total_alocado += size;
        m->total_objetos += 1;
    }
    return p;
}

void gc_desenlazar(GCObject *obj) {
    if (!obj) return;
    Memoria *m = g_memoria_actual;
    if (!m) return;   /* no rastreado: nada que desenlazar */

    /*
     * Linked list singly-linked sin tail-pointer. El desenlace lineal
     * es O(N) en el peor caso. En S1 esto se ejecuta cuando un
     * refcount llega a 0; en S4 con sweep activo se elimina y la
     * lista se reconstruye en cada ciclo GC.
     */
    GCObject **prev_ptr = &m->cabeza;
    while (*prev_ptr) {
        if (*prev_ptr == obj) {
            *prev_ptr = obj->siguiente;
            if (m->total_objetos > 0) m->total_objetos -= 1;
            return;
        }
        prev_ptr = &(*prev_ptr)->siguiente;
    }
    /*
     * Si llegamos aquí, el objeto no está en la lista. Esto puede
     * ocurrir legítimamente si fue creado sin Memoria instalada
     * (test directo) y luego se intenta desenlazar bajo una Memoria
     * instalada. No es error.
     */
}
