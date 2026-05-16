/*
 * Tests del linter (v1.49 - Fase 5 tooling).
 *
 * Verifica las 4 categorias de la version inicial:
 *   - UNREACHABLE      (sentencias tras retornar/romper/continuar/lanzar)
 *   - REDUNDANT_PASAR  (`pasar` en bloque con mas sentencias)
 *   - EQ_NULO          (== nulo / != nulo → es nulo / no es nulo)
 *   - UNUSED_IMPORT    (modulo importado pero nunca referenciado)
 *
 * Test estructura: cada caso compone un programa minimo, lo pasa por
 * lexer+parser, llama a `linter_analizar`, y comprueba que el numero y
 * tipo de avisos sea el esperado. Tambien verifica que codigo limpio
 * NO produce avisos (no falsos positivos).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "lexer.h"
#include "linter.h"
#include "parser.h"

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

typedef struct {
    int n_warnings;
    int n_unreachable;
    int n_redundant_pasar;
    int n_eq_nulo;
    int n_unused_import;
} Resumen;

static Resumen analizar(const char *fuente) {
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Arena a;
    arena_iniciar(&a, 4096);
    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);

    Resumen r = {0};

    if (p.tuvo_error) {
        fprintf(stderr, "ERROR: el fuente de test no parsea:\n%s\n", fuente);
        fallos++;
        arena_destruir(&a);
        return r;
    }

    LinterResultado lr = linter_analizar(sents, n);
    r.n_warnings = lr.n;
    for (int i = 0; i < lr.n; i++) {
        switch (lr.avisos[i].tipo) {
            case LINT_UNREACHABLE:     r.n_unreachable++; break;
            case LINT_REDUNDANT_PASAR: r.n_redundant_pasar++; break;
            case LINT_EQ_NULO:         r.n_eq_nulo++; break;
            case LINT_UNUSED_IMPORT:   r.n_unused_import++; break;
        }
    }
    linter_resultado_destruir(&lr);
    arena_destruir(&a);
    return r;
}

int main(void) {
    /* ─── UNREACHABLE ─── */
    {
        Resumen r = analizar(
            "funcion f():\n"
            "    retornar 1\n"
            "    imprimir(\"x\")\n"
            "fin funcion\n");
        AFIRMAR(r.n_unreachable == 1 && r.n_warnings == 1, "unreachable_tras_retornar");
    }
    {
        Resumen r = analizar(
            "para i en rango(10):\n"
            "    romper\n"
            "    imprimir(i)\n"
            "fin para\n");
        AFIRMAR(r.n_unreachable == 1, "unreachable_tras_romper");
    }
    {
        Resumen r = analizar(
            "funcion f():\n"
            "    lanzar Excepcion(\"x\")\n"
            "    imprimir(\"x\")\n"
            "fin funcion\n");
        AFIRMAR(r.n_unreachable == 1, "unreachable_tras_lanzar");
    }
    {
        Resumen r = analizar(
            "funcion f():\n"
            "    retornar 1\n"
            "fin funcion\n");
        AFIRMAR(r.n_unreachable == 0, "no_unreachable_si_ultimo");
    }

    /* ─── REDUNDANT_PASAR ─── */
    {
        Resumen r = analizar(
            "si verdadero:\n"
            "    pasar\n"
            "    imprimir(\"x\")\n"
            "fin si\n");
        AFIRMAR(r.n_redundant_pasar == 1, "pasar_redundante");
    }
    {
        Resumen r = analizar(
            "clase X:\n"
            "    pasar\n"
            "fin clase\n");
        AFIRMAR(r.n_redundant_pasar == 0, "pasar_solo_no_warn");
    }

    /* ─── EQ_NULO ─── */
    {
        Resumen r = analizar(
            "x = 1\n"
            "si x == nulo:\n"
            "    pasar\n"
            "fin si\n");
        AFIRMAR(r.n_eq_nulo == 1, "x_eq_nulo");
    }
    {
        Resumen r = analizar(
            "x = 1\n"
            "si x != nulo:\n"
            "    pasar\n"
            "fin si\n");
        AFIRMAR(r.n_eq_nulo == 1, "x_neq_nulo");
    }
    {
        Resumen r = analizar(
            "x = 1\n"
            "si nulo == x:\n"
            "    pasar\n"
            "fin si\n");
        AFIRMAR(r.n_eq_nulo == 1, "nulo_eq_x_lado_izq");
    }
    {
        Resumen r = analizar(
            "x = 1\n"
            "si x es nulo:\n"
            "    pasar\n"
            "fin si\n");
        AFIRMAR(r.n_eq_nulo == 0, "es_nulo_no_warn");
    }

    /* ─── UNUSED_IMPORT ─── */
    {
        Resumen r = analizar(
            "importar matematicas\n"
            "imprimir(\"hola\")\n");
        AFIRMAR(r.n_unused_import == 1, "import_no_usado");
    }
    {
        Resumen r = analizar(
            "importar matematicas\n"
            "imprimir(matematicas.PI)\n");
        AFIRMAR(r.n_unused_import == 0, "import_usado");
    }
    {
        Resumen r = analizar(
            "importar matematicas como mat\n"
            "imprimir(mat.PI)\n");
        AFIRMAR(r.n_unused_import == 0, "import_alias_usado");
    }
    {
        Resumen r = analizar(
            "importar matematicas como mat\n"
            "imprimir(\"hola\")\n");
        AFIRMAR(r.n_unused_import == 1, "import_alias_no_usado");
    }
    {
        Resumen r = analizar(
            "desde matematicas importar PI, factorial\n"
            "imprimir(PI)\n");
        AFIRMAR(r.n_unused_import == 1, "desde_un_no_usado");
    }
    {
        Resumen r = analizar(
            "desde matematicas importar PI\n"
            "imprimir(PI)\n");
        AFIRMAR(r.n_unused_import == 0, "desde_usado");
    }

    /* ─── COMBINADO: codigo limpio NO genera avisos ─── */
    {
        Resumen r = analizar(
            "funcion factorial(n):\n"
            "    si n == 0:\n"
            "        retornar 1\n"
            "    fin si\n"
            "    retornar n * factorial(n - 1)\n"
            "fin funcion\n"
            "imprimir(factorial(5))\n");
        AFIRMAR(r.n_warnings == 0, "codigo_limpio_sin_avisos");
    }

    /* ─── COMBINADO: programa con TODOS los tipos de aviso ─── */
    {
        Resumen r = analizar(
            "importar fechas\n"            /* unused */
            "funcion f(x):\n"
            "    si x == nulo:\n"          /* eq_nulo */
            "        retornar 0\n"
            "    fin si\n"
            "    retornar 1\n"
            "    imprimir(\"x\")\n"        /* unreachable */
            "fin funcion\n"
            "si verdadero:\n"
            "    pasar\n"
            "    imprimir(\"y\")\n"        /* redundant pasar */
            "fin si\n"
            "imprimir(f(0))\n");
        AFIRMAR(r.n_warnings == 4, "combinado_total");
        AFIRMAR(r.n_unused_import == 1, "combinado_unused");
        AFIRMAR(r.n_eq_nulo == 1, "combinado_eqnulo");
        AFIRMAR(r.n_unreachable == 1, "combinado_unreachable");
        AFIRMAR(r.n_redundant_pasar == 1, "combinado_pasar");
    }

    if (fallos == 0) {
        printf("linter: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "linter: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
