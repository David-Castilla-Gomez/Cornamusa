/*
 * Tests de las nativas `escribir` y `imprimir_error` (v1.148).
 *
 * `imprimir` siempre anade salto de linea final y va a stdout. Para
 * CLIs interactivas y separacion stdout/stderr faltaban dos
 * variantes basicas:
 *
 *   escribir(*args)        — como imprimir SIN \n final. Util para
 *                            prompts, barras de progreso, output
 *                            por trozos.
 *
 *   imprimir_error(*args)  — como imprimir pero a stderr. Idiomatico
 *                            para mensajes de error/aviso sin
 *                            contaminar stdout (que tipicamente
 *                            lleva el resultado del programa para
 *                            pipeear).
 *
 * Ambas son nativas en C (paralelas a imprimir): mismo loop con
 * separador espacio entre args. Sin kwargs por ahora — extender a
 * sep=/fin= como Python requeriria que las nativas acepten kwargs,
 * lo cual es un cambio mayor (las nativas en Cornamusa solo
 * reciben args posicionales).
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

/* Captura stdout y stderr a archivos separados. */
static int ejecutar_capturando(const char *fuente,
                                 char *out_buf, int out_cap,
                                 char *err_buf, int err_cap) {
    const char *tmpfile_out =
#ifdef _WIN32
        "test_escribir_out.txt";
#else
        "/tmp/test_escribir_out.txt";
#endif
    const char *tmpfile_err =
#ifdef _WIN32
        "test_escribir_err.txt";
#else
        "/tmp/test_escribir_err.txt";
#endif
    if (!freopen(tmpfile_out, "w+", stdout)) return -1;
    if (!freopen(tmpfile_err, "w+", stderr)) return -1;

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
    fflush(stderr);
#ifdef _WIN32
    freopen("CON", "w", stdout);
    freopen("CON", "w", stderr);
#else
    freopen("/dev/tty", "w", stdout);
    freopen("/dev/tty", "w", stderr);
#endif

    FILE *f = fopen(tmpfile_out, "r");
    if (f) {
        int leido = (int)fread(out_buf, 1, (size_t)(out_cap - 1), f);
        out_buf[leido] = '\0';
        fclose(f);
        remove(tmpfile_out);
    } else {
        out_buf[0] = '\0';
    }
    FILE *fe = fopen(tmpfile_err, "r");
    if (fe) {
        int leido = (int)fread(err_buf, 1, (size_t)(err_cap - 1), fe);
        err_buf[leido] = '\0';
        fclose(fe);
        remove(tmpfile_err);
    } else {
        err_buf[0] = '\0';
    }
    return rc;
}

int main(void) {
    /* escribir: sin salto de linea final */
    {
        char out[256], errb[256];
        ejecutar_capturando(
            "escribir(\"a\")\n"
            "escribir(\"b\")\n"
            "escribir(\"c\")\n",
            out, sizeof(out), errb, sizeof(errb));
        /* Sin \n entre llamadas — todo en una linea, sin newline final. */
        AFIRMAR(strcmp(out, "abc") == 0, "escribir_sin_newline");
    }

    /* escribir con multiples args separados por espacio */
    {
        char out[256], errb[256];
        ejecutar_capturando(
            "escribir(\"a\", \"b\", \"c\")\n",
            out, sizeof(out), errb, sizeof(errb));
        AFIRMAR(strcmp(out, "a b c") == 0, "escribir_multi_sep");
    }

    /* escribir + imprimir: forma una linea completa */
    {
        char out[256], errb[256];
        ejecutar_capturando(
            "escribir(\"Procesando\")\n"
            "para i en rango(0, 5):\n"
            "    escribir(\".\")\n"
            "fin para\n"
            "imprimir(\" done!\")\n",
            out, sizeof(out), errb, sizeof(errb));
        AFIRMAR(strstr(out, "Procesando..... done!") != NULL,
                "escribir_progreso");
    }

    /* imprimir_error va a stderr, no a stdout */
    {
        char out[256], errb[256];
        ejecutar_capturando(
            "imprimir(\"hola stdout\")\n"
            "imprimir_error(\"hola stderr\")\n"
            "imprimir(\"otra normal\")\n",
            out, sizeof(out), errb, sizeof(errb));
        AFIRMAR(strstr(out, "hola stdout") != NULL, "stdout_visible");
        AFIRMAR(strstr(out, "otra normal") != NULL, "stdout_post_error");
        AFIRMAR(strstr(out, "hola stderr") == NULL, "stderr_no_contamina_stdout");
        AFIRMAR(strstr(errb, "hola stderr") != NULL, "stderr_recibe_error");
    }

    /* imprimir_error con multiples args separa por espacio */
    {
        char out[256], errb[256];
        ejecutar_capturando(
            "imprimir_error(\"linea\", 1, \"aviso\")\n",
            out, sizeof(out), errb, sizeof(errb));
        AFIRMAR(strstr(errb, "linea 1 aviso") != NULL, "stderr_multi_args");
    }

    /* escribir sin args no emite nada (ni siquiera salto) */
    {
        char out[256], errb[256];
        ejecutar_capturando(
            "escribir()\n",
            out, sizeof(out), errb, sizeof(errb));
        AFIRMAR(strcmp(out, "") == 0, "escribir_sin_args");
    }

    /* imprimir() solo emite \n (regresion) */
    {
        char out[256], errb[256];
        ejecutar_capturando(
            "imprimir()\n",
            out, sizeof(out), errb, sizeof(errb));
        AFIRMAR(strcmp(out, "\n") == 0, "regr_imprimir_vacio");
    }

    /* escribir con tipos no-cadena: usa la representacion canonica */
    {
        char out[256], errb[256];
        ejecutar_capturando(
            "escribir(42, 3.14, verdadero, [1, 2])\n",
            out, sizeof(out), errb, sizeof(errb));
        AFIRMAR(strstr(out, "42 3.14 verdadero [1, 2]") != NULL,
                "escribir_tipos");
    }

    if (fallos == 0) {
        printf("escribir: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "escribir: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
