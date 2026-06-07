/*
 * Tests de spread `*xs` en literales de lista (v1.171).
 *
 * `[a, *xs, b]` desempaca el iterable xs en posicion. Funciona con
 * lista, tupla, conjunto, dicc (claves), cadena, rango.
 *
 * Implementacion: cuando el literal contiene al menos un spread,
 * el compilador emite `OP_BUILD_LISTA 0` y luego, por cada
 * elemento, `OP_LISTA_AGREGAR` (normal) u `OP_LISTA_EXTENDER`
 * (spread). El handler de extender se extendio a mas iterables.
 *
 * Tuplas, conjuntos y dicc literales con spread quedan pendientes
 * para releases posteriores.
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
        "test_spread_lista_out.txt";
#else
        "/tmp/test_spread_lista_out.txt";
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
    /* spread basico */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "ys = [0, *xs, 4]\n"
            "imprimir(ys)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 2, 3, 4]") != NULL, "basico");
    }

    /* multiples spreads */
    {
        char out[256];
        ejecutar_capturando(
            "a = [1, 2]\n"
            "b = [3, 4]\n"
            "imprimir([*a, 99, *b, 100])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 99, 3, 4, 100]") != NULL, "multiples");
    }

    /* solo spread */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [1, 2, 3]\n"
            "imprimir([*xs])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "solo_spread");
    }

    /* spread de tupla */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([*(10, 20, 30)])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[10, 20, 30]") != NULL, "tupla");
    }

    /* spread de cadena -> caracteres */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([*\"hola\"])\n",
            out, sizeof(out));
        /* Cada char es una cadena de 1 char */
        AFIRMAR(strstr(out, "[\"h\", \"o\", \"l\", \"a\"]") != NULL, "cadena");
    }

    /* spread de rango */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([*rango(5)])\n"
            "imprimir([*rango(2, 8, 2)])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 2, 3, 4]") != NULL, "rango_basico");
        AFIRMAR(strstr(out, "[2, 4, 6]") != NULL, "rango_paso");
    }

    /* spread de conjunto */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [*{1, 2, 3}]\n"
            "imprimir(longitud(xs))\n"
            "imprimir(1 en xs)\n"
            "imprimir(2 en xs)\n"
            "imprimir(3 en xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3\nverdadero\nverdadero\nverdadero") != NULL,
                "conjunto");
    }

    /* spread de dicc -> claves (paridad Python) */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"a\": 1, \"b\": 2}\n"
            "imprimir([*d])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"b\"]") != NULL, "dicc_claves");
    }

    /* spread vacio */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([*[]])\n"
            "imprimir([1, *[], 2])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]\n[1, 2]") != NULL, "vacio");
    }

    /* Mezcla con variable previa */
    {
        char out[256];
        ejecutar_capturando(
            "cuadrados = [x*x para x en rango(4)]\n"
            "imprimir([*cuadrados, 100])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 4, 9, 100]") != NULL,
                "spread_lista_calculada");
    }

    /* Spread con instancia que NO es iterable -> ErrorDeTipo */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    xs = [*42]\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "no_iterable_error");
    }

    /* spread NO rompe el path normal (sin spread sigue eficiente) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([1, 2, 3])\n"
            "imprimir([])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]\n[]") != NULL, "sin_spread_intacto");
    }

    if (fallos == 0) {
        printf("spread_lista: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "spread_lista: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
