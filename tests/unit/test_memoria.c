/*
 * Tests del allocator GC (Fase 7 sesión 1).
 *
 * En esta sesión solo verificamos:
 *   - gc_alocar funciona con/sin Memoria instalada.
 *   - El GCObject embebido se inicializa correctamente.
 *   - Los objetos rastreados se enlazan a la cabeza de la linked list.
 *   - gc_desenlazar quita objetos correctamente.
 *   - memoria_destruir libera todos los rastreados.
 *   - Lista (primer tipo migrado) se rastrea bajo VM y no fuera.
 *
 * Mark/sweep llega en sesiones siguientes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memoria.h"
#include "valor.h"
#include "vm.h"
#include "chunk.h"

static int fallos = 0;

#define AFIRMAR(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FALLO en %s:%d: %s\n", __FILE__, __LINE__, #cond);\
            fallos++;                                                          \
        }                                                                      \
    } while (0)

/* Struct de prueba con GCObject como primer campo. */
typedef struct {
    GCObject obj;
    int valor;
} ObjetoTest;

static void test_alocar_sin_memoria(void) {
    /* Sin Memoria instalada, gc_alocar es malloc puro pero inicializa
       el GCObject embebido. */
    gc_desinstalar();
    AFIRMAR(gc_actual() == NULL);

    ObjetoTest *o = (ObjetoTest *)gc_alocar(sizeof(ObjetoTest), GC_TIPO_LISTA);
    AFIRMAR(o != NULL);
    AFIRMAR(o->obj.siguiente == NULL);
    AFIRMAR(o->obj.marcado == false);
    AFIRMAR(o->obj.tipo == GC_TIPO_LISTA);
    free(o);
}

static void test_alocar_con_memoria(void) {
    /* Con Memoria instalada, los objetos se enlazan a la lista. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    AFIRMAR(m.cabeza == NULL);
    AFIRMAR(m.total_objetos == 0);

    ObjetoTest *a = (ObjetoTest *)gc_alocar(sizeof(ObjetoTest), GC_TIPO_LISTA);
    AFIRMAR(m.cabeza == &a->obj);
    AFIRMAR(m.total_objetos == 1);
    AFIRMAR(m.total_alocado == sizeof(ObjetoTest));

    ObjetoTest *b = (ObjetoTest *)gc_alocar(sizeof(ObjetoTest), GC_TIPO_DICCIONARIO);
    AFIRMAR(m.cabeza == &b->obj);            /* el más reciente está al frente */
    AFIRMAR(b->obj.siguiente == &a->obj);
    AFIRMAR(m.total_objetos == 2);

    /* Desenlazar el segundo (más reciente). */
    gc_desenlazar(&b->obj);
    AFIRMAR(m.cabeza == &a->obj);
    AFIRMAR(m.total_objetos == 1);
    free(b);

    /* Desenlazar el primero. */
    gc_desenlazar(&a->obj);
    AFIRMAR(m.cabeza == NULL);
    AFIRMAR(m.total_objetos == 0);
    free(a);

    gc_desinstalar();
    memoria_destruir(&m);
}

static void test_memoria_destruir_libera_residuos(void) {
    /* Si memoria_destruir se llama con objetos vivos rastreados, los
       libera. (Útil cuando un ciclo refcount no liberó algo.) */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    /* Alocar 3 objetos sin desenlazarlos. */
    (void)gc_alocar(sizeof(ObjetoTest), GC_TIPO_LISTA);
    (void)gc_alocar(sizeof(ObjetoTest), GC_TIPO_LISTA);
    (void)gc_alocar(sizeof(ObjetoTest), GC_TIPO_LISTA);
    AFIRMAR(m.total_objetos == 3);

    gc_desinstalar();
    memoria_destruir(&m);
    AFIRMAR(m.cabeza == NULL);
    AFIRMAR(m.total_objetos == 0);
}

static void test_lista_sin_memoria(void) {
    /* Crear y liberar Lista sin Memoria instalada. Debe seguir
       funcionando con malloc puro y refcount. */
    gc_desinstalar();

    Lista *l = lista_nueva(0);
    AFIRMAR(l != NULL);
    AFIRMAR(l->refcount == 1);
    /* El header GCObject debe estar inicializado correctamente para
       que el resto del runtime no lea basura. */
    AFIRMAR(l->obj.tipo == GC_TIPO_LISTA);
    AFIRMAR(l->obj.marcado == false);

    lista_agregar(l, valor_entero_de_long(42));
    AFIRMAR(l->cuenta == 1);

    lista_liberar(l);
}

