#include "memoria.h"

#include <stdlib.h>

#include "chunk.h"      /* FuncionBC, Closure, Upvalue */
#include "valor.h"      /* Valor + todos los tipos heap */

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

/* ──────────────────────────────────────────────────────────────────
 * Mark phase (Fase 7 sesión 3)
 * ────────────────────────────────────────────────────────────────── */

void gc_marcar_valor(const Valor *v) {
    if (!v) return;
    switch (v->tipo) {
        case VAL_LISTA:
            gc_marcar_objeto(&v->como.lista->obj);
            break;
        case VAL_DICCIONARIO:
            gc_marcar_objeto(&v->como.dicc->obj);
            break;
        case VAL_CONJUNTO:
            gc_marcar_objeto(&v->como.conjunto->obj);
            break;
        case VAL_TUPLA:
            gc_marcar_objeto(&v->como.tupla->obj);
            break;
        case VAL_FUNCION_BC:
            gc_marcar_objeto(&v->como.closure->obj);
            break;
        case VAL_PLANTILLA_BC:
            gc_marcar_objeto(&v->como.plantilla->obj);
            break;
        case VAL_ITERADOR:
            gc_marcar_objeto(&v->como.iterador->obj);
            break;
        case VAL_EXCEPCION:
            gc_marcar_objeto(&v->como.excepcion->obj);
            break;
        case VAL_CLASE:
            gc_marcar_objeto(&v->como.clase->obj);
            break;
        case VAL_INSTANCIA:
            gc_marcar_objeto(&v->como.instancia->obj);
            break;
        case VAL_METODO_LIGADO:
            gc_marcar_objeto(&v->como.metodo_ligado->obj);
            break;
        default:
            /* Tipos planos: nada que marcar. */
            break;
    }
}

void gc_marcar_objeto(GCObject *obj) {
    if (!obj || obj->marcado) return;
    obj->marcado = true;

    /* Propagar la marca a los hijos según el tipo. */
    switch ((TipoGC)obj->tipo) {
        case GC_TIPO_LISTA: {
            const Lista *l = (const Lista *)obj;
            for (int i = 0; i < l->cuenta; i++) {
                gc_marcar_valor(&l->elementos[i]);
            }
            break;
        }
        case GC_TIPO_DICCIONARIO: {
            const Diccionario *d = (const Diccionario *)obj;
            for (int i = 0; i < d->capacidad; i++) {
                if (d->entradas[i].ocupada) {
                    gc_marcar_valor(&d->entradas[i].clave);
                    gc_marcar_valor(&d->entradas[i].valor);
                }
            }
            break;
        }
        case GC_TIPO_CONJUNTO: {
            const Conjunto *c = (const Conjunto *)obj;
            for (int i = 0; i < c->capacidad; i++) {
                if (c->entradas[i].ocupada) {
                    gc_marcar_valor(&c->entradas[i].elemento);
                }
            }
            break;
        }
        case GC_TIPO_TUPLA: {
            const Tupla *t = (const Tupla *)obj;
            for (int i = 0; i < t->cuenta; i++) {
                gc_marcar_valor(&t->elementos[i]);
            }
            break;
        }
        case GC_TIPO_FUNCION_BC: {
            const FuncionBC *f = (const FuncionBC *)obj;
            for (int i = 0; i < f->chunk.constantes_cuenta; i++) {
                gc_marcar_valor(&f->chunk.constantes[i]);
            }
            break;
        }
        case GC_TIPO_CLOSURE: {
            const Closure *c = (const Closure *)obj;
            if (c->plantilla) gc_marcar_objeto(&c->plantilla->obj);
            for (int i = 0; i < c->plantilla->n_upvalues; i++) {
                if (c->upvalues[i]) {
                    gc_marcar_objeto(&c->upvalues[i]->obj);
                }
            }
            break;
        }
        case GC_TIPO_UPVALUE: {
            const Upvalue *u = (const Upvalue *)obj;
            /* Si está cerrado, posicion apunta a u->cerrado y debemos
               marcar el valor cerrado. Si está abierto, posicion apunta
               al stack — el stack se marca por separado en las raíces;
               aquí no propagamos para evitar doble cuenta o seguir un
               puntero a memoria que pertenece al stack-frame de la VM. */
            if (u->posicion == &u->cerrado) {
                gc_marcar_valor(&u->cerrado);
            }
            break;
        }
        case GC_TIPO_ITERADOR: {
            const Iterador *it = (const Iterador *)obj;
            gc_marcar_valor(&it->iterable);
            break;
        }
        case GC_TIPO_EXCEPCION:
            /* Solo cadenas char* — nada heap-rastreado. */
            break;
        case GC_TIPO_CLASE: {
            const Clase *c = (const Clase *)obj;
            if (c->metodos) gc_marcar_objeto(&c->metodos->obj);
            if (c->superclase) gc_marcar_objeto(&c->superclase->obj);
            break;
        }
        case GC_TIPO_INSTANCIA: {
            const Instancia *i = (const Instancia *)obj;
            if (i->clase) gc_marcar_objeto(&i->clase->obj);
            if (i->atributos) gc_marcar_objeto(&i->atributos->obj);
            break;
        }
        case GC_TIPO_METODO_LIGADO: {
            const MetodoLigado *m = (const MetodoLigado *)obj;
            gc_marcar_valor(&m->receptor);
            if (m->metodo) gc_marcar_objeto(&m->metodo->obj);
            break;
        }
    }
}

void gc_desmarcar_todos(Memoria *m) {
    if (!m) return;
    for (GCObject *o = m->cabeza; o != NULL; o = o->siguiente) {
        o->marcado = false;
    }
}

size_t gc_contar_marcados(const Memoria *m) {
    if (!m) return 0;
    size_t n = 0;
    for (GCObject *o = m->cabeza; o != NULL; o = o->siguiente) {
        if (o->marcado) n++;
    }
    return n;
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
