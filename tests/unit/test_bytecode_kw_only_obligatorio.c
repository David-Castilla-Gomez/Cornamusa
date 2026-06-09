/*
 * Tests de kw-only obligatorios sin default (v1.184).
 *
 * Paridad Python `def f(*args, kw)`: kw es obligatorio por keyword.
 * Llamarlo sin keyword da error claro.
 *
 * Restriccion v1.184: kw-only obligatorios deben ir ANTES de los
 * con default en la firma.
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
        "test_kw_only_obligatorio_out.txt";
#else
        "/tmp/test_kw_only_obligatorio_out.txt";
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
    /* Funcion con kw-only obligatorio */
    {
        char out[256];
        ejecutar_capturando(
            "funcion enviar(*args, destino, copia=falso):\n"
            "    imprimir(destino, copia, longitud(args))\n"
            "fin funcion\n"
            "enviar(\"hola\", destino=\"ana@ej.com\")\n"
            "enviar(\"hola\", destino=\"ana@ej.com\", copia=verdadero)\n"
            "enviar(\"a\", \"b\", destino=\"x@y.com\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ana@ej.com falso 1") != NULL, "default");
        AFIRMAR(strstr(out, "ana@ej.com verdadero 1") != NULL, "override");
        AFIRMAR(strstr(out, "x@y.com falso 2") != NULL, "varios_args");
    }

    /* Sin keyword obligatorio: error claro */
    {
        char out[512];
        ejecutar_capturando(
            "funcion enviar(*args, destino):\n"
            "    imprimir(destino)\n"
            "fin funcion\n"
            "funcion p():\n"
            "    intentar:\n"
            "        enviar(\"hola\")\n"
            "    atrapar ErrorDeTipo como e:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "sin_obl_error");
    }

    /* Multiples kw-only obligatorios */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f(*xs, alfa, beta):\n"
            "    imprimir(alfa, beta, longitud(xs))\n"
            "fin funcion\n"
            "f(1, 2, alfa=10, beta=20)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10 20 2") != NULL, "multi_obl");
    }

    /* Mezcla obligatorios + con default */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f(*xs, obl, opc=99):\n"
            "    imprimir(obl, opc, longitud(xs))\n"
            "fin funcion\n"
            "f(1, obl=10)\n"
            "f(1, obl=10, opc=20)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10 99 1") != NULL, "mezcla_default");
        AFIRMAR(strstr(out, "10 20 1") != NULL, "mezcla_override");
    }

    /* Lambda con kw-only obligatorio */
    {
        char out[256];
        ejecutar_capturando(
            "f = lambda *xs, factor: longitud(xs) * factor\n"
            "imprimir(f(1, 2, 3, factor=10))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "30") != NULL, "lambda_obl");
    }

    /* Faltar un obligatorio entre varios */
    {
        char out[512];
        ejecutar_capturando(
            "funcion f(*xs, a, b):\n"
            "    imprimir(a, b)\n"
            "fin funcion\n"
            "funcion p():\n"
            "    intentar:\n"
            "        f(1, a=10)\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok-faltab\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok-faltab") != NULL, "falta_b");
    }

    /* Fijos + *args + obl + con-def + **kw */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f(pref, *xs, obl, opc=99, **kw):\n"
            "    imprimir(pref, longitud(xs), obl, opc, longitud(kw))\n"
            "fin funcion\n"
            "f(\"p\", 1, 2, 3, obl=10, extra=\"x\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "p 3 10 99 1") != NULL, "completo");
    }

    if (fallos == 0) {
        printf("kw_only_obligatorio: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "kw_only_obligatorio: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
