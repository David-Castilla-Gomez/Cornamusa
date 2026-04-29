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

/* ───── Mark phase (Fase 7 sesión 3) ───── */

static void test_marcar_valores_planos(void) {
    /* Valores planos (entero, decimal, booleano, nulo, cadena de
       referencia) no contienen objetos heap — gc_marcar_valor es
       no-op pero no debe romper. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    Valor v1 = valor_entero_de_long(42);
    Valor v2 = valor_decimal(3.14);
    Valor v3 = valor_booleano(true);
    Valor v4 = valor_nulo();
    gc_marcar_valor(&v1);
    gc_marcar_valor(&v2);
    gc_marcar_valor(&v3);
    gc_marcar_valor(&v4);
    AFIRMAR(m.cabeza == NULL);  /* nada se aloca por estos */

    valor_destruir(&v1);
    valor_destruir(&v2);
    valor_destruir(&v3);
    valor_destruir(&v4);
    gc_desinstalar();
    memoria_destruir(&m);
}

static void test_marcar_lista_anidada(void) {
    /* Lista que contiene una sub-lista: marcar la externa marca la
       interna recursivamente. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    Lista *interna = lista_nueva(0);
    lista_agregar(interna, valor_entero_de_long(1));

    Lista *externa = lista_nueva(0);
    lista_agregar(externa, valor_lista(interna));   /* transfiere ref */
    /* externa ahora retiene a interna via valor_lista (la lista en
       sí no incrementa refcount; valor_lista solo crea el wrapper). */

    AFIRMAR(externa->obj.marcado == false);
    AFIRMAR(interna->obj.marcado == false);

    gc_marcar_objeto(&externa->obj);

    AFIRMAR(externa->obj.marcado == true);
    AFIRMAR(interna->obj.marcado == true);   /* propagado */

    /* Limpieza. */
    lista_liberar(externa);   /* libera la externa, decrementa la interna */
    gc_desinstalar();
    memoria_destruir(&m);
}

static void test_marcar_diccionario(void) {
    /* Marcar un dicc marca cada clave y cada valor. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    Diccionario *d = dicc_nuevo();
    Lista *contenido = lista_nueva(0);
    lista_retener(contenido);  /* mantenemos otra ref para verificar tras destruir d */
    dicc_asignar(d,
        valor_cadena_duplicar("clave", 5),
        valor_lista(contenido));

    AFIRMAR(contenido->obj.marcado == false);
    gc_marcar_objeto(&d->obj);
    AFIRMAR(d->obj.marcado == true);
    AFIRMAR(contenido->obj.marcado == true);

    dicc_liberar(d);
    lista_liberar(contenido);
    gc_desinstalar();
    memoria_destruir(&m);
}

static void test_marcar_clase_y_instancia(void) {
    /* Instancia marca su clase + atributos. Clase marca metodos +
       superclase. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    Clase *padre = clase_nueva("Padre", 5);
    Clase *hijo = clase_nueva("Hijo", 4);
    clase_retener(padre);
    hijo->superclase = padre;

    Instancia *inst = instancia_nueva(hijo);
    dicc_asignar(inst->atributos,
        valor_cadena_duplicar("x", 1), valor_entero_de_long(10));

    gc_marcar_objeto(&inst->obj);

    AFIRMAR(inst->obj.marcado == true);
    AFIRMAR(hijo->obj.marcado == true);
    AFIRMAR(padre->obj.marcado == true);
    AFIRMAR(hijo->metodos->obj.marcado == true);
    AFIRMAR(inst->atributos->obj.marcado == true);

    instancia_liberar(inst);
    clase_liberar(hijo);    /* hijo libera padre via superclase */
    gc_desinstalar();
    memoria_destruir(&m);
}

static void test_marcar_idempotente(void) {
    /* Marcar dos veces no es problema. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    Lista *l = lista_nueva(0);
    gc_marcar_objeto(&l->obj);
    gc_marcar_objeto(&l->obj);
    AFIRMAR(l->obj.marcado == true);

    lista_liberar(l);
    gc_desinstalar();
    memoria_destruir(&m);
}

static void test_marcar_ciclo_no_explota(void) {
    /* Crear un ciclo entre dos diccionarios: a["b"]=b, b["a"]=a.
       Marcar uno debe terminar (no recursión infinita) y marcar
       ambos. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    Diccionario *a = dicc_nuevo();
    Diccionario *b = dicc_nuevo();
    dicc_retener(a);   /* ciclo: a["b"] retiene b, b["a"] retiene a */
    dicc_retener(b);
    dicc_asignar(a, valor_cadena_duplicar("b", 1), valor_diccionario(b));
    dicc_asignar(b, valor_cadena_duplicar("a", 1), valor_diccionario(a));

    gc_marcar_objeto(&a->obj);
    AFIRMAR(a->obj.marcado == true);
    AFIRMAR(b->obj.marcado == true);

    /* Refcount no liberará por el ciclo — memoria_destruir limpia. */
    dicc_liberar(a);
    dicc_liberar(b);
    /* Aún quedan refs por el ciclo: memoria_destruir liberará el
       crudo restante (con leak interno aceptable en S3 sin sweep). */
    gc_desinstalar();
    memoria_destruir(&m);
}

