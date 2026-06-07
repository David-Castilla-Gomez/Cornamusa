/*
 * Tests del primer elemento spread en tupla y conjunto literales
 * (v1.174). Completa la limitacion documentada en v1.172:
 * `(*xs,)`, `(*xs, b)`, `{*xs}`, `{*xs, b}` ahora parsean.
 *
 * Implementacion: parsear_grupo detecta TT_ASTERISCO antes del
 * primer elemento; salta el camino "grupo" y va directo al bucle
 * de tupla. parsear_llaves detecta TT_ASTERISCO antes del primer
 * elemento y va directo al bucle de conjunto (no dict, no
 * comprehension).
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
        "test_spread_primer_out.txt";
#else
        "/tmp/test_spread_primer_out.txt";
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
    /* Tupla con primer spread (sola) */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "imprimir((*xs,))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 2, 3)") != NULL, "tupla_solo");
    }

    /* Tupla con primer spread + mas */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "imprimir((*xs, 99))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 2, 3, 99)") != NULL, "tupla_seguido");
    }

    /* Tupla con dos spreads */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2]\n"
            "ys = [3, 4]\n"
            "imprimir((*xs, *ys))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 2, 3, 4)") != NULL, "tupla_dos_spreads");
    }

    /* Tupla con spread de tupla */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir((*(10, 20), 99))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(10, 20, 99)") != NULL, "tupla_de_tupla");
    }

    /* Conjunto con primer spread (solo) */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "s = {*xs}\n"
            "imprimir(longitud(s))\n"
            "imprimir(1 en s)\n"
            "imprimir(3 en s)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3\nverdadero\nverdadero") != NULL, "conjunto_solo");
    }

    /* Conjunto con primer spread + mas */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "s = {*xs, 99}\n"
            "imprimir(longitud(s))\n"
            "imprimir(99 en s)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "4\nverdadero") != NULL, "conjunto_seguido");
    }

    /* Conjunto deduplica */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud({*[1, 1, 2, 3, 2]}))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "conjunto_dedup");
    }

    /* Conjunto con dos spreads */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud({*[1, 2], *[2, 3, 4]}))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "4") != NULL, "conjunto_dos_spreads");
    }

    /* Tupla vacia sigue funcionando (regression) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(())\n"
            "imprimir((42,))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "()\n(42,)") != NULL, "tupla_vacia_y_uno");
    }

    /* Grupo (1 elemento sin coma) sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir((1 + 2) * 3)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "9") != NULL, "grupo_no_es_tupla");
    }

    /* Conjunto con spread de rango */
    {
        char out[256];
        ejecutar_capturando(
            "s = {*rango(5)}\n"
            "imprimir(longitud(s))\n"
            "imprimir(0 en s)\n"
            "imprimir(4 en s)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5\nverdadero\nverdadero") != NULL,
                "conjunto_de_rango");
    }

    if (fallos == 0) {
        printf("spread_primer: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "spread_primer: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
