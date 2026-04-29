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

int main(void) {
    test_alocar_sin_memoria();
    test_alocar_con_memoria();
    test_memoria_destruir_libera_residuos();
    test_lista_sin_memoria();
    test_lista_con_memoria();
    test_vm_instala_memoria();

    if (fallos == 0) {
        printf("OK: todos los tests del GC (S1) pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
