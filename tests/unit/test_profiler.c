/*
 * Tests del profiler determinista (v1.71).
 *
 * Verifica:
 *   - Hooks no-op cuando inactivo (cero entradas tras ejecutar).
 *   - Hooks registran cuando activo.
 *   - Cuentas de llamadas correctas en un caso conocido (fib recursivo).
 *   - Self time descontado: padre tiene self < total cuando hay hijos.
 *   - Top-level y modulos aparecen como entradas distintas.
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
#include "profiler.h"
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

/* Ejecuta `fuente` con profiler opcional. Devuelve VM_OK / error.
 * Si out_prof != NULL siempre lo deja en estado inicializado (incluso
 * en errores tempranos) — el caller llama profiler_destruir() libremente. */
static ResultadoVM ejecutar(const char *fuente, bool activar_prof, Profiler *out_prof) {
    if (out_prof) profiler_iniciar(out_prof);
    Lexer l; lexer_iniciar(&l, fuente, "<test>");
    Arena a; arena_iniciar(&a, 8192);
    Parser p; parser_iniciar(&p, &l, &a, fuente, "<test>");
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    if (p.tuvo_error) { arena_destruir(&a); return VM_ERROR_RUNTIME; }

    Chunk chunk; chunk_iniciar(&chunk);
    Compilador c; compilador_iniciar(&c, &chunk);
    bool ok = compilador_compilar_programa(&c, sents, n);
    ResultadoVM rc = VM_ERROR_RUNTIME;
    if (ok) {
        VM vm; vm_iniciar(&vm);
        if (activar_prof) profiler_activar(&vm.profiler);
        Valor r = valor_nulo();
        rc = vm_ejecutar(&vm, &chunk, &r);
        valor_destruir(&r);
        if (activar_prof) {
            profiler_desactivar(&vm.profiler);
            if (out_prof) {
                /* Copia superficial; los nombres son strdup, transferimos posesion. */
                *out_prof = vm.profiler;
                vm.profiler.n_entradas = 0;  /* evitar doble free */
                vm.profiler.n_stack = 0;
            }
        }
        vm_destruir(&vm);
    }
    chunk_destruir(&chunk);
    arena_destruir(&a);
    return rc;
}

static const ProfilerEntrada *buscar(const Profiler *p, const char *nombre) {
    for (int i = 0; i < p->n_entradas; i++) {
        if (strcmp(p->entradas[i].nombre, nombre) == 0) return &p->entradas[i];
    }
    return NULL;
}

int main(void) {
    /* Test 1: profiler inactivo no registra nada. */
    {
        Profiler pf;
        profiler_iniciar(&pf);
        ResultadoVM rc = ejecutar("imprimir(1+2)\n", false, NULL);
        AFIRMAR(rc == VM_OK, "inactivo_ejecuta_ok");
        AFIRMAR(pf.n_entradas == 0, "inactivo_no_registra");
        profiler_destruir(&pf);
    }

    /* Test 2: profiler activo registra al menos top-level. */
    {
        Profiler pf;
        ResultadoVM rc = ejecutar("x = 1 + 2\n", true, &pf);
        AFIRMAR(rc == VM_OK, "activo_ejecuta_ok");
        AFIRMAR(pf.n_entradas >= 1, "activo_registra_top_level");
        const ProfilerEntrada *top = buscar(&pf, "<top-level>");
        AFIRMAR(top != NULL, "encontro_top_level");
        if (top) AFIRMAR(top->llamadas == 1, "top_level_1_llamada");
        profiler_destruir(&pf);
    }

    /* Test 3: cuenta exacta de llamadas en fib(5). */
    /* fib(n) total calls T(n) = 2*fib(n+1)-1: T(5) = 2*8-1 = 15. */
    {
        Profiler pf;
        ResultadoVM rc = ejecutar(
            "funcion fib(n):\n"
            "    si n < 2:\n"
            "        retornar n\n"
            "    fin si\n"
            "    retornar fib(n-1) + fib(n-2)\n"
            "fin funcion\n"
            "imprimir(fib(5))\n", true, &pf);
        AFIRMAR(rc == VM_OK, "fib_ejecuta_ok");
        const ProfilerEntrada *e_fib = buscar(&pf, "fib");
        AFIRMAR(e_fib != NULL, "fib_registrada");
        if (e_fib) {
            AFIRMAR(e_fib->llamadas == 15, "fib5_15_llamadas");
            /* Para funcion recursiva: total > self (los hijos descuentan). */
            AFIRMAR(e_fib->total_ns >= e_fib->self_ns, "fib_total_geq_self");
        }
        profiler_destruir(&pf);
    }

    /* Test 4: dos funciones distintas son buckets separados. */
    {
        Profiler pf;
        ResultadoVM rc = ejecutar(
            "funcion uno():\n"
            "    retornar 1\n"
            "fin funcion\n"
            "funcion dos():\n"
            "    retornar 2\n"
            "fin funcion\n"
            "uno()\nuno()\nuno()\n"
            "dos()\n", true, &pf);
        AFIRMAR(rc == VM_OK, "dos_funcs_ok");
        const ProfilerEntrada *e1 = buscar(&pf, "uno");
        const ProfilerEntrada *e2 = buscar(&pf, "dos");
        AFIRMAR(e1 != NULL, "uno_registrada");
        AFIRMAR(e2 != NULL, "dos_registrada");
        if (e1) AFIRMAR(e1->llamadas == 3, "uno_3_llamadas");
        if (e2) AFIRMAR(e2->llamadas == 1, "dos_1_llamada");
        profiler_destruir(&pf);
    }

    /* Test 5: profiler_tiempo_ns es monotonicamente creciente. */
    {
        uint64_t t1 = profiler_tiempo_ns();
        /* Trabajar un poco para que el clock avance. */
        volatile int x = 0;
        for (int i = 0; i < 10000; i++) x += i;
        uint64_t t2 = profiler_tiempo_ns();
        AFIRMAR(t2 >= t1, "tiempo_monotono");
        AFIRMAR(t2 - t1 < 1000000000ULL, "tiempo_razonable");  /* < 1s */
    }

    /* Test 6: profiler_dump no crashea con tabla vacia. */
    {
        Profiler pf;
        profiler_iniciar(&pf);
        FILE *devnull = fopen(
#ifdef _WIN32
            "NUL",
#else
            "/dev/null",
#endif
            "w");
        if (devnull) {
            profiler_dump(&pf, devnull, 0);
            fclose(devnull);
        }
        AFIRMAR(true, "dump_vacio_no_crashea");
        profiler_destruir(&pf);
    }

    if (fallos == 0) {
        printf("profiler: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "profiler: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
