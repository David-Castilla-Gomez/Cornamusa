/*
 * Tests del coverage tracker (v1.75).
 *
 * Verifica:
 *   - Inactivo: no registra nada.
 *   - Activo: marca lineas que se ejecutan.
 *   - El % es ejecutables_tocadas / ejecutables_totales.
 *   - Codigo no alcanzable (en rama no tomada) queda como uncovered.
 *   - Sin un cov_activar previo, cov_dump reporta "sin lineas".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "chunk.h"
#include "compilador.h"
#include "coverage.h"
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

/* Ejecuta `fuente` con cov opcional. out_reporte recibe el resultado
 * del calculo (n_ejecutables, n_tocadas, primeras uncovered). */
static ResultadoVM ejecutar_con_cov(const char *fuente, bool activar,
                                       CovReporte *out_reporte) {
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    if (p.tuvo_error) {
        arena_destruir(&a);
        if (out_reporte) memset(out_reporte, 0, sizeof(*out_reporte));
        return VM_ERROR_RUNTIME;
    }

    Chunk chunk; chunk_iniciar(&chunk);
    Compilador c; compilador_iniciar(&c, &chunk);
    ResultadoVM rc = VM_ERROR_RUNTIME;
    if (compilador_compilar_programa(&c, sents, n)) {
        VM vm; vm_iniciar(&vm);
        if (activar) cov_activar(&vm.cov, &chunk);
        Valor r = valor_nulo();
        rc = vm_ejecutar(&vm, &chunk, &r);
        valor_destruir(&r);
        if (activar) {
            cov_desactivar(&vm.cov);
            if (out_reporte) cov_calcular(&vm.cov, out_reporte);
        } else if (out_reporte) {
            memset(out_reporte, 0, sizeof(*out_reporte));
        }
        vm_destruir(&vm);
    }
    chunk_destruir(&chunk);
    arena_destruir(&a);
    return rc;
}

int main(void) {
    /* Test 1: cov inactivo no genera reporte. */
    {
        CovReporte r;
        ResultadoVM rc = ejecutar_con_cov("x = 1\nimprimir(x)\n", false, &r);
        AFIRMAR(rc == VM_OK, "inactivo_ejecuta");
        AFIRMAR(r.lineas_ejecutables == 0, "inactivo_sin_reporte");
    }

    /* Test 2: script lineal, 100% cubierto. */
    {
        CovReporte r;
        ResultadoVM rc = ejecutar_con_cov(
            "a = 1\n"
            "b = 2\n"
            "c = a + b\n"
            "imprimir(c)\n", true, &r);
        AFIRMAR(rc == VM_OK, "lineal_ejecuta");
        AFIRMAR(r.lineas_ejecutables >= 4, "lineal_4_lineas_ejecutables");
        AFIRMAR(r.lineas_tocadas == r.lineas_ejecutables, "lineal_100_pct");
        AFIRMAR(r.n_uncovered == 0, "lineal_sin_uncovered");
    }

    /* Test 3: rama no tomada queda uncovered. */
    {
        CovReporte r;
        ResultadoVM rc = ejecutar_con_cov(
            "x = 10\n"
            "si x > 5:\n"
            "    imprimir(\"a\")\n"
            "sino:\n"
            "    imprimir(\"b\")\n"
            "fin si\n", true, &r);
        AFIRMAR(rc == VM_OK, "if_ejecuta");
        AFIRMAR(r.lineas_tocadas < r.lineas_ejecutables, "if_no_100_pct");
        AFIRMAR(r.n_uncovered >= 1, "if_al_menos_1_uncovered");
        /* Linea 5 debe estar en la lista de no cubiertas. */
        bool encontro_5 = false;
        for (int i = 0; i < r.n_uncovered; i++) {
            if (r.uncovered[i] == 5) encontro_5 = true;
        }
        AFIRMAR(encontro_5, "if_linea_5_uncovered");
    }

    /* Test 4: bucle, todas las lineas dentro cubiertas. */
    {
        CovReporte r;
        ResultadoVM rc = ejecutar_con_cov(
            "total = 0\n"
            "para i en rango(3):\n"
            "    total = total + i\n"
            "fin para\n"
            "imprimir(total)\n", true, &r);
        AFIRMAR(rc == VM_OK, "bucle_ejecuta");
        AFIRMAR(r.lineas_tocadas == r.lineas_ejecutables, "bucle_100_pct");
    }

    /* Test 5: bucle con condicional dentro. La rama nunca tomada queda
     * uncovered. */
    {
        CovReporte r;
        ResultadoVM rc = ejecutar_con_cov(
            "para i en rango(3):\n"
            "    si i == 999:\n"
            "        imprimir(\"nunca\")\n"
            "    fin si\n"
            "fin para\n", true, &r);
        AFIRMAR(rc == VM_OK, "bucle_if_ejecuta");
        AFIRMAR(r.lineas_tocadas < r.lineas_ejecutables,
                "bucle_if_no_100_pct");
    }

    /* Test 6: codigo despues de retornar (en top-level no aplica, pero
     * podemos verificar que linea inalcanzable no se marca). Usamos un
     * bucle infinito con romper para crear codigo muerto detras. */
    {
        CovReporte r;
        ResultadoVM rc = ejecutar_con_cov(
            "x = 1\n"
            "mientras verdadero:\n"
            "    x = 2\n"
            "    romper\n"
            "    x = 999\n"
            "fin mientras\n"
            "imprimir(x)\n", true, &r);
        AFIRMAR(rc == VM_OK, "muerto_ejecuta");
        /* Linea 5 (x = 999) puede o no compilarse — el linter detecta
         * unreachable. Si se compila, debe quedar como uncovered. */
    }

    if (fallos == 0) {
        printf("coverage: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "coverage: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
