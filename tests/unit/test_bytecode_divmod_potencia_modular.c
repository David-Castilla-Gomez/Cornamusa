/*
 * Tests de `divmod(a, b)`, `potencia_modular(b, e, m)` y `mcm(a, b)`
 * (v1.158).
 *
 * Tres helpers numericos basicos que faltaban:
 *
 *   divmod(a, b)           Tupla (cociente, resto) en una pasada.
 *                          Floor div (paridad Python).
 *                          Soporta bignum (a o b enteros grandes).
 *
 *   potencia_modular(b, e, m)
 *                          (b^e) % m de forma eficiente —
 *                          O(log e * tam_m) via mp_exptmod.
 *                          Paridad con Python pow(b, e, m).
 *                          Util en criptografia y teoria de numeros.
 *
 *   matematicas.mcm(a, b)  Minimo comun multiplo. Devuelve 0 si
 *                          alguno es 0. Paridad con Python math.lcm.
 *
 * Sin cambios a bytecode ni VM. Las nativas usan libtommath
 * directamente.
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
        "test_divmod_pmod_out.txt";
#else
        "/tmp/test_divmod_pmod_out.txt";
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
    /* divmod basico positivo */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(divmod(17, 5))\n"
            "imprimir(divmod(20, 4))\n"
            "imprimir(divmod(0, 5))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(3, 2)") != NULL, "divmod_17_5");
        AFIRMAR(strstr(out, "(5, 0)") != NULL, "divmod_20_4");
        AFIRMAR(strstr(out, "(0, 0)") != NULL, "divmod_0_5");
    }

    /* divmod con negativos (floor div, paridad Python) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(divmod(-17, 5))\n"
            "imprimir(divmod(17, -5))\n"
            "imprimir(divmod(-17, -5))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(-4, 3)") != NULL, "divmod_neg_a");
        AFIRMAR(strstr(out, "(-4, -3)") != NULL, "divmod_neg_b");
        AFIRMAR(strstr(out, "(3, -2)") != NULL, "divmod_ambos_neg");
    }

    /* divmod con bignum (a > int64_max) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(divmod(10**30, 7))\n",
            out, sizeof(out));
        /* 10^30 / 7 = 142857142857142857142857142857, resto 1 */
        AFIRMAR(strstr(out, "(142857142857142857142857142857, 1)") != NULL,
                "divmod_bignum");
    }

    /* divmod por cero lanza ErrorAritmetico atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    divmod(5, 0)\n"
            "atrapar ErrorAritmetico:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "divmod_div_cero");
    }

    /* divmod rechaza no-enteros */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    divmod(3.14, 2)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "divmod_no_entero");
    }

    /* potencia_modular basico */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(potencia_modular(2, 10, 100))\n"       /* 1024 % 100 = 24 */
            "imprimir(potencia_modular(2, 10, 10000))\n"     /* 1024 (cabe) */
            "imprimir(potencia_modular(7, 100, 13))\n",      /* 9 */
            out, sizeof(out));
        AFIRMAR(strstr(out, "24") != NULL, "pmod_basico");
        AFIRMAR(strstr(out, "1024") != NULL, "pmod_no_satura");
        AFIRMAR(strstr(out, "9") != NULL, "pmod_real");
    }

    /* potencia_modular: 0^0 % n = 1 (convencion Python) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(potencia_modular(0, 0, 7))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "pmod_0_0");
    }

    /* potencia_modular con exponente grande (crypto-like) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(potencia_modular(123456789, 987654321, 1000000007))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "652541198") != NULL, "pmod_crypto");
    }

    /* potencia_modular con exponente negativo lanza */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    potencia_modular(2, -1, 5)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "pmod_exp_neg");
    }

    /* potencia_modular con modulo 0 lanza ErrorAritmetico */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    potencia_modular(2, 5, 0)\n"
            "atrapar ErrorAritmetico:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "pmod_mod_cero");
    }

    /* mcm basico */
    {
        char out[256];
        ejecutar_capturando(
            "desde matematicas importar mcm\n"
            "imprimir(mcm(4, 6))\n"          /* 12 */
            "imprimir(mcm(12, 18))\n"        /* 36 */
            "imprimir(mcm(0, 5))\n"          /* 0 */
            "imprimir(mcm(5, 0))\n",         /* 0 */
            out, sizeof(out));
        AFIRMAR(strstr(out, "12") != NULL, "mcm_4_6");
        AFIRMAR(strstr(out, "36") != NULL, "mcm_12_18");
        AFIRMAR(strstr(out, "0") != NULL, "mcm_con_cero");
    }

    /* mcm con coprimos = producto */
    {
        char out[256];
        ejecutar_capturando(
            "desde matematicas importar mcm\n"
            "imprimir(mcm(7, 11))\n"   /* 77 */
            "imprimir(mcm(13, 17))\n", /* 221 */
            out, sizeof(out));
        AFIRMAR(strstr(out, "77") != NULL, "mcm_coprimos");
        AFIRMAR(strstr(out, "221") != NULL, "mcm_coprimos_2");
    }

    if (fallos == 0) {
        printf("dm_pm: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "dm_pm: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
