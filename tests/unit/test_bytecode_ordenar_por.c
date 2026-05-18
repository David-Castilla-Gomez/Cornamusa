/*
 * Tests de funcionales.ordenar_por (v1.101).
 *
 * Mergesort estable pure-Cornamusa con clave callable. Soluciona la
 * limitacion del built-in `ordenar` que solo compara numeros y
 * cadenas directamente.
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
        "test_ordp_out.txt";
#else
        "/tmp/test_ordp_out.txt";
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
    /* Ordenar enteros por identidad */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "r = funcionales.ordenar_por([3, 1, 4, 1, 5, 9, 2, 6], lambda x: x)\n"
            "imprimir(r)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 1, 2, 3, 4, 5, 6, 9]") != NULL, "enteros_identidad");
    }

    /* Ordenar cadenas por longitud */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "r = funcionales.ordenar_por([\"hola\", \"a\", \"hola mundo\", \"ab\"],"
            " lambda s: longitud(s))\n"
            "imprimir(r)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"ab\", \"hola\", \"hola mundo\"]") != NULL,
                "cadenas_por_longitud");
    }

    /* Lista vacia */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.ordenar_por([], lambda x: x))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "lista_vacia");
    }

    /* Lista de un solo elemento */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.ordenar_por([42], lambda x: x))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[42]") != NULL, "lista_un_elemento");
    }

    /* No muta la lista original */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "xs = [3, 1, 2]\n"
            "funcionales.ordenar_por(xs, lambda x: x)\n"
            "imprimir(xs)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[3, 1, 2]") != NULL, "no_muta_original");
    }

    /* Ordenar diccionarios por campo */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "g = [{\"n\": \"A\", \"e\": 30}, {\"n\": \"B\", \"e\": 20},"
            " {\"n\": \"C\", \"e\": 40}]\n"
            "r = funcionales.ordenar_por(g, lambda p: p[\"e\"])\n"
            "para p en r:\n"
            "    imprimir(p[\"n\"])\n"
            "fin para\n", out, sizeof(out));
        AFIRMAR(strstr(out, "B\nA\nC") != NULL || strstr(out, "B\r\nA\r\nC") != NULL,
                "dicts_por_campo");
    }

    /* Estabilidad: elementos con misma clave preservan orden */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            /* Mismo grupo 0 con tres elementos en orden A, B, C */
            "xs = [[0, \"A\"], [1, \"X\"], [0, \"B\"], [0, \"C\"]]\n"
            "r = funcionales.ordenar_por(xs, lambda p: p[0])\n"
            "imprimir(r)\n", out, sizeof(out));
        /* Esperamos: [0,A], [0,B], [0,C], [1,X] */
        AFIRMAR(strstr(out, "[[0, \"A\"], [0, \"B\"], [0, \"C\"], [1, \"X\"]]") != NULL,
                "estabilidad");
    }

    /* ordenar_por_inverso */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "r = funcionales.ordenar_por_inverso([1, 5, 3, 2, 4], lambda x: x)\n"
            "imprimir(r)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[5, 4, 3, 2, 1]") != NULL, "inverso_basico");
    }

    /* Clave compuesta: dos pasadas estables = sort por dos campos */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "items = [[\"b\", 1], [\"a\", 2], [\"c\", 0], [\"a\", 1]]\n"
            "por_n = funcionales.ordenar_por(items, lambda p: p[1])\n"
            "por_l = funcionales.ordenar_por(por_n, lambda p: p[0])\n"
            "imprimir(por_l)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[[\"a\", 1], [\"a\", 2], [\"b\", 1], [\"c\", 0]]") != NULL,
                "clave_compuesta");
    }

    /* Funcion key con nombre (no lambda) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "funcion segunda_letra(s):\n"
            "    si longitud(s) < 2:\n"
            "        retornar \"\"\n"
            "    fin si\n"
            "    retornar s[1]\n"
            "fin funcion\n"
            "r = funcionales.ordenar_por([\"xyz\", \"abc\", \"bcd\"], segunda_letra)\n"
            "imprimir(r)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[\"abc\", \"bcd\", \"xyz\"]") != NULL, "key_funcion_nombrada");
    }

    /* Lista grande (>10 elementos para que mergesort recurra varios niveles) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "xs = [9, 3, 7, 1, 8, 2, 5, 4, 6, 0, 11, 14, 12, 13, 10]\n"
            "r = funcionales.ordenar_por(xs, lambda x: x)\n"
            "imprimir(r)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]") != NULL,
                "lista_grande");
    }

    if (fallos == 0) {
        printf("ordenar_por: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "ordenar_por: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
