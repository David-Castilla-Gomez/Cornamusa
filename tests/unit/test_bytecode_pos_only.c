/*
 * Tests de parametros posicional-only con `/` (v1.185).
 *
 * Paridad Python 3.8+:
 *   def f(a, b, /, c, d):  # a, b son solo-posicional
 * Llamar `f(a=1, b=2, c=3, d=4)` da error porque a, b no aceptan keyword.
 *
 * Funciona en funcion y lambda. `/` solo permitido antes de `*args`.
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
        "test_pos_only_out.txt";
#else
        "/tmp/test_pos_only_out.txt";
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
    /* Funcion: pos-only + mixto */
    {
        char out[256];
        ejecutar_capturando(
            "funcion saludar(nombre, /, prefijo=\"hola\"):\n"
            "    imprimir(prefijo, nombre)\n"
            "fin funcion\n"
            "saludar(\"Ana\")\n"
            "saludar(\"Ana\", \"buenos dias\")\n"
            "saludar(\"Ana\", prefijo=\"hi\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "hola Ana") != NULL, "default_pref");
        AFIRMAR(strstr(out, "buenos dias Ana") != NULL, "pos_pref");
        AFIRMAR(strstr(out, "hi Ana") != NULL, "kw_pref");
    }

    /* Keyword en pos-only -> error */
    {
        char out[256];
        ejecutar_capturando(
            "funcion saludar(nombre, /, prefijo=\"hola\"):\n"
            "    imprimir(prefijo, nombre)\n"
            "fin funcion\n"
            "funcion p():\n"
            "    intentar:\n"
            "        saludar(nombre=\"Ana\")\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "kw_en_pos_only");
    }

    /* Todos pos-only */
    {
        char out[256];
        ejecutar_capturando(
            "funcion suma(a, b, /):\n"
            "    retornar a + b\n"
            "fin funcion\n"
            "imprimir(suma(3, 4))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "7") != NULL, "todos_pos_only");
    }

    /* Lambda con pos-only */
    {
        char out[256];
        ejecutar_capturando(
            "mul = lambda a, b, /: a * b\n"
            "imprimir(mul(5, 6))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "30") != NULL, "lambda_pos_only");
    }

    /* Combinado: pos-only + normales + *args + kw-only + **kw */
    {
        char out[512];
        ejecutar_capturando(
            "funcion completa(a, b, /, c, d=4, *xs, kw=\"k\", **kwargs):\n"
            "    imprimir(a, b, c, d, longitud(xs), kw, longitud(kwargs))\n"
            "fin funcion\n"
            "completa(1, 2, 3)\n"
            "completa(1, 2, 3, 99, 100, 200, kw=\"hola\", extra=\"x\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 2 3 4 0 k 0") != NULL, "completa_default");
        AFIRMAR(strstr(out, "1 2 3 99 2 hola 1") != NULL, "completa_full");
    }

    /* `/` despues de *args -> error de compilacion */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f(*args, /, kw=1):\n"
            "    pasar\n"
            "fin funcion\n",
            out, sizeof(out));
        /* Compile-time error: no se ejecuta, no hay output. */
        AFIRMAR(strlen(out) == 0, "slash_tras_args");
    }

    /* `/` al inicio -> error */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f(/, a):\n"
            "    pasar\n"
            "fin funcion\n",
            out, sizeof(out));
        AFIRMAR(strlen(out) == 0, "slash_inicial");
    }

    if (fallos == 0) {
        printf("pos_only: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "pos_only: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
