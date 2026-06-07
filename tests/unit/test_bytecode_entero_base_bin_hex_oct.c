/*
 * Tests de `entero(s, base)`, `binario(n)`, `hexadecimal(n)`,
 * `octal(n)` (v1.159).
 *
 * Cornamusa tenia `entero(x)` (un solo argumento, decimal). Faltaban
 * los equivalentes a:
 *
 *   Python int(s, base)   - parsear con base 2..36 o 0 (auto).
 *   Python bin(n)         - cadena base 2 con prefijo "0b".
 *   Python hex(n)         - cadena base 16 con prefijo "0x".
 *   Python oct(n)         - cadena base 8 con prefijo "0o".
 *
 * Todos soportan bignum via libtommath (mp_read_radix / mp_to_radix).
 * Negativos llevan '-' al frente (no en el medio).
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
        "test_ebin_out.txt";
#else
        "/tmp/test_ebin_out.txt";
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
    /* entero(s, base) — bases comunes */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(entero(\"ff\", 16))\n"       /* 255 */
            "imprimir(entero(\"FF\", 16))\n"       /* mayusculas */
            "imprimir(entero(\"1010\", 2))\n"      /* 10 */
            "imprimir(entero(\"777\", 8))\n"       /* 511 */
            "imprimir(entero(\"zz\", 36))\n",      /* 1295 */
            out, sizeof(out));
        AFIRMAR(strstr(out, "255") != NULL, "ent_hex");
        AFIRMAR(strstr(out, "10") != NULL, "ent_bin");
        AFIRMAR(strstr(out, "511") != NULL, "ent_oct");
        AFIRMAR(strstr(out, "1295") != NULL, "ent_36");
    }

    /* entero(s, base) acepta prefijo coincidente */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(entero(\"0xff\", 16))\n"
            "imprimir(entero(\"0b1010\", 2))\n"
            "imprimir(entero(\"0o17\", 8))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "255") != NULL, "ent_pref_hex");
        AFIRMAR(strstr(out, "10") != NULL, "ent_pref_bin");
        AFIRMAR(strstr(out, "15") != NULL, "ent_pref_oct");
    }

    /* entero(s, 0) auto-detecta */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(entero(\"0xff\", 0))\n"
            "imprimir(entero(\"0b1010\", 0))\n"
            "imprimir(entero(\"0o17\", 0))\n"
            "imprimir(entero(\"42\", 0))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "255") != NULL, "auto_hex");
        AFIRMAR(strstr(out, "10") != NULL, "auto_bin");
        AFIRMAR(strstr(out, "15") != NULL, "auto_oct");
        AFIRMAR(strstr(out, "42") != NULL, "auto_dec");
    }

    /* Signo negativo */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(entero(\"-7b\", 16))\n"
            "imprimir(entero(\"-1010\", 2))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "-123") != NULL, "ent_neg_hex");
        AFIRMAR(strstr(out, "-10") != NULL, "ent_neg_bin");
    }

    /* binario/hexadecimal/octal */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(binario(10))\n"
            "imprimir(binario(0))\n"
            "imprimir(binario(-5))\n"
            "imprimir(hexadecimal(255))\n"
            "imprimir(hexadecimal(0xdeadbeef))\n"
            "imprimir(octal(8))\n"
            "imprimir(octal(64))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0b1010") != NULL, "bin_10");
        AFIRMAR(strstr(out, "0b0") != NULL, "bin_0");
        AFIRMAR(strstr(out, "-0b101") != NULL, "bin_neg");
        AFIRMAR(strstr(out, "0xff") != NULL, "hex_255");
        AFIRMAR(strstr(out, "0xdeadbeef") != NULL, "hex_deadbeef");
        AFIRMAR(strstr(out, "0o10") != NULL, "oct_8");
        AFIRMAR(strstr(out, "0o100") != NULL, "oct_64");
    }

    /* Bignum: 2**100 */
    {
        char out[512];
        ejecutar_capturando(
            "imprimir(hexadecimal(2**64))\n"
            "imprimir(longitud(binario(2**100)) > 100)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0x10000000000000000") != NULL, "hex_bignum");
        AFIRMAR(strstr(out, "verdadero") != NULL, "bin_bignum_largo");
    }

    /* Round-trip: entero(binario(n), 0) == n */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(entero(binario(42), 0) == 42)\n"
            "imprimir(entero(hexadecimal(0xdeadbeef), 0) == 0xdeadbeef)\n"
            "imprimir(entero(octal(64), 0) == 64)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "roundtrip");
    }

    /* Errores: dígito inválido para la base */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    entero(\"gg\", 16)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "ent_digito_invalido");
    }

    /* Errores: base fuera de rango */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    entero(\"42\", 1)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n"
            "intentar:\n"
            "    entero(\"42\", 37)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ok2\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "base_baja");
        AFIRMAR(strstr(out, "ok2") != NULL, "base_alta");
    }

    /* Errores: binario/etc sobre no-entero */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    binario(3.14)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "bin_decimal_rechaza");
    }

    /* Regresión: entero(x) sin base sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(entero(\"42\"))\n"
            "imprimir(entero(42))\n"
            "imprimir(entero(3.7))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "regr_dec");
        AFIRMAR(strstr(out, "3") != NULL, "regr_trunc");
    }

    /* Guiones bajos en cadena (idiomático Cornamusa/Python) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(entero(\"de_ad_be_ef\", 16))\n"
            "imprimir(entero(\"1_000_000\", 0))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3735928559") != NULL, "guion_bajo_hex");
        AFIRMAR(strstr(out, "1000000") != NULL, "guion_bajo_dec");
    }

    if (fallos == 0) {
        printf("ebin: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "ebin: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
