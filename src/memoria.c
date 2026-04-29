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
    m->fn_marcar_raices = NULL;
    m->contexto_raices = NULL;
    m->recolectando = false;
    m->gc_habilitado = false;
}

void gc_set_marcador_raices(Memoria *m, FnMarcarRaices fn, void *contexto) {
    if (!m) return;
    m->fn_marcar_raices = fn;
    m->contexto_raices = contexto;
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
    /*
     * v0.8.0: el trigger automático del recolector queda deshabilitado.
     * El motivo es que muchas factory functions (clase_nueva, instancia_nueva,
     * iter_nuevo, etc.) anidan llamadas a `gc_alocar`: tras una primera
     * alocación, el nuevo objeto está enlazado en la lista de la
     * memoria pero todavía no es alcanzable desde ninguna raíz; si el
     * GC triggerara durante una alocación interna posterior, lo
     * barrería incorrectamente. Resolverlo limpiamente requiere
     * paréntesis pause/resume en cada factory, o un modelo de trigger
     * a nivel de opcode-boundary.
     *
     * En v0.8.0 el GC se invoca solo manualmente vía `gc_recolectar`.
     * Refcount sigue siendo el liberador primario; `gc_recolectar`
     * existe para que el usuario pueda romper ciclos cuando los
     * sospeche. La automaticidad llega en una versión posterior tras
     * añadir el modelo de pausing en factories.
     */
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

/* ──────────────────────────────────────────────────────────────────
 * Sweep phase (Fase 7 sesión 4)
 *
 * Liberar un objeto recogido por sweep es delicado: NO podemos llamar
 * a `lista_liberar`/etc. (recursos via valor_destruir → liberar de hijos
 * que pueden estar en el mismo ciclo, generando use-after-free). En su
 * lugar, liberamos solo:
 *   - partes propietarias NO heap-rastreadas (mp_int, char* dueño,
 *     buffers internos como elementos[]),
 *   - y la struct misma.
 * Las referencias a otros heap-rastreados (sub-listas, métodos de
 * clase, etc.) NO se decrementan: esos objetos están en la misma
 * lista del GC y serán barridos por su cuenta cuando el sweep llegue
 * a ellos.
 * ────────────────────────────────────────────────────────────────── */

static void liberar_partes_no_gc_valor(Valor *v) {
    if (!v) return;
    switch (v->tipo) {
        case VAL_ENTERO:
            if (v->como.entero) {
                mp_clear(v->como.entero);
                free(v->como.entero);
                v->como.entero = NULL;
            }
            break;
        case VAL_CADENA:
            if (v->dueno_cadena && v->como.cadena.texto) {
                free((char *)v->como.cadena.texto);
                v->como.cadena.texto = NULL;
                v->dueno_cadena = false;
            }
            break;
        case VAL_RANGO:
            if (v->como.rango.inicio) {
                mp_clear(v->como.rango.inicio); free(v->como.rango.inicio);
                v->como.rango.inicio = NULL;
            }
            if (v->como.rango.fin) {
                mp_clear(v->como.rango.fin); free(v->como.rango.fin);
                v->como.rango.fin = NULL;
            }
            if (v->como.rango.paso) {
                mp_clear(v->como.rango.paso); free(v->como.rango.paso);
                v->como.rango.paso = NULL;
            }
            break;
        default:
            /* Tipos planos sin alocaciones extra, o tipos heap-rastreados
               (Lista/Dicc/Conjunto/Tupla/FuncionBC/PlantillaBC/Iterador/
               Excepcion/Clase/Instancia/MetodoLigado): nada que hacer aquí —
               sweep los procesa por separado. */
            break;
    }
    v->tipo = VAL_NULO;
}

static void gc_destruir_chunk_no_recursivo(Chunk *c) {
    if (!c) return;
    free(c->codigo);
    free(c->lineas);
    if (c->constantes) {
        for (int i = 0; i < c->constantes_cuenta; i++) {
            liberar_partes_no_gc_valor(&c->constantes[i]);
        }
        free(c->constantes);
    }
    c->codigo = NULL;
    c->lineas = NULL;
    c->constantes = NULL;
    c->cuenta = 0;
    c->capacidad = 0;
    c->constantes_cuenta = 0;
    c->constantes_capacidad = 0;
}

static void gc_liberar_objeto(GCObject *o) {
    if (!o) return;
    switch ((TipoGC)o->tipo) {
        case GC_TIPO_LISTA: {
            Lista *l = (Lista *)o;
            if (l->elementos) {
                for (int i = 0; i < l->cuenta; i++) {
                    liberar_partes_no_gc_valor(&l->elementos[i]);
                }
                free(l->elementos);
            }
            free(l);
            break;
        }
        case GC_TIPO_DICCIONARIO: {
            Diccionario *d = (Diccionario *)o;
            if (d->entradas) {
                for (int i = 0; i < d->capacidad; i++) {
                    if (d->entradas[i].ocupada) {
                        liberar_partes_no_gc_valor(&d->entradas[i].clave);
                        liberar_partes_no_gc_valor(&d->entradas[i].valor);
                    }
                }
                free(d->entradas);
            }
            free(d);
            break;
        }
        case GC_TIPO_CONJUNTO: {
            Conjunto *c = (Conjunto *)o;
            if (c->entradas) {
                for (int i = 0; i < c->capacidad; i++) {
                    if (c->entradas[i].ocupada) {
                        liberar_partes_no_gc_valor(&c->entradas[i].elemento);
                    }
                }
                free(c->entradas);
            }
            free(c);
            break;
        }
        case GC_TIPO_TUPLA: {
            Tupla *t = (Tupla *)o;
            if (t->elementos) {
                for (int i = 0; i < t->cuenta; i++) {
                    liberar_partes_no_gc_valor(&t->elementos[i]);
                }
                free(t->elementos);
            }
            free(t);
            break;
        }
        case GC_TIPO_FUNCION_BC: {
            FuncionBC *f = (FuncionBC *)o;
            gc_destruir_chunk_no_recursivo(&f->chunk);
            free(f->nombre);
            free(f);
            break;
        }
        case GC_TIPO_CLOSURE: {
            Closure *c = (Closure *)o;
            free(c->upvalues);
            free(c);
            break;
        }
        case GC_TIPO_UPVALUE: {
            Upvalue *u = (Upvalue *)o;
            if (u->posicion == &u->cerrado) {
                liberar_partes_no_gc_valor(&u->cerrado);
            }
            free(u);
            break;
        }
        case GC_TIPO_ITERADOR: {
            Iterador *it = (Iterador *)o;
            liberar_partes_no_gc_valor(&it->iterable);
            free(it);
            break;
        }
        case GC_TIPO_EXCEPCION: {
            Excepcion *e = (Excepcion *)o;
            free(e->clase);
            free(e->mensaje);
            free(e);
            break;
        }
        case GC_TIPO_CLASE: {
            Clase *c = (Clase *)o;
            free(c->nombre);
            free(c);
            break;
        }
        case GC_TIPO_INSTANCIA: {
            Instancia *i = (Instancia *)o;
            free(i);
            break;
        }
        case GC_TIPO_METODO_LIGADO: {
            MetodoLigado *m = (MetodoLigado *)o;
            liberar_partes_no_gc_valor(&m->receptor);
            free(m);
            break;
        }
    }
}

size_t gc_barrer(Memoria *m) {
    if (!m) return 0;
    size_t liberados = 0;
    GCObject *prev = NULL;
    GCObject *o = m->cabeza;
    while (o != NULL) {
        if (o->marcado) {
            o->marcado = false;       /* desmarcar para el siguiente ciclo */
            prev = o;
            o = o->siguiente;
        } else {
            GCObject *recoger = o;
            o = o->siguiente;
            if (prev != NULL) prev->siguiente = o;
            else              m->cabeza = o;
            if (m->total_objetos > 0) m->total_objetos -= 1;
            gc_liberar_objeto(recoger);
            liberados++;
        }
    }
    return liberados;
}

size_t gc_recolectar(Memoria *m, FnMarcarRaices marcar_raices,
                      void *contexto) {
    if (!m) return 0;
    /* 1. Desmarcar todos (estado conocido: white). */
    for (GCObject *o = m->cabeza; o != NULL; o = o->siguiente) {
        o->marcado = false;
    }
    /* 2. Marcar raíces — el callback recorre el contexto del cliente. */
    if (marcar_raices) marcar_raices(contexto);
    /* 3. Barrer no-marcados. (Esto también desmarca los que quedaron). */
    return gc_barrer(m);
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
