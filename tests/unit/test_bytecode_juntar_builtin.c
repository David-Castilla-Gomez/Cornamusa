/*
 * Tests de `juntar(*iterables)` como builtin global (v1.193).
 *
 * Es el zip de Python. Antes solo en stdlib/iteradores (requeria
 * importar). Companero natural de enumerar (v1.192).
 *
 * Devuelve lista de tuplas; se detiene en el iterable mas corto.
 * Sin args devuelve []. Eager.
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
        "test_juntar_builtin_out.txt";
#else
        "/tmp/test_juntar_builtin_out.txt";
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
    /* Dos listas con destructuring en para */
    {
        char out[256];
        ejecutar_capturando(
            "para a, b en juntar([1, 2, 3], [\"x\", \"z\", \"w\"]):\n"
            "    imprimir(a, b)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 x\n2 z\n3 w") != NULL, "dos_listas");
    }

    /* Corta en el mas corto */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(juntar([1, 2, 3, 4], [\"a\", \"b\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, \"a\"), (2, \"b\")]") != NULL,
                "corta_en_menor");
    }

    /* Tres iterables mixtos */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(juntar([1, 2], \"ab\", (verdadero, falso)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, \"a\", verdadero), (2, \"b\", falso)]") != NULL,
                "tres_mixtos");
    }

    /* Sin args */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(juntar())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "sin_args");
    }

    /* Un solo iterable: tuplas de 1 */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(juntar([5, 6]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(5,), (6,)]") != NULL, "uno_solo");
    }

    /* Combinado con enumerar */
    {
        char out[256];
        ejecutar_capturando(
            "para i, par en enumerar(juntar(\"xy\", [10, 20])):\n"
            "    imprimir(i, par)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0 (\"x\", 10)\n1 (\"y\", 20)") != NULL,
                "con_enumerar");
    }

    /* Con rango */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(juntar(rango(3), [\"a\", \"b\", \"c\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(0, \"a\"), (1, \"b\"), (2, \"c\")]") != NULL,
                "con_rango");
    }

    /* Iterable vacio en la mezcla -> [] */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(juntar([1, 2], []))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]") != NULL, "con_vacio");
    }

    /* Modulo iteradores sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "importar iteradores\n"
            "imprimir(iteradores.juntar([1], [\"m\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, \"m\")]") != NULL, "modulo_compat");
    }

    /* Tipo no iterable -> error atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    juntar([1], 42)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "no_iterable");
    }

    if (fallos == 0) {
        printf("juntar_builtin: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "juntar_builtin: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
