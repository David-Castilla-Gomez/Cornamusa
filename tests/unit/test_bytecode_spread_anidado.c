/*
 * Tests de spread con comprehensions y genex inline (v1.177).
 *
 * Cierra la ultima limitacion documentada en v1.171:
 *   [*[x para x en xs], 99]   antes daba "estado interno corrupto"
 *
 * Root cause: el path spread mantenia la lista temporal como valor
 * "huerfano" en el stack sin reservar slot local. Las comprehensions
 * interiores reservaban slots a partir de n_locales, descuadrando
 * el indice calculado en compilacion respecto a la altura runtime
 * del stack.
 *
 * Fix: por cada elemento del literal con spread, reservar un slot
 * local $spread_tmp antes de compilar la sub-expresion. Asi el
 * n_locales del compilador coincide con la altura real del stack
 * durante toda la sub-compilacion.
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
        "test_spread_anidado_out.txt";
#else
        "/tmp/test_spread_anidado_out.txt";
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
    /* Caso de v1.171 que rompia */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([*[x*x para x en rango(4)], 100])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 4, 9, 100]") != NULL,
                "comprehension_en_spread_lista");
    }

    /* Comprehension en spread de conjunto */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud({*[x para x en rango(5)]}))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "comprehension_en_spread_conjunto");
    }

    /* Genex en spread de tupla */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir((*[10, 20], *(x*2 para x en rango(3)), 99))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(10, 20, 0, 2, 4, 99)") != NULL,
                "genex_en_spread_tupla");
    }

    /* Dict comprehension en dspread */
    {
        char out[256];
        ejecutar_capturando(
            "d = {**{x: x*x para x en rango(3)}, \"extra\": 99}\n"
            "imprimir(d[0])\n"
            "imprimir(d[1])\n"
            "imprimir(d[2])\n"
            "imprimir(d[\"extra\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0\n1\n4\n99") != NULL,
                "dict_comp_en_dspread");
    }

    /* Spread anidado en spread (sin comprehension) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([*[[*[1, 2]], *[3, 4]]])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[[1, 2], 3, 4]") != NULL, "spread_anidado");
    }

    /* Multiples comprehensions en mismo literal */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [*[a para a en rango(3)], *[b*10 para b en rango(3)]]\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 2, 0, 10, 20]") != NULL,
                "dos_comprehensions");
    }

    /* Comprehension con guarda */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([0, *[x para x en rango(10) si x % 2 == 0], 99])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 0, 2, 4, 6, 8, 99]") != NULL,
                "comprehension_con_guarda");
    }

    /* Casos viejos siguen funcionando (regresion) */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "imprimir([*xs, 4])\n"
            "imprimir((*xs, 4))\n"
            "imprimir({*xs, 4})\n"
            "imprimir({**{\"a\": 1}, \"b\": 2})\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4]") != NULL, "regr_lista");
        AFIRMAR(strstr(out, "(1, 2, 3, 4)") != NULL, "regr_tupla");
        AFIRMAR(strstr(out, "{") != NULL && strstr(out, "1") != NULL,
                "regr_conjunto_y_dicc");
    }

    /* Spread dentro de funcion (frame con parametros) */
    {
        char out[256];
        ejecutar_capturando(
            "funcion combinar(a, b):\n"
            "    retornar [*a, *[x para x en b]]\n"
            "fin funcion\n"
            "imprimir(combinar([1, 2], [3, 4]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4]") != NULL, "en_funcion");
    }

    if (fallos == 0) {
        printf("spread_anidado: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "spread_anidado: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
