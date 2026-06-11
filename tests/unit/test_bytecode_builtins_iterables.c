/*
 * Tests de suma/minimo/maximo/cualquiera/todos como builtins globales
 * (v1.194). Completa la familia iniciada con enumerar (v1.192) y
 * juntar (v1.193).
 *
 * - suma(it, inicial=0): acumula con `+` despachado (bignum,
 *   decimales, cadenas con inicial="").
 * - minimo/maximo: forma iterable (1 arg) y escalar (2+ args),
 *   semantica Python min/max.
 * - cualquiera/todos: any/all con corto-circuito.
 *   cualquiera([]) = falso, todos([]) = verdadero.
 *
 * Las versiones con `clave=` siguen en funcionales (callables no
 * invocables desde nativas C).
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
        "test_builtins_iterables_out.txt";
#else
        "/tmp/test_builtins_iterables_out.txt";
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
    /* suma basica + rango + inicial */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(suma([1, 2, 3, 4]))\n"
            "imprimir(suma(rango(101)))\n"
            "imprimir(suma([], 100))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10\n5050\n100") != NULL, "suma_basica");
    }

    /* suma con bignum */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(suma([10**20, 10**20]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "200000000000000000000") != NULL, "suma_bignum");
    }

    /* suma de cadenas con inicial */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(suma([\"a\", \"b\", \"c\"], \"\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "abc") != NULL, "suma_cadenas");
    }

    /* minimo/maximo forma iterable */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(minimo([3, 1, 4, 1, 5]))\n"
            "imprimir(maximo([3, 1, 4, 1, 5]))\n"
            "imprimir(minimo(rango(5, 15)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1\n5\n5") != NULL, "extremos_iterable");
    }

    /* minimo/maximo forma escalar (2+ args) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(minimo(2, 7))\n"
            "imprimir(maximo(2, 7, 4))\n"
            "imprimir(minimo(-1, -5, 3))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2\n7\n-5") != NULL, "extremos_escalar");
    }

    /* Comparacion lexicografica de cadenas */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(minimo(\"pera\", \"manzana\"))\n"
            "imprimir(maximo([\"a\", \"z\", \"m\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "manzana\nz") != NULL, "cadenas_lex");
    }

    /* minimo de vacio -> ErrorDeValor */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    minimo([])\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "minimo_vacio");
    }

    /* cualquiera / todos basicos */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(cualquiera([falso, falso, verdadero]))\n"
            "imprimir(cualquiera([falso, falso]))\n"
            "imprimir(todos([verdadero, verdadero]))\n"
            "imprimir(todos([verdadero, falso]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso\nverdadero\nfalso") != NULL,
                "any_all_basico");
    }

    /* Semantica de vacios (paridad Python) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(cualquiera([]))\n"
            "imprimir(todos([]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "falso\nverdadero") != NULL, "vacios");
    }

    /* Truthiness de no-booleanos */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(cualquiera([0, 0, 3]))\n"
            "imprimir(todos([1, 2, \"\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso") != NULL, "truthiness");
    }

    /* Modulos stdlib siguen funcionando (clave= en funcionales,
     * 2-arg escalar en matematicas) */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.minimo([9, 2], clave=lambda x: -x))\n"
            "importar matematicas\n"
            "imprimir(matematicas.maximo(1, 2))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "9\n2") != NULL, "modulos_compat");
    }

    /* suma de tipos incompatibles -> error atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    suma([1, \"x\"])\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "suma_tipos_mixtos");
    }

    if (fallos == 0) {
        printf("builtins_iterables: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "builtins_iterables: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
