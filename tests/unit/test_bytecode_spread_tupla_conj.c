/*
 * Tests de spread `*xs` en literales de tupla y conjunto (v1.172).
 *
 * Continúa v1.171 (que cubrió listas). Implementación:
 *   - Tupla: construye lista incremental con OP_LISTA_AGREGAR /
 *     OP_LISTA_EXTENDER, y convierte al final con
 *     OP_LISTA_A_TUPLA.
 *   - Conjunto: análogo con OP_BUILD_CONJUNTO 0 +
 *     OP_CONJUNTO_AGREGAR / OP_CONJUNTO_EXTENDER (nuevo opcode).
 *
 * Dicc con `**d` queda pendiente para v1.173.
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
        "test_spread_tupla_conj_out.txt";
#else
        "/tmp/test_spread_tupla_conj_out.txt";
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
    /* Tupla con spread */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "imprimir((0, *xs, 4))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(0, 1, 2, 3, 4)") != NULL, "tupla_basico");
    }

    /* Tupla con spread de tupla */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir((1, *(10, 20), 99))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 10, 20, 99)") != NULL, "tupla_de_tupla");
    }

    /* Tupla con spread de cadena */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir((1, *\"ab\", 2))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, \"a\", \"b\", 2)") != NULL, "tupla_cadena");
    }

    /* Tupla con spread de rango */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir((-1, *rango(5), 100))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(-1, 0, 1, 2, 3, 4, 100)") != NULL, "tupla_rango");
    }

    /* Conjunto con spread */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "s = {0, *xs, 4}\n"
            "imprimir(longitud(s))\n"
            "imprimir(0 en s)\n"
            "imprimir(2 en s)\n"
            "imprimir(4 en s)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5\nverdadero\nverdadero\nverdadero") != NULL,
                "conjunto_basico");
    }

    /* Conjunto deduplica spread */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud({1, *[1, 2, 3], 2}))\n",
            out, sizeof(out));
        /* {1, 2, 3} = 3 elementos */
        AFIRMAR(strstr(out, "3") != NULL, "conjunto_dedup");
    }

    /* Conjunto con spread de cadena */
    {
        char out[256];
        ejecutar_capturando(
            "s = {0, *\"hola\"}\n"
            "imprimir(longitud(s))\n"
            "imprimir(\"h\" en s)\n"
            "imprimir(\"o\" en s)\n",
            out, sizeof(out));
        /* 0, h, o, l, a = 5 */
        AFIRMAR(strstr(out, "5\nverdadero\nverdadero") != NULL,
                "conjunto_cadena");
    }

    /* Conjunto con elementos no hashables en spread -> ErrorDeTipo */
    {
        char out[256];
        ejecutar_capturando(
            "funcion p():\n"
            "    intentar:\n"
            "        s = {1, *[[1, 2]]}\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "conjunto_no_hashable");
    }

    /* Tupla sin spread sigue funcionando (path eficiente) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir((1, 2, 3))\n"
            "imprimir((42,))\n"
            "imprimir(())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 2, 3)\n(42,)\n()") != NULL,
                "tupla_sin_spread");
    }

    /* Conjunto sin spread sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud({1, 2, 3}))\n"
            "imprimir(longitud({1, 1, 2}))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3\n2") != NULL, "conjunto_sin_spread");
    }

    /* Multiples spreads */
    {
        char out[256];
        ejecutar_capturando(
            "a = (1, 2)\n"
            "b = (3, 4)\n"
            "imprimir((0, *a, 99, *b))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(0, 1, 2, 99, 3, 4)") != NULL,
                "multiples_spreads");
    }

    if (fallos == 0) {
        printf("spread_tupla_conj: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "spread_tupla_conj: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