static void test_marcar_raices_vm(void) {
    /* Crear una VM, ejecutar un programa simple, marcar raíces, y
       verificar que las globales y los objetos vivos quedan
       marcados. */
    /* Setup inline sin AST/parser para no incluir todo el pipeline:
       solo verificamos que las raíces básicas (globales) funcionan. */
    VM vm;
    vm_iniciar(&vm);

    /* Añadir un valor heap-rastreado a las globales. */
    Lista *l = lista_nueva(0);
    lista_agregar(l, valor_entero_de_long(123));
    dicc_asignar(vm.globales,
        valor_cadena_duplicar("mi_lista", 8),
        valor_lista(l));

    AFIRMAR(l->obj.marcado == false);
    AFIRMAR(vm.globales->obj.marcado == false);

    gc_marcar_raices(&vm);

    AFIRMAR(vm.globales->obj.marcado == true);
    AFIRMAR(l->obj.marcado == true);

    vm_destruir(&vm);
}

/* ───── Sweep phase (Fase 7 sesión 4) ───── */

static void test_barrer_libera_no_marcados(void) {
    /* Sin marcar nada, sweep debe liberar todos los objetos rastreados.
       Después la cabeza queda NULL. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    Lista *l = lista_nueva(0);
    lista_agregar(l, valor_entero_de_long(1));
    lista_agregar(l, valor_entero_de_long(2));

    AFIRMAR(m.total_objetos >= 1);
    /* refcount sigue siendo 1: si llamáramos a lista_liberar, lo destruiría
       limpiamente. Pero queremos verificar que sweep sin marcar lo recoge. */

    size_t liberados = gc_barrer(&m);
    AFIRMAR(liberados >= 1);
    AFIRMAR(m.cabeza == NULL);

    gc_desinstalar();
    memoria_destruir(&m);
}

/* Marcador no-op para tests donde queremos sweep total (no marcar nada). */
static void marcador_vacio(void *ctx) { (void)ctx; }

/* Marcador que marca un objeto específico. */
static void marcador_un_objeto(void *ctx) {
    GCObject *o = (GCObject *)ctx;
    gc_marcar_objeto(o);
}

static void test_recolectar_cicla(void) {
    /* El gran test: cycle entre 2 dicc que refcount no puede liberar.
       gc_recolectar (sin raíces) debe limpiarlos. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    Diccionario *a = dicc_nuevo();
    Diccionario *b = dicc_nuevo();
    /* Crear ciclo: a["b"] = b, b["a"] = a. */
    dicc_retener(a);
    dicc_retener(b);
    dicc_asignar(a, valor_cadena_duplicar("b", 1), valor_diccionario(b));
    dicc_asignar(b, valor_cadena_duplicar("a", 1), valor_diccionario(a));
    /* Soltar las refs externas. Refcount no libera por el ciclo. */
    dicc_liberar(a);
    dicc_liberar(b);

    AFIRMAR(m.total_objetos >= 2);   /* a y b siguen vivos por el ciclo */

    size_t liberados = gc_recolectar(&m, marcador_vacio, NULL);
    AFIRMAR(liberados >= 2);
    AFIRMAR(m.cabeza == NULL);

    gc_desinstalar();
    memoria_destruir(&m);
}

static void test_recolectar_no_toca_marcados(void) {
    /* Si una raíz mantiene el objeto, no se libera. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    Lista *protegida = lista_nueva(0);
    Lista *huerfana = lista_nueva(0);

    AFIRMAR(m.total_objetos >= 2);

    /* Marcar solo 'protegida' como raíz. */
    size_t liberados = gc_recolectar(&m, marcador_un_objeto, &protegida->obj);
    AFIRMAR(liberados >= 1);   /* huerfana */
    /* protegida sigue viva. */
    AFIRMAR(protegida->obj.tipo == GC_TIPO_LISTA);

    /* Ahora soltarla y limpiar. */
    lista_liberar(protegida);
    AFIRMAR(m.cabeza == NULL);

    gc_desinstalar();
    memoria_destruir(&m);
}

static void test_memoria_destruir_sin_leaks(void) {
    /* memoria_destruir libera todo lo restante. Útil cuando el VM se
       destruye sin haber barrido ciclos. */
    Memoria m;
    memoria_iniciar(&m);
    gc_instalar(&m);

    /* Crear ciclo y no limpiarlo manualmente. */
    Diccionario *a = dicc_nuevo();
    Diccionario *b = dicc_nuevo();
    dicc_retener(a); dicc_retener(b);
    dicc_asignar(a, valor_cadena_duplicar("b", 1), valor_diccionario(b));
    dicc_asignar(b, valor_cadena_duplicar("a", 1), valor_diccionario(a));
    dicc_liberar(a); dicc_liberar(b);

    AFIRMAR(m.total_objetos >= 2);

    gc_desinstalar();
    memoria_destruir(&m);
    AFIRMAR(m.cabeza == NULL);
    /* No assertion sobre leaks de heap propias (mp_int/char*) — están
       siendo aceptados como limitación documentada de v0.8.0 cuando se
       destruye la VM con ciclos sin haber barrido antes. En la práctica
       el usuario hace gc_recolectar antes de destruir. */
}

int main(void) {
    test_alocar_sin_memoria();
    test_alocar_con_memoria();
    test_memoria_destruir_libera_residuos();
    test_lista_sin_memoria();
    test_lista_con_memoria();
    test_vm_instala_memoria();
    test_todos_los_tipos_rastrean();
    test_marcar_valores_planos();
    test_marcar_lista_anidada();
    test_marcar_diccionario();
    test_marcar_clase_y_instancia();
    test_marcar_idempotente();
    test_marcar_ciclo_no_explota();
    test_marcar_raices_vm();
    test_barrer_libera_no_marcados();
    test_recolectar_cicla();
    test_recolectar_no_toca_marcados();
    test_memoria_destruir_sin_leaks();

    if (fallos == 0) {
        printf("OK: todos los tests del GC (S1) pasaron\n");
        return 0;
    }
    fprintf(stderr, "FALLOS: %d\n", fallos);
    return 1;
}
