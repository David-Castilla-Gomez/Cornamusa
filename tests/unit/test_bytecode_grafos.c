/*
 * Tests del modulo stdlib/grafos (v1.119).
 *
 * Cubre clase Grafo (dirigido + no dirigido) + algoritmos
 * clasicos: BFS, DFS, Dijkstra, camino_mas_corto, topologico,
 * tiene_ciclo, componentes.
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
        "test_grafos_out.txt";
#else
        "/tmp/test_grafos_out.txt";
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
    /* Grafo basico: agregar aristas crea nodos */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"A\", \"B\", 5)\n"
            "imprimir(longitud(g))\n"
            "imprimir(g.peso(\"A\", \"B\"))\n"
            "imprimir(g.peso(\"B\", \"A\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "grafo_nodos_creados");
        AFIRMAR(strstr(out, "5") != NULL, "grafo_peso");
        AFIRMAR(strstr(out, "nulo") != NULL, "grafo_dirigido_no_inversa");
    }

    /* No dirigido: inversa creada */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo(falso)\n"
            "g.agregar_arista(\"A\", \"B\", 7)\n"
            "imprimir(g.peso(\"B\", \"A\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "7") != NULL, "no_dirigido_inversa");
    }

    /* BFS orden de visita en grafo lineal */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"A\", \"B\")\n"
            "g.agregar_arista(\"A\", \"C\")\n"
            "g.agregar_arista(\"B\", \"D\")\n"
            "imprimir(grafos.bfs(g, \"A\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"A\", \"B\", \"C\", \"D\"]") != NULL, "bfs_orden");
    }

    /* DFS orden preorden */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"A\", \"B\")\n"
            "g.agregar_arista(\"A\", \"C\")\n"
            "g.agregar_arista(\"B\", \"D\")\n"
            "imprimir(grafos.dfs(g, \"A\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"A\", \"B\", \"D\", \"C\"]") != NULL, "dfs_orden");
    }

    /* Dijkstra: camino corto por C en vez de B directo */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"A\", \"B\", 4)\n"
            "g.agregar_arista(\"A\", \"C\", 2)\n"
            "g.agregar_arista(\"C\", \"B\", 1)\n"
            "d = grafos.dijkstra(g, \"A\")\n"
            "imprimir(d[\"A\"])\n"
            "imprimir(d[\"B\"])\n"
            "imprimir(d[\"C\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "dijkstra_origen");
        AFIRMAR(strstr(out, "3") != NULL, "dijkstra_via_C");
        AFIRMAR(strstr(out, "2") != NULL, "dijkstra_C_directo");
    }

    /* Dijkstra: peso negativo lanza */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"A\", \"B\", -1)\n"
            "intentar:\n"
            "    grafos.dijkstra(g, \"A\")\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "dijkstra_neg_lanza");
    }

    /* camino_mas_corto */
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
        AFIRMAR(strstr(out, "[\"A\", \"C\", \"B\"]") != NULL, "camino_via_C");
    }

    /* camino_mas_corto: nodos no conectados -> [] */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_nodo(\"A\")\n"
            "g.agregar_nodo(\"B\")\n"
            "imprimir(grafos.camino_mas_corto(g, \"A\", \"B\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "camino_no_existe");
    }

    /* Topologico de un DAG */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"a\", \"b\")\n"
            "g.agregar_arista(\"a\", \"c\")\n"
            "g.agregar_arista(\"b\", \"d\")\n"
            "g.agregar_arista(\"c\", \"d\")\n"
            "imprimir(grafos.topologico(g))\n",
            out, sizeof(out));
        /* a primero, d ultimo; b y c pueden ir en cualquier orden */
        AFIRMAR(strstr(out, "\"a\"") != NULL, "topo_a_primero");
        AFIRMAR(strstr(out, "\"d\"") != NULL, "topo_d_ultimo");
    }

    /* Topologico con ciclo lanza */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"a\", \"b\")\n"
            "g.agregar_arista(\"b\", \"a\")\n"
            "intentar:\n"
            "    grafos.topologico(g)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "topo_ciclo_lanza");
    }

    /* tiene_ciclo dirigido */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"a\", \"b\")\n"
            "g.agregar_arista(\"b\", \"c\")\n"
            "g.agregar_arista(\"c\", \"a\")\n"
            "imprimir(grafos.tiene_ciclo(g))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "ciclo_dirigido");
    }

    /* tiene_ciclo dirigido = falso */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"a\", \"b\")\n"
            "g.agregar_arista(\"b\", \"c\")\n"
            "imprimir(grafos.tiene_ciclo(g))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "falso") != NULL, "ciclo_no_dirigido_dag");
    }

    /* Componentes: 3 grupos en no dirigido */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo(falso)\n"
            "g.agregar_arista(1, 2)\n"
            "g.agregar_arista(3, 4)\n"
            "g.agregar_nodo(5)\n"
            "imprimir(longitud(grafos.componentes(g)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "componentes_3");
    }

    /* quitar_arista */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_arista(\"A\", \"B\", 5)\n"
            "g.quitar_arista(\"A\", \"B\")\n"
            "imprimir(g.peso(\"A\", \"B\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "nulo") != NULL, "quitar_arista");
    }

    /* contiene */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "g.agregar_nodo(\"X\")\n"
            "imprimir(g.contiene(\"X\"))\n"
            "imprimir(g.contiene(\"Y\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "contiene_si");
        AFIRMAR(strstr(out, "falso") != NULL, "contiene_no");
    }

    /* BFS desde nodo no presente lanza */
    {
        char out[256];
        ejecutar_capturando(
            "importar grafos\n"
            "g = grafos.Grafo()\n"
            "intentar:\n"
            "    grafos.bfs(g, \"X\")\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "bfs_nodo_ausente_lanza");
    }

    if (fallos == 0) {
        printf("grafos: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "grafos: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
