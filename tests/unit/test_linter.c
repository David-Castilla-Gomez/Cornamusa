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
    int n_unused_local;
    int n_unused_param;
    int n_shadow;
    int n_unused_loop_var;
    int n_mutable_default;
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
            case LINT_UNUSED_LOCAL:    r.n_unused_local++; break;
            case LINT_UNUSED_PARAM:    r.n_unused_param++; break;
            case LINT_SHADOW:          r.n_shadow++; break;
            case LINT_UNUSED_LOOP_VAR: r.n_unused_loop_var++; break;
            case LINT_MUTABLE_DEFAULT: r.n_mutable_default++; break;
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

    /* ─── UNUSED_LOCAL (v1.50) ─── */
    {
        Resumen r = analizar(
            "funcion f():\n"
            "    x = 1\n"
            "    z = 2\n"
            "    retornar x\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_local == 1, "local_no_usada");
    }
    {
        Resumen r = analizar(
            "funcion f():\n"
            "    x = 1\n"
            "    x = x + 1\n"
            "    retornar x\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_local == 0, "reasignacion_uso");
    }
    {
        Resumen r = analizar(
            "funcion f():\n"
            "    _ignorada = 99\n"
            "    retornar 1\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_local == 0, "underscore_skip");
    }
    {
        Resumen r = analizar(
            "funcion par():\n"
            "    retornar (1, 2)\n"
            "fin funcion\n"
            "funcion f():\n"
            "    a, b = par()\n"
            "    retornar a\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_local == 1, "destructuring_uno_no_usado");
    }
    {
        /* Variables module-level no se warnean (scope nulo). */
        Resumen r = analizar("x = 1\nz = 2\n");
        AFIRMAR(r.n_unused_local == 0, "module_level_no_warn");
    }

    /* ─── UNUSED_PARAM (v1.50) ─── */
    {
        Resumen r = analizar(
            "funcion f(a, b, c):\n"
            "    retornar a + c\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_param == 1, "param_no_usado");
    }
    {
        Resumen r = analizar(
            "funcion m(yo, x):\n"
            "    retornar x\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_param == 0, "yo_skip");
    }
    {
        Resumen r = analizar(
            "funcion f(_, x):\n"
            "    retornar x\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_param == 0, "underscore_param_skip");
    }
    {
        Resumen r = analizar(
            "funcion f(a, *args, **kw):\n"
            "    retornar a\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_param == 0, "varargs_skip");
    }
    {
        Resumen r = analizar(
            "f = lambda x, z: x + 1\n"
            "imprimir(f(1, 2))\n");
        AFIRMAR(r.n_unused_param == 1, "lambda_param_no_usado");
    }

    /* ─── nolocal/global no warnean ─── */
    {
        Resumen r = analizar(
            "funcion contador():\n"
            "    n = 0\n"
            "    funcion inc():\n"
            "        nolocal n\n"
            "        n = n + 1\n"
            "        retornar n\n"
            "    fin funcion\n"
            "    retornar inc\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_local == 0, "closure_nolocal_marca_outer");
        AFIRMAR(r.n_unused_param == 0, "closure_sin_unused_param");
    }
    {
        Resumen r = analizar(
            "funcion f():\n"
            "    global x\n"
            "    x = 99\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_local == 0, "global_no_warn");
    }

    /* ─── Closures capturan: la outer var SE marca usada ─── */
    {
        Resumen r = analizar(
            "funcion outer():\n"
            "    valor = 42\n"
            "    funcion inner():\n"
            "        retornar valor + 1\n"
            "    fin funcion\n"
            "    retornar inner\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_local == 0, "closure_lee_outer");
    }

    /* ─── SHADOW (v1.55) ─── */
    {
        Resumen r = analizar(
            "funcion outer():\n"
            "    x = 1\n"
            "    funcion inner():\n"
            "        x = 2\n"
            "        retornar x\n"
            "    fin funcion\n"
            "    retornar inner() + x\n"
            "fin funcion\n");
        AFIRMAR(r.n_shadow == 1, "shadow_local");
    }
    {
        Resumen r = analizar(
            "funcion outer(a):\n"
            "    funcion inner(a):\n"
            "        retornar a\n"
            "    fin funcion\n"
            "    retornar inner(a)\n"
            "fin funcion\n");
        AFIRMAR(r.n_shadow == 1, "shadow_param");
    }
    {
        /* nolocal NO genera shadow. */
        Resumen r = analizar(
            "funcion outer():\n"
            "    n = 0\n"
            "    funcion inner():\n"
            "        nolocal n\n"
            "        n = n + 1\n"
            "    fin funcion\n"
            "    inner()\n"
            "    retornar n\n"
            "fin funcion\n");
        AFIRMAR(r.n_shadow == 0, "nolocal_no_shadow");
    }
    {
        /* `_` no genera shadow. */
        Resumen r = analizar(
            "funcion outer():\n"
            "    _ = 1\n"
            "    funcion inner():\n"
            "        _ = 2\n"
            "        retornar 3\n"
            "    fin funcion\n"
            "    retornar inner()\n"
            "fin funcion\n");
        AFIRMAR(r.n_shadow == 0, "underscore_no_shadow");
    }

    /* ─── UNUSED_LOOP_VAR (v1.55) ─── */
    {
        Resumen r = analizar(
            "funcion f():\n"
            "    para i en rango(10):\n"
            "        imprimir(\"ping\")\n"
            "    fin para\n"
            "    retornar 0\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_loop_var == 1, "loop_var_no_usado");
    }
    {
        Resumen r = analizar(
            "funcion f():\n"
            "    para i en rango(10):\n"
            "        imprimir(i)\n"
            "    fin para\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_loop_var == 0, "loop_var_usado");
    }
    {
        /* `_` como loop var no warnea. */
        Resumen r = analizar(
            "funcion f():\n"
            "    para _ en rango(10):\n"
            "        imprimir(\"x\")\n"
            "    fin para\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_loop_var == 0, "loop_underscore");
    }
    {
        /* Loop var usada DESPUES del loop (semantica Python) cuenta. */
        Resumen r = analizar(
            "funcion f():\n"
            "    para i en rango(10):\n"
            "        pasar\n"
            "    fin para\n"
            "    retornar i\n"
            "fin funcion\n");
        AFIRMAR(r.n_unused_loop_var == 0, "loop_usada_post_loop");
    }

    /* ─── MUTABLE_DEFAULT (v1.55) ─── */
    {
        Resumen r = analizar(
            "funcion f(items=[]):\n"
            "    retornar items\n"
            "fin funcion\n");
        AFIRMAR(r.n_mutable_default == 1, "mutable_lista");
    }
    {
        Resumen r = analizar(
            "funcion f(cache={}):\n"
            "    retornar cache\n"
            "fin funcion\n");
        AFIRMAR(r.n_mutable_default == 1, "mutable_dict");
    }
    {
        /* Defaults inmutables: no warna. */
        Resumen r = analizar(
            "funcion f(n=0, s=\"\", b=verdadero):\n"
            "    retornar n\n"
            "fin funcion\n");
        AFIRMAR(r.n_mutable_default == 0, "default_inmutable_ok");
    }
    {
        Resumen r = analizar(
            "g = lambda items=[]: items\n"
            "imprimir(g())\n");
        AFIRMAR(r.n_mutable_default == 1, "lambda_mutable_default");
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
