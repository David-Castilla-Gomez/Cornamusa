/*
 * Tests de format specifiers en f-strings (v1.139).
 *
 * v1.45 introdujo f-string format specs basicos: alineacion
 * (<>^), fill char, width, precision, tipos d/f/e/x/X/b/s.
 *
 * v1.139 anade:
 *   - Tipo 'o' (octal). Acepta enteros y booleanos.
 *   - Tipo '%' (porcentaje). Multiplica por 100 y agrega '%'.
 *   - Flag 'sign' ('+' o ' ') para tipos numericos. Prepend '+'
 *     o espacio cuando el valor no es negativo.
 *   - Zero-padding con signo: el signo se preserva al frente y
 *     los ceros se rellenan despues (paridad con Python: '+0005'
 *     en vez de '000+5').
 *
 * Implementacion: extension de FmtSpec con campo sign, parseo
 * del flag tras [fill][align], y manejo en fmt_aplicar_padding
 * para zero-padding+signo. Sin cambios a bytecode ni VM.
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
        "test_fstring_specs_out.txt";
#else
        "/tmp/test_fstring_specs_out.txt";
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
    /* Octal */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{42:o}]\")\n"
            "imprimir(f\"[{255:o}]\")\n"
            "imprimir(f\"[{42:5o}]\")\n"
            "imprimir(f\"[{8:#04o}\" + \"]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[52]") != NULL, "octal_42");
        AFIRMAR(strstr(out, "[377]") != NULL, "octal_255");
        AFIRMAR(strstr(out, "[   52]") != NULL, "octal_width");
    }

    /* Signo + (positivos explicitos) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{5:+d}]\")\n"
            "imprimir(f\"[{0:+d}]\")\n"
            "imprimir(f\"[{-5:+d}]\")\n"
            "imprimir(f\"[{3.14:+.2f}]\")\n"
            "imprimir(f\"[{-3.14:+.2f}]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[+5]") != NULL, "sign_pos");
        AFIRMAR(strstr(out, "[+0]") != NULL, "sign_cero");
        AFIRMAR(strstr(out, "[-5]") != NULL, "sign_neg");
        AFIRMAR(strstr(out, "[+3.14]") != NULL, "sign_decimal_pos");
        AFIRMAR(strstr(out, "[-3.14]") != NULL, "sign_decimal_neg");
    }

    /* Signo espacio */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{5: d}]\")\n"
            "imprimir(f\"[{-5: d}]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[ 5]") != NULL, "sign_space_pos");
        AFIRMAR(strstr(out, "[-5]") != NULL, "sign_space_neg");
    }

    /* Zero-padding con signo: el signo va antes de los ceros */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{5:+05d}]\")\n"
            "imprimir(f\"[{-5:05d}]\")\n"
            "imprimir(f\"[{3.14:+08.2f}]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[+0005]") != NULL, "zero_pad_pos");
        AFIRMAR(strstr(out, "[-0005]") != NULL, "zero_pad_neg");
        AFIRMAR(strstr(out, "[+0003.14]") != NULL, "zero_pad_decimal");
    }

    /* Porcentaje */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{0.5:.2%}]\")\n"
            "imprimir(f\"[{0.123:.1%}]\")\n"
            "imprimir(f\"[{-0.25:.0%}]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[50.00%]") != NULL, "pct_50");
        AFIRMAR(strstr(out, "[12.3%]") != NULL, "pct_12");
        AFIRMAR(strstr(out, "[-25%]") != NULL, "pct_neg");
    }

    /* Regresion: tipos clasicos */
    {
        char out[512];
        ejecutar_capturando(
            "imprimir(f\"[{42:5d}]\")\n"
            "imprimir(f\"[{42:b}]\")\n"
            "imprimir(f\"[{42:x}]\")\n"
            "imprimir(f\"[{42:X}]\")\n"
            "imprimir(f\"[{3.14:.4f}]\")\n"
            "imprimir(f\"[{'hola':>10}]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[   42]") != NULL, "regr_d");
        AFIRMAR(strstr(out, "[101010]") != NULL, "regr_b");
        AFIRMAR(strstr(out, "[2a]") != NULL, "regr_x");
        AFIRMAR(strstr(out, "[2A]") != NULL, "regr_X");
        AFIRMAR(strstr(out, "[3.1400]") != NULL, "regr_f");
        AFIRMAR(strstr(out, "[      hola]") != NULL, "regr_s_align");
    }

    /* Error: tipo no soportado */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    imprimir(f\"[{42:z}]\")\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "tipo_invalido");
    }

    /* Error: tipo numerico sobre cadena */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    imprimir(f\"[{'no num':d}]\")\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "tipo_d_sobre_cadena");
    }

    if (fallos == 0) {
        printf("fstring_specs: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "fstring_specs: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
