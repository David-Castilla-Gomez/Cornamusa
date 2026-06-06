/*
 * Tests de grouping separator (',' y '_') y tipo 'c' (code point)
 * en f-strings (v1.140).
 *
 * v1.139 cerro los gaps basicos (octal, porcentaje, signo). v1.140
 * anade los dos restantes:
 *
 *   ','/'_'  separador de miles. Va entre width y precision en el
 *            spec. Solo compatible con tipos numericos decimales
 *            (d, f, e, %). Hex/octal/binario lo rechazan con error
 *            claro (Python los admite pero acotamos por simplicidad).
 *
 *   'c'      entero -> caracter Unicode (UTF-8). Acepta rango
 *            [0, 0x10FFFF]. Usa utf8proc_encode_char internamente
 *            para producir la secuencia UTF-8 (1-4 bytes).
 *
 * Implementacion: campo grouping en FmtSpec, helper fmt_agrupar que
 * inserta el separador cada 3 digitos en la parte entera respetando
 * signo prefijo y parte decimal/exponente/sufijo. Caso 'c' en la
 * cascada de tipos invoca utf8proc_encode_char tras validar rango.
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
        "test_fstring_grp_out.txt";
#else
        "/tmp/test_fstring_grp_out.txt";
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
    /* Grouping con coma sobre enteros */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{1234567:,d}]\")\n"
            "imprimir(f\"[{1234567890:,d}]\")\n"
            "imprimir(f\"[{1000:,d}]\")\n"
            "imprimir(f\"[{999:,d}]\")\n"
            "imprimir(f\"[{-1234567:,d}]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1,234,567]") != NULL, "coma_basico");
        AFIRMAR(strstr(out, "[1,234,567,890]") != NULL, "coma_largo");
        AFIRMAR(strstr(out, "[1,000]") != NULL, "coma_1000");
        AFIRMAR(strstr(out, "[999]") != NULL, "coma_sin");
        AFIRMAR(strstr(out, "[-1,234,567]") != NULL, "coma_negativo");
    }

    /* Grouping con decimales */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{1234.5678:,.2f}]\")\n"
            "imprimir(f\"[{12345.67:,.2%}]\")\n"
            "imprimir(f\"[{10000000000.0:,.2f}]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1,234.57]") != NULL, "coma_decimal");
        AFIRMAR(strstr(out, "[1,234,567.00%]") != NULL, "coma_pct");
        AFIRMAR(strstr(out, "[10,000,000,000.00]") != NULL, "coma_grande");
    }

    /* Grouping con underscore */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{1234567:_d}]\")\n"
            "imprimir(f\"[{1234567890:_d}]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1_234_567]") != NULL, "underscore_basico");
        AFIRMAR(strstr(out, "[1_234_567_890]") != NULL, "underscore_largo");
    }

    /* Combinaciones: sign + grouping, width + grouping, zero+grouping */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{1234567:+,d}]\")\n"
            "imprimir(f\"[{1234567:15,d}]\")\n"
            "imprimir(f\"[{1234.56:>15,.2f}]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[+1,234,567]") != NULL, "sign_coma");
        AFIRMAR(strstr(out, "[      1,234,567]") != NULL, "width_coma");
        AFIRMAR(strstr(out, "[       1,234.56]") != NULL, "right_coma_decimal");
    }

    /* Code-point 'c' */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{65:c}]\")\n"
            "imprimir(f\"[{241:c}]\")\n"
            "imprimir(f\"[{0x4E2D:c}]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[A]") != NULL, "c_A");
        /* ñ es 2 bytes UTF-8 (0xC3 0xB1) */
        AFIRMAR(strstr(out, "[\xc3\xb1]") != NULL, "c_enie");
        /* 中 es 3 bytes UTF-8 (0xE4 0xB8 0xAD) */
        AFIRMAR(strstr(out, "[\xe4\xb8\xad]") != NULL, "c_cjk");
    }

    /* Code-point 4 bytes (emoji) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{0x1F600:c}]\")\n",
            out, sizeof(out));
        /* 😀 (U+1F600) son 4 bytes UTF-8: 0xF0 0x9F 0x98 0x80 */
        AFIRMAR(strstr(out, "[\xf0\x9f\x98\x80]") != NULL, "c_emoji");
    }

    /* Error: separador incompatible con hex */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    imprimir(f\"[{1234:,x}]\")\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "err_sep_hex");
    }

    /* Error: code-point fuera de rango */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    imprimir(f\"[{2000000:c}]\")\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "err_codepoint_alto");
    }

    /* Error: code-point sobre decimal */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    imprimir(f\"[{3.14:c}]\")\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "err_codepoint_decimal");
    }

    /* Regresion: v1.139 sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"[{42:o}]\")\n"
            "imprimir(f\"[{5:+d}]\")\n"
            "imprimir(f\"[{-5:05d}]\")\n"
            "imprimir(f\"[{0.5:.2%}]\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[52]") != NULL, "regr_o");
        AFIRMAR(strstr(out, "[+5]") != NULL, "regr_sign");
        AFIRMAR(strstr(out, "[-0005]") != NULL, "regr_zero_sign");
        AFIRMAR(strstr(out, "[50.00%]") != NULL, "regr_pct");
    }

    if (fallos == 0) {
        printf("fstring_grouping: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "fstring_grouping: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
