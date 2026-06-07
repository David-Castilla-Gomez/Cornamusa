/*
 * Tests de `**d` dict spread en literales de diccionario (v1.173).
 *
 * Cierra la trilogia de spread (v1.171 lista, v1.172 tupla/conjunto).
 * Patron Python `{**defaults, "k": v, **overrides}` para fusionar
 * configuraciones.
 *
 * Implementacion:
 *   - Parser: detecta `**` en posicion de par; lo marca como
 *     EXPR_UNARIO(TT_DOBLE_ASTERISCO, expr) con valor=NULL.
 *     Tambien acepta `**d` como primer elemento (fuerza dict).
 *   - Compilador: si hay dspread, OP_BUILD_DICC 0 + bucle
 *     DICC_AGREGAR_PAR (pares normales) / DICC_EXTENDER (dspread).
 *     OP_DICC_EXTENDER ya existia desde v1.150.
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
        "test_spread_dicc_out.txt";
#else
        "/tmp/test_spread_dicc_out.txt";
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
    /* dspread basico tras un par normal */
    {
        char out[256];
        ejecutar_capturando(
            "a = {\"k\": 1, \"j\": 2}\n"
            "b = {\"x\": 99, **a}\n"
            "imprimir(b[\"k\"])\n"
            "imprimir(b[\"j\"])\n"
            "imprimir(b[\"x\"])\n"
            "imprimir(longitud(b))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1\n2\n99\n3") != NULL, "tras_par");
    }

    /* dspread como PRIMER elemento (fuerza dict, no conjunto) */
    {
        char out[256];
        ejecutar_capturando(
            "a = {\"k\": 1}\n"
            "b = {**a, \"x\": 99}\n"
            "imprimir(b[\"k\"])\n"
            "imprimir(b[\"x\"])\n"
            "imprimir(longitud(b))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1\n99\n2") != NULL, "primer_dspread");
    }

    /* solo dspread (copia) */
    {
        char out[256];
        ejecutar_capturando(
            "a = {\"x\": 1, \"y\": 2}\n"
            "b = {**a}\n"
            "imprimir(longitud(b))\n"
            "imprimir(a == b)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2\nverdadero") != NULL, "copia");
    }

    /* multiples dspreads */
    {
        char out[256];
        ejecutar_capturando(
            "d1 = {\"x\": 1}\n"
            "d2 = {\"y\": 2}\n"
            "d3 = {**d1, **d2, \"z\": 3}\n"
            "imprimir(longitud(d3))\n"
            "imprimir(d3[\"x\"])\n"
            "imprimir(d3[\"y\"])\n"
            "imprimir(d3[\"z\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3\n1\n2\n3") != NULL, "multiples_dspreads");
    }

    /* dspread con override: ultimo gana */
    {
        char out[256];
        ejecutar_capturando(
            "defecto = {\"timeout\": 30, \"host\": \"default\"}\n"
            "config = {**defecto, \"host\": \"prod\"}\n"
            "imprimir(config[\"host\"])\n"
            "imprimir(config[\"timeout\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "prod\n30") != NULL, "override");
    }

    /* dspread con override inverso: explicito primero, dspread sobrescribe */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir({\"a\": 1, **{\"a\": 99}})\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "99") != NULL, "override_inverso");
    }

    /* dicc vacio sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir({})\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "{}") != NULL, "vacio");
    }

    /* dicc sin dspread sigue siendo eficiente (path normal) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir({\"a\": 1, \"b\": 2})\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "{\"a\": 1, \"b\": 2}") != NULL, "sin_dspread");
    }

    /* dspread de un valor no-dicc -> ErrorDeTipo atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "funcion p():\n"
            "    intentar:\n"
            "        d = {**[1, 2, 3]}\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "dspread_no_dicc");
    }

    /* Mezcla con claves no-string */
    {
        char out[256];
        ejecutar_capturando(
            "a = {1: \"uno\", 2: \"dos\"}\n"
            "b = {**a, 3: \"tres\"}\n"
            "imprimir(b[1])\n"
            "imprimir(b[2])\n"
            "imprimir(b[3])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "uno\ndos\ntres") != NULL, "claves_enteras");
    }

    if (fallos == 0) {
        printf("spread_dicc: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "spread_dicc: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
