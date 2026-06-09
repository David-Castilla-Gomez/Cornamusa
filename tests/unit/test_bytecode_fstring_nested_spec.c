/*
 * Tests de format spec con interpolaciones `{...}` en f-strings (v1.189).
 *
 * Paridad Python: f"{x:{ancho}}" evalua `ancho` y lo usa como ancho.
 * Multiples interpolaciones en spec: f"{x:{fill}>{ancho}}".
 *
 * Implementacion: si el spec contiene `{`, el parser lo divide en
 * sub-partes (literal + expr) y el compilador construye el spec
 * en runtime concatenando. Nuevo opcode OP_FORMATO_F_SPEC_DIN toma
 * el spec del stack.
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
        "test_fstring_nested_out.txt";
#else
        "/tmp/test_fstring_nested_out.txt";
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
    /* Ancho dinamico */
    {
        char out[256];
        ejecutar_capturando(
            "ancho = 10\n"
            "imprimir(\"|\" + f\"{42:{ancho}}\" + \"|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|        42|") != NULL, "ancho_din");
    }

    /* Align + ancho dinamico */
    {
        char out[256];
        ejecutar_capturando(
            "ancho = 10\n"
            "imprimir(\"|\" + f\"{42:>{ancho}}\" + \"|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|        42|") != NULL, "align_ancho_din");
    }

    /* Precision dinamica */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"|\" + f\"{3.14159:.{6}f}\" + \"|\")\n"
            "imprimir(\"|\" + f\"{3.14159:.{2}f}\" + \"|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|3.141590|") != NULL, "precision_6");
        AFIRMAR(strstr(out, "|3.14|") != NULL, "precision_2");
    }

    /* Fill + align + ancho dinamicos */
    {
        char out[256];
        ejecutar_capturando(
            "fill = \"*\"\n"
            "anc = 8\n"
            "imprimir(\"|\" + f\"{42:{fill}>{anc}}\" + \"|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|******42|") != NULL, "fill_align_din");
    }

    /* Hex con ancho dinamico */
    {
        char out[256];
        ejecutar_capturando(
            "anc = 10\n"
            "imprimir(\"|\" + f\"{255:#{anc}x}\" + \"|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|      0xff|") != NULL, "hex_alt_din");
    }

    /* Calculo en la expresion del spec */
    {
        char out[256];
        ejecutar_capturando(
            "base = 5\n"
            "imprimir(\"|\" + f\"{42:{base + 5}}\" + \"|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|        42|") != NULL, "expr_calc");
    }

    /* Spec sin interpolacion sigue funcionando (regresion) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"|\" + f\"{42:5}\" + \"|\")\n"
            "imprimir(\"|\" + f\"{3.14:.2f}\" + \"|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|   42|") != NULL, "regr_estatico");
        AFIRMAR(strstr(out, "|3.14|") != NULL, "regr_decimal");
    }

    /* Conversor + spec dinamico */
    {
        char out[256];
        ejecutar_capturando(
            "anc = 10\n"
            "imprimir(\"|\" + f\"{\"hola\"!r:>{anc}}\" + \"|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|    \"hola\"|") != NULL, "conv_din");
    }

    if (fallos == 0) {
        printf("fstring_nested: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "fstring_nested: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
