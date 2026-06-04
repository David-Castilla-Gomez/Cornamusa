/*
 * Tests del modulo stdlib/iteradores (v1.118).
 *
 * Cubre combinatoria (producto, producto3, producto_repeticion,
 * permutaciones, combinaciones, combinaciones_con_repeticion) y
 * herramientas de iteracion (concatenar, repetir, ventana,
 * pares_consecutivos, agrupar_consecutivos, comprimir, dividir_en).
 *
 * Distinto de test_bytecode_iteradores.c, que cubre el dunder
 * `__iterar__` (protocolo de iteracion de clases).
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
        "test_iters_out.txt";
#else
        "/tmp/test_iters_out.txt";
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
    /* producto: 2x3 = 6 elementos */
    {
        char out[512];
        ejecutar_capturando(
            "importar iteradores\n"
            "p = iteradores.producto([1, 2], [\"a\", \"b\", \"c\"])\n"
            "imprimir(longitud(p))\n"
            "imprimir(p[0])\n"
            "imprimir(p[5])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "6") != NULL, "producto_cardinal");
        AFIRMAR(strstr(out, "[1, \"a\"]") != NULL, "producto_primero");
        AFIRMAR(strstr(out, "[2, \"c\"]") != NULL, "producto_ultimo");
    }

    /* producto3: 2x2x2 = 8 */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "p = iteradores.producto3([1, 2], [3, 4], [5, 6])\n"
            "imprimir(longitud(p))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "8") != NULL, "producto3_cardinal");
    }

    /* producto_repeticion: 2^4 = 16 */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "p = iteradores.producto_repeticion([0, 1], 4)\n"
            "imprimir(longitud(p))\n"
            "imprimir(p[0])\n"
            "imprimir(p[15])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "16") != NULL, "producto_rep_cardinal");
        AFIRMAR(strstr(out, "[0, 0, 0, 0]") != NULL, "producto_rep_primero");
        AFIRMAR(strstr(out, "[1, 1, 1, 1]") != NULL, "producto_rep_ultimo");
    }

    /* permutaciones: 3! = 6 */
    {
        char out[512];
        ejecutar_capturando(
            "importar iteradores\n"
            "p = iteradores.permutaciones([1, 2, 3])\n"
            "imprimir(longitud(p))\n"
            "imprimir(p[0])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "6") != NULL, "perm_cardinal");
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "perm_primera");
    }

    /* permutaciones r=2 de 4: P(4,2) = 12 */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "p = iteradores.permutaciones([1, 2, 3, 4], 2)\n"
            "imprimir(longitud(p))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "12") != NULL, "perm_r_cardinal");
    }

    /* combinaciones C(4,2) = 6 */
    {
        char out[512];
        ejecutar_capturando(
            "importar iteradores\n"
            "c = iteradores.combinaciones([1, 2, 3, 4], 2)\n"
            "imprimir(longitud(c))\n"
            "imprimir(c[0])\n"
            "imprimir(c[5])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "6") != NULL, "comb_cardinal");
        AFIRMAR(strstr(out, "[1, 2]") != NULL, "comb_primera");
        AFIRMAR(strstr(out, "[3, 4]") != NULL, "comb_ultima");
    }

    /* combinaciones r=0 -> [[]] */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "c = iteradores.combinaciones([1, 2, 3], 0)\n"
            "imprimir(longitud(c))\n"
            "imprimir(c[0])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "comb_r0_card");
        AFIRMAR(strstr(out, "[]") != NULL, "comb_r0_vacia");
    }

    /* combinaciones r>n -> [] */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "c = iteradores.combinaciones([1, 2], 5)\n"
            "imprimir(longitud(c))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "comb_r_mayor_n");
    }

    /* combinaciones_con_repeticion C(n+r-1, r): C(4, 2) = 6 */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "c = iteradores.combinaciones_con_repeticion([1, 2, 3], 2)\n"
            "imprimir(longitud(c))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "6") != NULL, "comb_rep_cardinal");
    }

    /* concatenar */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "imprimir(iteradores.concatenar([1, 2], [3, 4, 5]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4, 5]") != NULL, "concatenar");
    }

    /* repetir */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "imprimir(iteradores.repetir(\"x\", 3))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"x\", \"x\", \"x\"]") != NULL, "repetir");
    }

    /* repetir n=0 -> [] */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "imprimir(longitud(iteradores.repetir(1, 0)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "repetir_n0");
    }

    /* ventana: n-k+1 ventanas */
    {
        char out[512];
        ejecutar_capturando(
            "importar iteradores\n"
            "v = iteradores.ventana([1, 2, 3, 4, 5], 3)\n"
            "imprimir(longitud(v))\n"
            "imprimir(v[0])\n"
            "imprimir(v[2])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "ventana_cardinal");
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "ventana_primera");
        AFIRMAR(strstr(out, "[3, 4, 5]") != NULL, "ventana_ultima");
    }

    /* ventana mas grande que la lista -> [] */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "imprimir(longitud(iteradores.ventana([1, 2], 5)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "ventana_grande");
    }

    /* ventana n=0 lanza */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "intentar:\n"
            "    iteradores.ventana([1, 2, 3], 0)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "ventana_n0_lanza");
    }

    /* pares_consecutivos */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "imprimir(iteradores.pares_consecutivos([1, 2, 3, 4]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[[1, 2], [2, 3], [3, 4]]") != NULL, "pares");
    }

    /* agrupar_consecutivos: [1,1,2,3,3,3,1] -> 4 grupos */
    {
        char out[512];
        ejecutar_capturando(
            "importar iteradores\n"
            "g = iteradores.agrupar_consecutivos([1, 1, 2, 3, 3, 3, 1])\n"
            "imprimir(longitud(g))\n"
            "imprimir(g[0])\n"
            "imprimir(g[3])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "4") != NULL, "agrupar_cardinal");
        AFIRMAR(strstr(out, "[1, [1, 1]]") != NULL, "agrupar_primero");
        AFIRMAR(strstr(out, "[1, [1]]") != NULL, "agrupar_ultimo");
    }

    /* agrupar_consecutivos vacia -> [] */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "imprimir(longitud(iteradores.agrupar_consecutivos([])))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "agrupar_vacia");
    }

    /* comprimir */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "r = iteradores.comprimir([\"a\", \"b\", \"c\", \"d\"], "
            "[verdadero, falso, verdadero, falso])\n"
            "imprimir(r)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"c\"]") != NULL, "comprimir");
    }

    /* dividir_en: bloques iguales y resto */
    {
        char out[512];
        ejecutar_capturando(
            "importar iteradores\n"
            "d = iteradores.dividir_en([1, 2, 3, 4, 5, 6, 7], 3)\n"
            "imprimir(longitud(d))\n"
            "imprimir(d[0])\n"
            "imprimir(d[2])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "dividir_cardinal");
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "dividir_primero");
        AFIRMAR(strstr(out, "[7]") != NULL, "dividir_ultimo");
    }

    /* dividir_en n=0 lanza */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "intentar:\n"
            "    iteradores.dividir_en([1, 2, 3], 0)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "dividir_n0_lanza");
    }

    if (fallos == 0) {
        printf("iteradores_stdlib: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "iteradores_stdlib: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
