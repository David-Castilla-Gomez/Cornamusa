/*
 * Tests de `tomar_mientras`, `descartar_mientras` y `particionar`
 * en stdlib (v1.147).
 *
 * Cornamusa ya tenia `tomar(n, xs)` y `saltar(n, xs)` por cantidad
 * fija. v1.147 anade los tres variantes funcionales por predicado:
 *
 *   tomar_mientras(p, xs)     — paridad con itertools.takewhile
 *   descartar_mientras(p, xs) — paridad con itertools.dropwhile
 *   particionar(p, xs)        — equivale a (filtrar(p, xs), filtrar(no p, xs))
 *                                pero en una sola pasada
 *
 * Idiomaticos para procesar streams con prefijos homogeneos o
 * dividir en "buenos/malos" sin escribir dos filtrar consecutivos.
 *
 * Sin cambios al nucleo. Solo stdlib usando features existentes
 * (lambdas, romper, continuar, tupla retornada).
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
        "test_tdp_out.txt";
#else
        "/tmp/test_tdp_out.txt";
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
    /* tomar_mientras: prefijo de positivos */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar tomar_mientras\n"
            "imprimir(tomar_mientras(lambda x: x > 0, [3, 1, 2, -1, 5, 7]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[3, 1, 2]") != NULL, "tm_prefijo");
    }

    /* tomar_mientras: todos cumplen */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar tomar_mientras\n"
            "imprimir(tomar_mientras(lambda x: x < 100, [1, 2, 3]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "tm_todos");
    }

    /* tomar_mientras: ninguno cumple (el primero rompe) */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar tomar_mientras\n"
            "imprimir(tomar_mientras(lambda x: x < 0, [1, 2, 3]))\n"
            "imprimir(tomar_mientras(lambda x: verdadero, []))\n",
            out, sizeof(out));
        const char *p = out;
        int n = 0;
        while ((p = strstr(p, "[]")) != NULL) { n++; p++; }
        AFIRMAR(n >= 2, "tm_ninguno_y_vacio");
    }

    /* descartar_mientras: simetrico a tomar_mientras */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar descartar_mientras\n"
            "imprimir(descartar_mientras(lambda x: x > 0, [3, 1, 2, -1, 5, 7]))\n",
            out, sizeof(out));
        /* Tras descartar 3,1,2 (positivos al inicio), -1 rompe la racha
         * y los positivos siguientes (5, 7) tambien pasan. */
        AFIRMAR(strstr(out, "[-1, 5, 7]") != NULL, "dm_simetria");
    }

    /* descartar_mientras: primero no cumple → conserva todo */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar descartar_mientras\n"
            "imprimir(descartar_mientras(lambda x: x < 0, [1, 2, 3]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "dm_primero_falla");
    }

    /* descartar_mientras: todos cumplen → vacio */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar descartar_mientras\n"
            "imprimir(descartar_mientras(lambda x: x < 100, [1, 2, 3]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "dm_todos");
    }

    /* particionar: caso clasico pares vs impares */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar particionar\n"
            "pares, impares = particionar(lambda x: x % 2 == 0, [1, 2, 3, 4, 5])\n"
            "imprimir(pares)\n"
            "imprimir(impares)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[2, 4]") != NULL, "part_pares");
        AFIRMAR(strstr(out, "[1, 3, 5]") != NULL, "part_impares");
    }

    /* particionar: vacio */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar particionar\n"
            "imprimir(particionar(lambda x: x > 0, []))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "([], [])") != NULL, "part_vacio");
    }

    /* particionar: todo a un lado */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar particionar\n"
            "imprimir(particionar(lambda x: verdadero, [1, 2, 3]))\n"
            "imprimir(particionar(lambda x: falso, [1, 2, 3]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "([1, 2, 3], [])") != NULL, "part_todos_si");
        AFIRMAR(strstr(out, "([], [1, 2, 3])") != NULL, "part_todos_no");
    }

    /* Caso real: separar buenos/malos por prefijo de cadena */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar particionar\n"
            "msgs = [\"INFO: ok\", \"ERROR: fallo\", \"INFO: ok\", \"ERROR: critico\"]\n"
            "buenos, malos = particionar(lambda m: m[0:4] == \"INFO\", msgs)\n"
            "imprimir(longitud(buenos), longitud(malos))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2 2") != NULL, "part_real");
    }

    if (fallos == 0) {
        printf("tdp: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "tdp: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
