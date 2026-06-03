/*
 * Tests de Heap y Contador (v1.116) en stdlib/coleccion.
 *
 * Heap: min-heap binario sobre lista. Operaciones poner/sacar O(log n),
 * vista O(1). Solo valores comparables nativos (numeros y cadenas).
 *
 * Contador: multiset estilo Counter de Python. Cuenta apariciones,
 * top-N mas comunes, total.
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
        "test_heap_out.txt";
#else
        "/tmp/test_heap_out.txt";
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
    /* Heap: orden de extraccion ascendente */
    {
        char out[512];
        ejecutar_capturando(
            "importar coleccion\n"
            "h = coleccion.Heap()\n"
            "para x en [5, 3, 8, 1, 9, 2, 7]:\n"
            "    h.poner(x)\n"
            "fin para\n"
            "ordenado = []\n"
            "mientras no h.vacia():\n"
            "    agregar(ordenado, h.sacar())\n"
            "fin mientras\n"
            "imprimir(ordenado)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 5, 7, 8, 9]") != NULL,
                "heap_orden_asc");
    }

    /* Heap: vista no consume */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "h = coleccion.Heap()\n"
            "h.poner(10)\n"
            "h.poner(5)\n"
            "h.poner(20)\n"
            "imprimir(h.vista())\n"
            "imprimir(h.vista())\n"
            "imprimir(h.sacar())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "5\n5\n5") != NULL ||
                strstr(out, "5\r\n5\r\n5") != NULL,
                "heap_vista_no_consume");
    }

    /* Heap: cadenas (orden lexicografico) */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "h = coleccion.Heap()\n"
            "h.poner(\"zorro\")\n"
            "h.poner(\"abeja\")\n"
            "h.poner(\"perro\")\n"
            "imprimir(h.sacar())\n"
            "imprimir(h.sacar())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "abeja") != NULL, "heap_min_cadena");
        AFIRMAR(strstr(out, "perro") != NULL, "heap_segundo");
    }

    /* Heap: vacio lanza ErrorDeValor */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "intentar:\n"
            "    coleccion.Heap().sacar()\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "heap_vacio_lanza");
    }

    /* Heap: longitud refleja contenido */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "h = coleccion.Heap()\n"
            "imprimir(longitud(h))\n"
            "h.poner(1)\n"
            "h.poner(2)\n"
            "imprimir(longitud(h))\n"
            "h.sacar()\n"
            "imprimir(longitud(h))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "0\n2\n1") != NULL ||
                strstr(out, "0\r\n2\r\n1") != NULL,
                "heap_longitud");
    }

    /* Contador: incrementos y obtener */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "c = coleccion.Contador()\n"
            "c.incrementar(\"rojo\")\n"
            "c.incrementar(\"azul\", 3)\n"
            "c.incrementar(\"rojo\")\n"
            "imprimir(c.obtener(\"rojo\"))\n"
            "imprimir(c.obtener(\"azul\"))\n"
            "imprimir(c.obtener(\"verde\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "cont_rojo_2");
        AFIRMAR(strstr(out, "3") != NULL, "cont_azul_3");
        AFIRMAR(strstr(out, "0") != NULL, "cont_default_0");
    }

    /* Contador: total */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "c = coleccion.Contador()\n"
            "c.incrementar(\"a\", 5)\n"
            "c.incrementar(\"b\", 3)\n"
            "c.incrementar(\"c\", 2)\n"
            "imprimir(c.total())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "10") != NULL, "cont_total");
    }

    /* Contador: mas_comunes ordenado descendente */
    {
        char out[512];
        ejecutar_capturando(
            "importar coleccion\n"
            "c = coleccion.Contador()\n"
            "c.incrementar(\"a\", 5)\n"
            "c.incrementar(\"b\", 10)\n"
            "c.incrementar(\"c\", 2)\n"
            "top = c.mas_comunes(2)\n"
            "imprimir(top[0][0])\n"
            "imprimir(top[1][0])\n"
            "imprimir(longitud(top))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "b") != NULL, "cont_mas_comunes_primero_b");
        AFIRMAR(strstr(out, "a") != NULL, "cont_mas_comunes_segundo_a");
        AFIRMAR(strstr(out, "2") != NULL, "cont_top_n");
    }

    /* Contador: decrementar elimina cuando llega a 0 */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "c = coleccion.Contador()\n"
            "c.incrementar(\"x\", 2)\n"
            "c.decrementar(\"x\")\n"
            "imprimir(c.obtener(\"x\"))\n"
            "c.decrementar(\"x\")\n"
            "imprimir(c.obtener(\"x\"))\n"
            "imprimir(longitud(c))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "1\n0\n0") != NULL ||
                strstr(out, "1\r\n0\r\n0") != NULL,
                "cont_decrementar_elimina");
    }

    /* Contador: inicializar con lista */
    {
        char out[256];
        ejecutar_capturando(
            "importar coleccion\n"
            "c = coleccion.Contador([\"a\", \"b\", \"a\", \"c\", \"a\", \"b\"])\n"
            "imprimir(c.obtener(\"a\"))\n"
            "imprimir(c.obtener(\"b\"))\n"
            "imprimir(c.obtener(\"c\"))\n"
            "imprimir(c.total())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "3\n2\n1\n6") != NULL ||
                strstr(out, "3\r\n2\r\n1\r\n6") != NULL,
                "cont_init_lista");
    }

    /* Contador: items para iteracion */
    {
        char out[512];
        ejecutar_capturando(
            "importar coleccion\n"
            "c = coleccion.Contador()\n"
            "c.incrementar(\"x\", 10)\n"
            "c.incrementar(\"y\", 5)\n"
            "total = 0\n"
            "para par en c.items():\n"
            "    total = total + par[1]\n"
            "fin para\n"
            "imprimir(total)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "15") != NULL, "cont_items_iter");
    }

    if (fallos == 0) {
        printf("heap_contador: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "heap_contador: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
