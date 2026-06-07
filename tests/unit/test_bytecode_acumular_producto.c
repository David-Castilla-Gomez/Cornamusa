/*
 * Tests de `producto(xs, inicial=1)` y
 * `acumular(xs, op=nulo, inicial=nulo)` (v1.166) en
 * stdlib/funcionales.cor.
 *
 * - producto: dual aritmetico de suma, inicial=1 por defecto.
 * - acumular: sumas parciales / running totals. Paralelo de
 *   itertools.accumulate. Acepta op custom y semilla.
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
        "test_acum_prod_out.txt";
#else
        "/tmp/test_acum_prod_out.txt";
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
    /* producto basico */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.producto([2, 3, 4]))\n"
            "imprimir(funcionales.producto([5]))\n"
            "imprimir(funcionales.producto([]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "24\n5\n1") != NULL, "producto_basico");
    }

    /* producto con decimales */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.producto([2.5, 4.0]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10") != NULL, "producto_decimal");
    }

    /* producto = factorial usando rango */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.producto(rango(1, 11)))\n",
            out, sizeof(out));
        /* 10! = 3628800 */
        AFIRMAR(strstr(out, "3628800") != NULL, "producto_factorial");
    }

    /* producto con bignum */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.producto(rango(1, 21)))\n",
            out, sizeof(out));
        /* 20! = 2432902008176640000 */
        AFIRMAR(strstr(out, "2432902008176640000") != NULL,
                "producto_bignum_20fact");
    }

    /* producto con inicial */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.producto([2, 3], inicial=10))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "60") != NULL, "producto_inicial");
    }

    /* acumular suma basica */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.acumular([1, 2, 3, 4]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 3, 6, 10]") != NULL, "acumular_basico");
    }

    /* acumular con inicial */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.acumular([1, 2, 3, 4], inicial=100))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[101, 103, 106, 110]") != NULL,
                "acumular_con_inicial");
    }

    /* acumular con op custom (producto) */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.acumular([2, 3, 4], op=lambda a, b: a * b))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[2, 6, 24]") != NULL, "acumular_op_producto");
    }

    /* acumular con op custom (max) */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "funcion mayor(a, b):\n"
            "    si a > b:\n"
            "        retornar a\n"
            "    sino:\n"
            "        retornar b\n"
            "    fin si\n"
            "fin funcion\n"
            "imprimir(funcionales.acumular([3, 1, 4, 1, 5, 9, 2], op=mayor))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[3, 3, 4, 4, 5, 9, 9]") != NULL,
                "acumular_op_max");
    }

    /* acumular vacio */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.acumular([]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "acumular_vacio");
    }

    /* acumular con un solo elemento */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.acumular([42]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[42]") != NULL, "acumular_singleton");
    }

    /* acumular sobre cadenas (concatenacion) */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.acumular([\"a\", \"b\", \"c\"], inicial=\"\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"ab\", \"abc\"]") != NULL,
                "acumular_cadenas");
    }

    /* acumular sobre rango es lazy-friendly */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.acumular(rango(1, 6)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 3, 6, 10, 15]") != NULL, "acumular_rango");
    }

    if (fallos == 0) {
        printf("acumular/producto: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "acumular/producto: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
