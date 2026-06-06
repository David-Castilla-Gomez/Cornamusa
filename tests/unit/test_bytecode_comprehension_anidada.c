/*
 * Tests de patrones anidados (tuplas dentro de tuplas) en
 * comprehensions y generator expressions (v1.138).
 *
 * Antes (post v1.135/v1.136):
 *   [a + b para a, b en pares]                   OK (un nivel)
 *   [a + b + c para (a, (b, c)) en triples]      ErrorDeSintaxis
 *
 * v1.138: paridad con `para` (v1.137). El parser
 * parsear_destino_compr acepta `(...)` recursivamente. El
 * compilador EXPR_COMPREHENSION (incluyendo genex) usa helpers
 * recursivos:
 *   validar_patron_compr        — DFS, max un STAR por nivel
 *   contar_slots_patron         — DFS, slots hoja + sub-tupla
 *   prereservar_slots_patron_compr — DFS, OP_NULO+agregar_local
 *   emitir_destruct_patron_compr   — DFS, OP_INDICE/REBANADA y
 *                                     recursion para sub-tupla
 *
 * Sin cambios a bytecode ni VM.
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
        "test_compr_anid_out.txt";
#else
        "/tmp/test_compr_anid_out.txt";
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
    /* List comprehension con patron anidado */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([a + b + c para (a, (b, c)) en "
            "[(1, (10, 100)), (2, (20, 200))]])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[111, 222]") != NULL, "list_anidado");
    }

    /* Mezcla plano + anidado (primer destino plano, segundo
     * anidado, sin envolver) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([(a, b, c) para a, (b, c) en "
            "[(1, (10, 100)), (2, (20, 200))]])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 10, 100)") != NULL, "mezcla_1");
        AFIRMAR(strstr(out, "(2, 20, 200)") != NULL, "mezcla_2");
    }

    /* Star dentro de sub-patron */
    {
        char out[512];
        ejecutar_capturando(
            "imprimir([(a, ult, mid) para (a, (*mid, ult)) en "
            "[(1, (10, 20, 30)), (2, (100, 200))]])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 30, [10, 20])") != NULL,
                "star_anidado_1");
        AFIRMAR(strstr(out, "(2, 200, [100])") != NULL,
                "star_anidado_2");
    }

    /* Dict comprehension con patron anidado */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir({k: v + w para (k, (v, w)) en "
            "[(\"a\", (1, 2)), (\"b\", (10, 20))]})\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "\"a\": 3") != NULL, "dict_anid_a");
        AFIRMAR(strstr(out, "\"b\": 30") != NULL, "dict_anid_b");
    }

    /* Set comprehension con patron anidado */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud({a + b para (a, (b, c)) en "
            "[(1, (2, 3)), (4, (5, 6))]}))\n",
            out, sizeof(out));
        /* a=1,b=2 -> 3 ; a=4,b=5 -> 9. Dos valores unicos. */
        AFIRMAR(strstr(out, "2") != NULL, "set_anid");
    }

    /* Generator expression con patron anidado */
    {
        char out[256];
        ejecutar_capturando(
            "g = (a + b + c para (a, (b, c)) en "
            "[(1, (10, 100)), (2, (20, 200))])\n"
            "para v en g: imprimir(v)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "111") != NULL && strstr(out, "222") != NULL,
                "genex_anid");
    }

    /* Triple profundidad */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([(a, b, c, d) para (a, (b, (c, d))) en "
            "[(1, (2, (3, 4))), (10, (20, (30, 40)))]])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 2, 3, 4)") != NULL, "profundo_1");
        AFIRMAR(strstr(out, "(10, 20, 30, 40)") != NULL, "profundo_2");
    }

    /* Anidado dentro de funcion */
    {
        char out[256];
        ejecutar_capturando(
            "funcion procesar(triples):\n"
            "    retornar [a + b + c para (a, (b, c)) en triples]\n"
            "fin funcion\n"
            "imprimir(procesar([(1, (10, 100)), (2, (20, 200))]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[111, 222]") != NULL, "en_funcion");
    }

    /* Multi-para con anidados en distintas clausulas */
    {
        char out[512];
        ejecutar_capturando(
            "imprimir([(a, b, x, w) para (a, b) en [(1, 2)] "
            "para (x, w) en [(10, 20), (30, 40)]])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 2, 10, 20)") != NULL, "multi_anid_1");
        AFIRMAR(strstr(out, "(1, 2, 30, 40)") != NULL, "multi_anid_2");
    }

    /* Aridad del sub-patron incorrecta lanza ErrorDeValor */
    {
        char out[512];
        ejecutar_capturando(
            "intentar:\n"
            "    bad = [a + b + c para (a, (b, c)) en [(1, (10, 100, 999))]]\n"
            "    imprimir(bad)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "aridad_subpatron");
    }

    /* Regresion: comprehension simple sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([mm * 2 para mm en [1, 2, 3]])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[2, 4, 6]") != NULL, "regresion_simple");
    }

    /* Regresion: destructuring plano */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([a + b para a, b en [(1, 10), (2, 20)]])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[11, 22]") != NULL, "regresion_plano");
    }

    /* Regresion: star plano */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([resto para primero, *resto en [[1, 2, 3], [10, 20]]])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[[2, 3], [20]]") != NULL, "regresion_star");
    }

    if (fallos == 0) {
        printf("compr_anidada: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "compr_anidada: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
