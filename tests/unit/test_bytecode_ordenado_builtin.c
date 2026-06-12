/*
 * Tests de ordenado(iterable, clave=nulo, invertido=falso) como
 * builtin global (v1.196). Segundo fruto del invocador de callables
 * (v1.195).
 *
 * - Devuelve lista NUEVA (no muta, a diferencia de .ordenar()).
 * - Sort ESTABLE (merge sort) — paridad Python sorted.
 * - clave: callable opcional (Schwartzian).
 * - invertido: booleano posicional (3er arg).
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
        "test_ordenado_builtin_out.txt";
#else
        "/tmp/test_ordenado_builtin_out.txt";
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
    /* Basico + no muta el original */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [3, 1, 4, 1, 5]\n"
            "imprimir(ordenado(xs))\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 1, 3, 4, 5]\n[3, 1, 4, 1, 5]") != NULL,
                "no_muta");
    }

    /* clave con nativa (longitud) y con lambda */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(ordenado([\"bbb\", \"a\", \"cc\"], longitud))\n"
            "imprimir(ordenado([3, -1, 2], lambda x: -x))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"cc\", \"bbb\"]") != NULL, "clave_nativa");
        AFIRMAR(strstr(out, "[3, 2, -1]") != NULL, "clave_lambda");
    }

    /* invertido (con clave nulo) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(ordenado([3, 1, 2], nulo, verdadero))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[3, 2, 1]") != NULL, "invertido");
    }

    /* clave + invertido */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(ordenado([\"bbb\", \"a\", \"cc\"], longitud, verdadero))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"bbb\", \"cc\", \"a\"]") != NULL,
                "clave_invertido");
    }

    /* ESTABILIDAD: claves iguales conservan orden de entrada */
    {
        char out[512];
        ejecutar_capturando(
            "pares = [(1, \"x\"), (2, \"a\"), (1, \"y\"), (2, \"b\")]\n"
            "imprimir(ordenado(pares, lambda p: p[0]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, \"x\"), (1, \"y\"), (2, \"a\"), (2, \"b\")]") != NULL,
                "estabilidad");
    }

    /* Iterables: rango invertido, cadena, tupla */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(ordenado(rango(5, 0, -1)))\n"
            "imprimir(ordenado(\"dcba\"))\n"
            "imprimir(ordenado((3, 1, 2)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4, 5]") != NULL, "rango");
        AFIRMAR(strstr(out, "[\"a\", \"b\", \"c\", \"d\"]") != NULL, "cadena");
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "tupla");
    }

    /* Cadenas lexicografico */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(ordenado([\"pera\", \"manzana\", \"kiwi\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"kiwi\", \"manzana\", \"pera\"]") != NULL,
                "lexicografico");
    }

    /* Vacio */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(ordenado([]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "vacio");
    }

    /* Tipos mixtos no comparables -> ErrorDeTipo */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    ordenado([1, \"a\"])\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "mixtos_error");
    }

    /* Excepcion en la clave propaga */
    {
        char out[256];
        ejecutar_capturando(
            "funcion mala(x):\n"
            "    lanzar ErrorDeValor(\"boom\")\n"
            "fin funcion\n"
            "intentar:\n"
            "    ordenado([1, 2], mala)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "clave_excepcion");
    }

    /* funcionales.ordenado sigue funcionando (kwargs) */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.ordenado([2, 1], invertido=verdadero))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[2, 1]") != NULL, "modulo_compat");
    }

    /* Tuplas comparadas lexicograficamente sin clave */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(ordenado([(2, 1), (1, 9), (1, 2)]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, 2), (1, 9), (2, 1)]") != NULL,
                "tuplas_lex");
    }

    if (fallos == 0) {
        printf("ordenado_builtin: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "ordenado_builtin: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
