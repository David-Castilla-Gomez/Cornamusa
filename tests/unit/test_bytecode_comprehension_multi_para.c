/*
 * Tests de comprehensions con multiples `para` (v1.132).
 *
 * Antes: `[(x, y) para x en xs para y en ys]` daba ErrorDeSintaxis
 * con "se esperaba ']' al final de la comprehension".
 *
 * v1.132:
 *   AST: nueva struct `ClausulaComp` y campos `clausulas_extra` +
 *   `n_extras` en EXPR_COMPREHENSION para llevar las cláusulas
 *   adicionales (la primera sigue en los campos legacy).
 *
 *   Parser: `parsear_comprehension_cola` ahora acepta cero o más
 *   `para X en Y [si Z]` adicionales tras la primera. Para generator
 *   expressions (paréntesis) NO se aceptan extras todavía.
 *
 *   Compilador: refactor del case EXPR_COMPREHENSION para emitir N
 *   bucles anidados con slots pre-reservados de iter+var de TODAS
 *   las cláusulas ANTES del primer inicio_loop. Las cláusulas extra
 *   reciben su iterador con OP_ITER_INICIAR + OP_ASIGNAR_LOCAL al
 *   slot pre-reservado (no OP_NULO + agregar_local) — evita que el
 *   stack crezca cada iteración del loop padre (mismo bug que el de
 *   compilar_mientras de v1.130).
 *
 * Limite practico: 16 cláusulas (`para` x16 anidados). Mas allá de
 * eso el compilador rechaza con error claro.
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
        "test_comp_multi_out.txt";
#else
        "/tmp/test_comp_multi_out.txt";
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
    /* Producto cartesiano basico */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "letras = [\"a\", \"b\"]\n"
            "imprimir([(x, l) para x en xs para l en letras])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, \"a\"), (1, \"b\")") != NULL, "cartesiano_lista");
        AFIRMAR(strstr(out, "(3, \"b\")") != NULL, "cartesiano_final");
    }

    /* Guarda en la 2a clausula */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([(i, j) para i en rango(0, 3) para j en rango(0, 3) si i != j])\n",
            out, sizeof(out));
        /* 3*3 = 9 pares, menos i==j (3 pares) = 6 pares. */
        AFIRMAR(strstr(out, "(0, 1)") != NULL, "guarda_2a_0_1");
        AFIRMAR(strstr(out, "(2, 1)") != NULL, "guarda_2a_2_1");
    }

    /* Guarda en la 1a clausula */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([(a, b) para a en rango(0, 3) si a > 0 para b en rango(0, 2)])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 0)") != NULL, "guarda_1a_1_0");
        AFIRMAR(strstr(out, "(2, 1)") != NULL, "guarda_1a_2_1");
        AFIRMAR(strstr(out, "(0,") == NULL, "guarda_1a_excluye_0");
    }

    /* Dict comprehension multi-para */
    {
        char out[256];
        ejecutar_capturando(
            "d = {f\"{i}-{j}\": i + j para i en [1, 2] para j en [10, 20]}\n"
            "imprimir(d)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "\"1-10\": 11") != NULL, "dict_multi_11");
        AFIRMAR(strstr(out, "\"2-20\": 22") != NULL, "dict_multi_22");
    }

    /* Set comprehension multi-para */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud({i * j para i en [1, 2] para j en [3, 4]}))\n",
            out, sizeof(out));
        /* 1*3=3, 1*4=4, 2*3=6, 2*4=8 — 4 valores únicos. */
        AFIRMAR(strstr(out, "4") != NULL, "set_multi_card");
    }

    /* Tres clausulas anidadas */
    {
        char out[256];
        ejecutar_capturando(
            "tres = [(a, b, csv) para a en [1, 2] para b en [3, 4] para csv en [5, 6]]\n"
            "imprimir(longitud(tres))\n"
            "imprimir(tres[0])\n"
            "imprimir(tres[7])\n",
            out, sizeof(out));
        /* 2*2*2 = 8 tuplas. */
        AFIRMAR(strstr(out, "8") != NULL, "tres_card");
        AFIRMAR(strstr(out, "(1, 3, 5)") != NULL, "tres_primero");
        AFIRMAR(strstr(out, "(2, 4, 6)") != NULL, "tres_ultimo");
    }

    /* Regresion: comprehension simple sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([x * x para x en rango(0, 4)])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 4, 9]") != NULL, "regr_simple_lista");
    }

    /* Regresion: comprehension con guarda simple */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([x para x en rango(0, 10) si x % 2 == 0])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 2, 4, 6, 8]") != NULL, "regr_simple_guarda");
    }

    /* Regresion: dict comprehension simple */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir({x: x * x para x en rango(0, 4)})\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0: 0") != NULL, "regr_simple_dict_0");
        AFIRMAR(strstr(out, "3: 9") != NULL, "regr_simple_dict_3");
    }

    if (fallos == 0) {
        printf("comp_multi: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "comp_multi: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
