/*
 * Tests de `ordenado(xs, clave=, invertido=)` y comparacion lex de
 * tuplas y listas (v1.145).
 *
 * Pre v1.145:
 *   - `ordenar(lista)` solo aceptaba enteros/decimales/cadenas; tuplas
 *     y listas anidadas fallaban con ErrorDeTipo "no puede comparar
 *     tipos mixtos no numericos".
 *   - No habia version funcional (sin mutar) ni `key=` ni `reverse=`.
 *
 * v1.145:
 *   - `comparador_ordenar` en src/nativos.c añade dos casos: tupla vs
 *     tupla y lista vs lista. Compara lexicograficamente y recurre
 *     sobre los elementos (paridad con Python `sorted([(1, 2)])`).
 *   - Nueva funcion en stdlib `ordenado(xs, clave=nulo, invertido=falso)`:
 *     - sin clave: hace lista(xs) + ordenar() — copia, no muta.
 *     - con clave: Schwartzian transform (decorar con
 *       (clave(x), indice, x), ordenar, undecorar). El indice rompe
 *       empates de forma estable.
 *     - invertido: invierte el resultado al final.
 *
 * Sin cambios a VM ni bytecode (la kwarg `clave=` cae sobre funcion
 * libre, soportada desde v1.23).
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
        "test_ordenado_out.txt";
#else
        "/tmp/test_ordenado_out.txt";
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
    /* Comparacion lex de tuplas (sin clave) */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [(2, \"a\"), (1, \"z\"), (3, \"m\"), (1, \"a\")]\n"
            "ordenar(xs)\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, \"a\"), (1, \"z\"), (2, \"a\"), (3, \"m\")]") != NULL,
                "lex_tuplas");
    }

    /* Comparacion lex de listas (sin clave) */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [[2, 1], [1, 5], [1, 2]]\n"
            "ordenar(xs)\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[[1, 2], [1, 5], [2, 1]]") != NULL,
                "lex_listas");
    }

    /* Tuplas anidadas */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [(2, (1, 1)), (1, (2, 2)), (1, (1, 3))]\n"
            "ordenar(xs)\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, (1, 3)), (1, (2, 2)), (2, (1, 1))]") != NULL,
                "lex_anidado");
    }

    /* ordenado sin clave: no muta el original */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar ordenado\n"
            "orig = [3, 1, 2]\n"
            "nuevo = ordenado(orig)\n"
            "imprimir(orig)\n"
            "imprimir(nuevo)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[3, 1, 2]") != NULL, "ordenado_no_muta_orig");
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "ordenado_resultado");
    }

    /* ordenado con clave (Schwartzian) */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar ordenado\n"
            "xs = [(2, \"a\"), (1, \"z\"), (3, \"m\")]\n"
            "imprimir(ordenado(xs, clave=lambda p: p[1]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(2, \"a\"), (3, \"m\"), (1, \"z\")]") != NULL,
                "ordenado_clave");
    }

    /* ordenado invertido */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar ordenado\n"
            "imprimir(ordenado([3, 1, 4, 1, 5], invertido=verdadero))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[5, 4, 3, 1, 1]") != NULL, "ordenado_inv");
    }

    /* ordenado con clave + invertido */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar ordenado\n"
            "xs = [(2, \"a\"), (1, \"z\"), (3, \"m\")]\n"
            "imprimir(ordenado(xs, clave=lambda p: p[1], invertido=verdadero))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, \"z\"), (3, \"m\"), (2, \"a\")]") != NULL,
                "ordenado_clave_inv");
    }

    /* Estabilidad: claves iguales preservan orden original.
     * Construimos elementos con (clave_compartida, identidad) y
     * verificamos que tras ordenar por la clave, las identidades
     * salen en orden original. */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar ordenado\n"
            "xs = [(\"k\", 1), (\"k\", 2), (\"k\", 3), (\"k\", 4)]\n"
            "ys = ordenado(xs, clave=lambda p: p[0])\n"
            "ids = [p[1] para p en ys]\n"
            "imprimir(ids)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4]") != NULL, "estable");
    }

    /* Comparacion mixta entre incomparables sigue lanzando */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    ordenar([1, \"a\"])\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err mixto\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err mixto") != NULL, "mixto_falla");
    }

    if (fallos == 0) {
        printf("ordenado: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "ordenado: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
