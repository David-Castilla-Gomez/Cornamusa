/*
 * Tests de lambdas con parametros keyword-only (v1.183).
 *
 * Paridad con funcion en v1.182: lambda *args, kw=default: ... soportado.
 * Llamada posicional pasa extras a *args; kw-only usa default.
 * Llamada con keyword sobrescribe el default.
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
        "test_lambda_kw_only_out.txt";
#else
        "/tmp/test_lambda_kw_only_out.txt";
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
    /* lambda *args + kw-only */
    {
        char out[256];
        ejecutar_capturando(
            "f = lambda *args, inicial=0: inicial + longitud(args)\n"
            "imprimir(f(1, 2, 3))\n"
            "imprimir(f(1, 2, 3, inicial=10))\n"
            "imprimir(f(inicial=100))\n"
            "imprimir(f())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3\n13\n100\n0") != NULL, "lambda_basico");
    }

    /* lambda con fijo + *args + kw-only */
    {
        char out[256];
        ejecutar_capturando(
            "f = lambda nombre, *vals, sep=\"-\": nombre + \":\" + sep + cadena(longitud(vals))\n"
            "imprimir(f(\"a\", 10, 20))\n"
            "imprimir(f(\"b\", 1, 2, sep=\"|\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "a:-2") != NULL, "fijo_default");
        AFIRMAR(strstr(out, "b:|2") != NULL, "fijo_override");
    }

    /* lambda con multiples kw-only */
    {
        char out[256];
        ejecutar_capturando(
            "f = lambda *xs, a=1, b=2, c=3: a + b + c + longitud(xs)\n"
            "imprimir(f())\n"
            "imprimir(f(99))\n"
            "imprimir(f(a=10))\n"
            "imprimir(f(99, b=20))\n"
            "imprimir(f(1, 2, 3, c=30))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "6\n7\n15\n25\n36") != NULL, "multi_kw_only");
    }

    /* lambda en posicion de callback */
    {
        char out[256];
        ejecutar_capturando(
            "funcion aplicar(f, *xs):\n"
            "    retornar f(*xs)\n"
            "fin funcion\n"
            "imprimir(aplicar(lambda *xs, mult=2: longitud(xs) * mult, 1, 2, 3))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "6") != NULL, "como_callback");
    }

    if (fallos == 0) {
        printf("lambda_kw_only: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "lambda_kw_only: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
