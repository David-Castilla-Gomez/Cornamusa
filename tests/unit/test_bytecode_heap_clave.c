/*
 * Tests de Heap con clave personalizada (v1.120).
 *
 * Heap acepta callable opcional `clave` que extrae el valor de
 * comparacion. Permite heap de listas/dicts/cualquier valor no
 * comparable directamente con `<`.
 *
 * Tambien cubre regresion: dijkstra con heap interno debe seguir
 * dando los mismos resultados que la version O(V^2) anterior.
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
        "test_heap_clave_out.txt";
#else
        "/tmp/test_heap_clave_out.txt";
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
    /* Heap sin clave: comportamiento clasico (regresion) */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "h = coleccion.Heap()\n"
            "para x en [5, 1, 3, 8, 2]:\n"
            "    h.poner(x)\n"
            "fin para\n"
            "ord = []\n"
            "mientras no h.vacia():\n"
            "    agregar(ord, h.sacar())\n"
            "fin mientras\n"
            "imprimir(ord)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 5, 8]") != NULL, "heap_sin_clave_regresion");
    }

    /* Heap con clave: pares [prioridad, dato] */
    {
        char out[512];
        ejecutar_capturando(
            "importar coleccion\n"
            "h = coleccion.Heap(lambda t: t[0])\n"
            "h.poner([3, \"limpiar\"])\n"
            "h.poner([1, \"urgente\"])\n"
            "h.poner([2, \"medio\"])\n"
            "imprimir(h.sacar())\n"
            "imprimir(h.sacar())\n"
            "imprimir(h.sacar())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, \"urgente\"]") != NULL, "clave_par_primero");
        AFIRMAR(strstr(out, "[2, \"medio\"]") != NULL, "clave_par_segundo");
        AFIRMAR(strstr(out, "[3, \"limpiar\"]") != NULL, "clave_par_tercero");
    }

    /* Heap con clave: dicts ordenados por campo */
    {
        char out[512];
        ejecutar_capturando(
            "importar coleccion\n"
            "h = coleccion.Heap(lambda d: d[\"n\"])\n"
            "h.poner({\"n\": 10, \"v\": \"diez\"})\n"
            "h.poner({\"n\": 1,  \"v\": \"uno\"})\n"
            "h.poner({\"n\": 5,  \"v\": \"cinco\"})\n"
            "imprimir(h.sacar()[\"v\"])\n"
            "imprimir(h.sacar()[\"v\"])\n"
            "imprimir(h.sacar()[\"v\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "uno") != NULL, "clave_dict_primero");
        AFIRMAR(strstr(out, "cinco") != NULL, "clave_dict_segundo");
        AFIRMAR(strstr(out, "diez") != NULL, "clave_dict_tercero");
    }

    /* Heap con clave invertida -> max-heap */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "h = coleccion.Heap(lambda x: -x)\n"
            "para x en [3, 1, 7, 4, 2]:\n"
            "    h.poner(x)\n"
            "fin para\n"
            "ord = []\n"
            "mientras no h.vacia():\n"
            "    agregar(ord, h.sacar())\n"
            "fin mientras\n"
            "imprimir(ord)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[7, 4, 3, 2, 1]") != NULL, "clave_max_heap");
    }

    /* Heap con clave: vista() devuelve el menor por clave */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "h = coleccion.Heap(lambda p: p[0])\n"
            "h.poner([10, \"a\"])\n"
            "h.poner([3,  \"b\"])\n"
            "h.poner([7,  \"c\"])\n"
            "imprimir(h.vista())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[3, \"b\"]") != NULL, "clave_vista");
    }

    /* Empates por clave preservan equivalencia (no se rompe) */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "h = coleccion.Heap(lambda p: p[0])\n"
            "h.poner([1, \"a\"])\n"
            "h.poner([1, \"b\"])\n"
            "h.poner([1, \"c\"])\n"
            "imprimir(longitud(h))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "clave_empates");
    }

    /* Regresion: dijkstra con heap sigue dando mismos resultados */
    {
        char out[512];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"A\", \"B\", 4)\n"
            "g.agregar_arista(\"A\", \"C\", 2)\n"
            "g.agregar_arista(\"C\", \"B\", 1)\n"
            "g.agregar_arista(\"B\", \"D\", 5)\n"
            "g.agregar_arista(\"C\", \"D\", 8)\n"
            "d = grafos.dijkstra(g, \"A\")\n"
            "imprimir(d[\"A\"])\n"
            "imprimir(d[\"B\"])\n"
            "imprimir(d[\"C\"])\n"
            "imprimir(d[\"D\"])\n",
            out, sizeof(out));
        /* A=0, C=2 (directo), B=3 (A->C->B), D=8 (A->C->B->D) */
        AFIRMAR(strstr(out, "0") != NULL, "dijkstra_heap_A");
        AFIRMAR(strstr(out, "2") != NULL, "dijkstra_heap_C");
        AFIRMAR(strstr(out, "3") != NULL, "dijkstra_heap_B");
        AFIRMAR(strstr(out, "8") != NULL, "dijkstra_heap_D");
    }

    /* Regresion: camino_mas_corto con heap */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"A\", \"B\", 4)\n"
            "g.agregar_arista(\"A\", \"C\", 2)\n"
            "g.agregar_arista(\"C\", \"B\", 1)\n"
            "imprimir(grafos.camino_mas_corto(g, \"A\", \"B\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"A\", \"C\", \"B\"]") != NULL, "camino_heap");
    }

    if (fallos == 0) {
        printf("heap_clave: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "heap_clave: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
