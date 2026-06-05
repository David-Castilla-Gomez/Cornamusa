/*
 * Tests del bug "OP_ITER_SIGUIENTE sin iterador en slot N" cerrado
 * en v1.130.
 *
 * Sintoma original (documentado en CHANGELOG v1.119): al escribir
 * `componentes` en stdlib/grafos.cor con `para vec en g.vecinos(cur)`
 * dentro de un `mientras` dentro de un `para n en g.nodos()`, la
 * segunda iteracion del exterior crasheaba con
 * "OP_ITER_SIGUIENTE sin iterador en slot 9". El workaround fue
 * reescribir TODOS los bucles internos como `mientras + indice manual`,
 * desviandose del estilo idiomatico.
 *
 * Root cause: compilar_mientras llamaba pre_reservar_locales para los
 * locales del cuerpo (cur, etc.) pero NO emitia OP_DESCARTAR al salir
 * (compilar_para SI lo hace). El stack crecia +N por iteracion del
 * exterior, y el slot calculado en compile-time para el $iter del
 * `para` interno ya no apuntaba al iter real en runtime — leia el
 * OP_NULO pre-reservado del while de una iteracion anterior.
 *
 * Fix: anadir cleanup `n_locales -= n_locales_entrada` con N
 * OP_DESCARTAR al final de compilar_mientras (igual que compilar_para).
 * Tras el fix, `stdlib/grafos.cor:componentes` volvio a la version
 * idiomatica con `para` anidados.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "chunk.h"
#include "compilador.h"
#include "lexer.h"
#include "parser.h"
#include "valor.h"
#include "vm.h"

static int fallos = 0;
static int casos = 0;

#define AFIRMAR(cond, etiqueta)                                                \
    do {                                                                        \
        casos++;                                                                \
        if (!(cond)) {                                                          \
            fprintf(stderr, "FALLO %s:%d (%s)\n", __FILE__, __LINE__, etiqueta);\
            fallos++;                                                           \
        }                                                                       \
    } while (0)

static int ejecutar_capturando(const char *fuente, char *out_buf, int out_cap) {
    const char *tmpfile =
#ifdef _WIN32
        "test_para_mien_out.txt";
#else
        "/tmp/test_para_mien_out.txt";
#endif
    if (!freopen(tmpfile, "w+", stdout)) return -1;

    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    int rc = -1;
    if (!p.tuvo_error) {
        Chunk chunk; chunk_iniciar(&chunk);
        Compilador c; compilador_iniciar(&c, &chunk);
        if (compilador_compilar_programa(&c, sents, n)) {
            VM vm; vm_iniciar(&vm);
            Valor r = valor_nulo();
            ResultadoVM rcvm = vm_ejecutar(&vm, &chunk, &r);
            valor_destruir(&r);
            vm_destruir(&vm);
            if (rcvm == VM_OK) rc = 0;
        }
        chunk_destruir(&chunk);
    }
    arena_destruir(&a);

    fflush(stdout);
#ifdef _WIN32
    freopen("CON", "w", stdout);
#else
    freopen("/dev/tty", "w", stdout);
#endif

    FILE *f = fopen(tmpfile, "r");
    if (f) {
        int leido = (int)fread(out_buf, 1, (size_t)(out_cap - 1), f);
        out_buf[leido] = '\0';
        fclose(f);
        remove(tmpfile);
    } else {
        out_buf[0] = '\0';
    }
    return rc;
}

int main(void) {
    /* Caso minimo: `para` dentro de `mientras` dentro de `para` con
       una asignacion nueva (cur) dentro del while. Crasheaba en la
       segunda iter del exterior antes de v1.130. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar coleccion\n"
            "funcion f():\n"
            "    nodos = [\"a\", \"b\"]\n"
            "    para n en nodos:\n"
            "        cola = coleccion.Cola()\n"
            "        cola.poner(n)\n"
            "        mientras no cola.vacia():\n"
            "            cur = cola.sacar()\n"
            "            imprimir(cur)\n"
            "            para v en [1, 2]:\n"
            "                imprimir(v)\n"
            "            fin para\n"
            "        fin mientras\n"
            "    fin para\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        /* Esperado: a, 1, 2, b, 1, 2 */
        AFIRMAR(strstr(out, "a") != NULL, "iter1_cur");
        AFIRMAR(strstr(out, "b") != NULL, "iter2_cur");
        /* "1" y "2" aparecen 2 veces cada uno */
        const char *p1 = strstr(out, "1");
        if (p1) p1 = strstr(p1 + 1, "1");
        AFIRMAR(p1 != NULL, "doble_para_interno");
    }

    /* Variante: tres niveles de para anidados con mientras intermedio. */
    {
        char out[1024];
        ejecutar_capturando(
            "funcion f():\n"
            "    para i en [1, 2]:\n"
            "        contador = 0\n"
            "        mientras contador < 2:\n"
            "            contador = contador + 1\n"
            "            para j en [10, 20]:\n"
            "                imprimir(i, contador, j)\n"
            "            fin para\n"
            "        fin mientras\n"
            "    fin para\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        /* Sin crash. Debe imprimir 8 lineas. */
        int n_lineas = 0;
        for (const char *p = out; *p; p++) if (*p == '\n') n_lineas++;
        AFIRMAR(n_lineas == 8, "tres_niveles");
    }

    /* Regresion: mientras simple (sin para interno) sigue funcionando. */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    i = 0\n"
            "    mientras i < 5:\n"
            "        i = i + 1\n"
            "    fin mientras\n"
            "    retornar i\n"
            "fin funcion\n"
            "imprimir(f())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "mientras_simple");
    }

    /* Regresion: mientras con romper. */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    i = 0\n"
            "    mientras verdadero:\n"
            "        si i == 3:\n"
            "            romper\n"
            "        fin si\n"
            "        i = i + 1\n"
            "    fin mientras\n"
            "    retornar i\n"
            "fin funcion\n"
            "imprimir(f())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "mientras_romper");
    }

    /* Regresion: la version idiomatica de grafos.componentes funciona. */
    {
        char out[512];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo(falso)\n"
            "g.agregar_arista(1, 2)\n"
            "g.agregar_arista(3, 4)\n"
            "g.agregar_nodo(5)\n"
            "imprimir(longitud(grafos.componentes(g)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "grafos_componentes");
    }

    if (fallos == 0) {
        printf("para_mien: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "para_mien: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