static void test_lista_con_memoria(void) {
    /* Crear y destruir una VM real, verificando que el GC rastrea
       las listas creadas durante la ejecución. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    Lista *l = lista_nueva(0);
    AFIRMAR(m.cabeza == &l->obj);
    AFIRMAR(m.total_objetos == 1);

    lista_liberar(l);
    /* Tras la liberación por refcount, la lista se desenlaza. */
    AFIRMAR(m.cabeza == NULL);
    AFIRMAR(m.total_objetos == 0);

    gc_desinstalar();
    memoria_destruir(&m);
}

static void test_vm_instala_memoria(void) {
    /* Cuando la VM se inicia, la memoria debe estar instalada. Tras
       destruirla, debe estar des-instalada. */
    AFIRMAR(gc_actual() == NULL);

    VM vm;
    vm_iniciar(&vm);
    AFIRMAR(gc_actual() == &vm.memoria);

    vm_destruir(&vm);
    AFIRMAR(gc_actual() == NULL);
}

static void test_todos_los_tipos_rastrean(void) {
    /* Sesión 2: cada tipo migrado debe registrar GCObject en la
       linked list. Verificamos creación + liberación → cabeza vuelve
       a NULL. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    /* Lista. */
    Lista *l = lista_nueva(0);
    AFIRMAR(m.cabeza == &l->obj);
    AFIRMAR(l->obj.tipo == GC_TIPO_LISTA);
    lista_liberar(l);
    AFIRMAR(m.cabeza == NULL);

    /* Diccionario. */
    Diccionario *d = dicc_nuevo();
    AFIRMAR(m.cabeza == &d->obj);
    AFIRMAR(d->obj.tipo == GC_TIPO_DICCIONARIO);
    dicc_liberar(d);
    AFIRMAR(m.cabeza == NULL);

    /* Conjunto. */
    Conjunto *cj = conj_nuevo();
    AFIRMAR(m.cabeza == &cj->obj);
    AFIRMAR(cj->obj.tipo == GC_TIPO_CONJUNTO);
    conj_liberar(cj);
    AFIRMAR(m.cabeza == NULL);

    /* Tupla. */
    Tupla *t = tupla_nueva(0);
    AFIRMAR(m.cabeza == &t->obj);
    AFIRMAR(t->obj.tipo == GC_TIPO_TUPLA);
    tupla_liberar(t);
    AFIRMAR(m.cabeza == NULL);

    /* FuncionBC. */
    FuncionBC *fn = funcion_bc_nueva("test", 4, 0);
    AFIRMAR(m.cabeza == &fn->obj);
    AFIRMAR(fn->obj.tipo == GC_TIPO_FUNCION_BC);
    funcion_bc_liberar(fn);
    AFIRMAR(m.cabeza == NULL);

    /* Closure encapsula una FuncionBC (retiene). El cliente sigue
       manteniendo su propia ref a la FuncionBC y debe liberarla. */
    FuncionBC *fn2 = funcion_bc_nueva("test", 4, 0);
    Closure *cl = closure_nuevo(fn2);
    AFIRMAR(m.cabeza == &cl->obj);
    AFIRMAR(cl->obj.tipo == GC_TIPO_CLOSURE);
    closure_liberar(cl);
    funcion_bc_liberar(fn2);
    AFIRMAR(m.cabeza == NULL);

    /* Excepcion. */
    Excepcion *ex = excepcion_nueva("Excepcion", 9, "msg", 3);
    AFIRMAR(m.cabeza == &ex->obj);
    AFIRMAR(ex->obj.tipo == GC_TIPO_EXCEPCION);
    excepcion_liberar(ex);
    AFIRMAR(m.cabeza == NULL);

    /* Clase y Instancia. clase_nueva crea Clase + Diccionario (metodos)
       internamente, así que la cabeza acaba siendo el Dicc (último
       alocado). Verificamos solo el tag de cada objeto. */
    Clase *clase = clase_nueva("Foo", 3);
    AFIRMAR(clase->obj.tipo == GC_TIPO_CLASE);
    AFIRMAR(m.total_objetos == 2);   /* Clase + Diccionario interno */
    Instancia *inst = instancia_nueva(clase);
    AFIRMAR(inst->obj.tipo == GC_TIPO_INSTANCIA);
    AFIRMAR(m.total_objetos == 4);   /* + Instancia + su Dicc atributos */
    instancia_liberar(inst);
    AFIRMAR(m.total_objetos == 2);
    clase_liberar(clase);
    AFIRMAR(m.cabeza == NULL);

    gc_desinstalar();
    memoria_destruir(&m);
}

int main(void) {
    test_alocar_sin_memoria();
    test_alocar_con_memoria();
    test_memoria_destruir_libera_residuos();
    test_lista_sin_memoria();
    test_lista_con_memoria();
    test_vm_instala_memoria();
    test_todos_los_tipos_rastrean();

    if (fallos == 0) {
        printf("OK: todos los tests del GC (S1) pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
