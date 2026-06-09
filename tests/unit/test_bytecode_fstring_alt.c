/*
 * Tests del flag `#` (alternate form) en f-strings (v1.188).
 *
 * Paridad Python: `#` añade prefijo `0x`/`0X`/`0b`/`0o` al
 * resultado de los tipos hex/bin/oct. Con `0` zero-padding, el
 * relleno va entre el prefijo y los dígitos.
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
        "test_fstring_alt_out.txt";
#else
        "/tmp/test_fstring_alt_out.txt";
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
    /* Prefijos básicos */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{255:#x}\")\n"
            "imprimir(f\"{255:#X}\")\n"
            "imprimir(f\"{8:#b}\")\n"
            "imprimir(f\"{8:#o}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0xff") != NULL, "hex");
        AFIRMAR(strstr(out, "0XFF") != NULL, "HEX");
        AFIRMAR(strstr(out, "0b1000") != NULL, "bin");
        AFIRMAR(strstr(out, "0o10") != NULL, "oct");
    }

    /* Negativos: prefijo va tras el signo */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{-255:#x}\")\n"
            "imprimir(f\"{-8:#b}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "-0xff") != NULL, "neg_hex");
        AFIRMAR(strstr(out, "-0b1000") != NULL, "neg_bin");
    }

    /* Cero */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{0:#x}\")\n"
            "imprimir(f\"{0:#b}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0x0") != NULL, "cero_x");
        AFIRMAR(strstr(out, "0b0") != NULL, "cero_b");
    }

    /* Ancho con padding fill */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"|{255:#10x}|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|      0xff|") != NULL, "ancho_default");
    }

    /* Zero-padding: relleno entre prefijo y dígitos */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{255:#010x}\")\n"
            "imprimir(f\"{15:#08b}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0x000000ff") != NULL, "zero_pad_hex");
        AFIRMAR(strstr(out, "0b001111") != NULL, "zero_pad_bin");
    }

    /* Zero-padding con negativo: signo, prefijo, ceros, dígitos */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{-15:#06x}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "-0x00f") != NULL, "zero_pad_neg");
    }

    /* Alineación con fill custom (mismo `*` evita conflicto con `#`) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"|{255:*>#10x}|\")\n"
            "imprimir(f\"|{255:*<#10x}|\")\n"
            "imprimir(f\"|{255:*^#10x}|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "|******0xff|") != NULL, "rellena_der");
        AFIRMAR(strstr(out, "|0xff******|") != NULL, "rellena_izq");
        AFIRMAR(strstr(out, "|***0xff***|") != NULL, "rellena_centro");
    }

    /* Sin # no se añade prefijo (regresión) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{255:x}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ff") != NULL && strstr(out, "0xff") == NULL,
                "sin_alt");
    }

    if (fallos == 0) {
        printf("fstring_alt: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "fstring_alt: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
