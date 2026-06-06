/*
 * Tests de `cadena.dividir_palabras()` y `cadena.rellenar_ceros(n)`
 * (v1.157).
 *
 * Python:
 *   str.split() (sin args)  - divide por whitespace, sin vacios
 *   str.zfill(n)            - rellena con '0' a la izquierda,
 *                              respetando signo prefijo
 *
 * Cornamusa ya tenia:
 *   separar(sep)  divide por separador literal (emite vacios)
 *   alinear_derecha(n, "0")  rellena con cero PERO si el valor
 *                             empieza con '-', el '-' queda
 *                             enmedio porque se trata como char
 *                             arbitrario.
 *
 * v1.157 cubre los dos huecos: split-por-whitespace idiomatico
 * (sin vacios entre runs) y zfill con signo correcto.
 *
 * Sin cambios a bytecode ni VM.
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
        "test_cad_pc_out.txt";
#else
        "/tmp/test_cad_pc_out.txt";
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
    /* dividir_palabras basico */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola mundo\".dividir_palabras())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"hola\", \"mundo\"]") != NULL, "dp_basico");
    }

    /* Multiples espacios consecutivos NO producen vacios */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"a    b\".dividir_palabras())\n"
            "imprimir(\"hola   mundo   adios\".dividir_palabras())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"a\", \"b\"]") != NULL, "dp_runs");
        AFIRMAR(strstr(out, "[\"hola\", \"mundo\", \"adios\"]") != NULL,
                "dp_runs_3");
    }

    /* Leading/trailing whitespace se descarta */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"   leading trailing   \".dividir_palabras())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"leading\", \"trailing\"]") != NULL,
                "dp_strip");
    }

    /* Cadena vacia y solo whitespace */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"\".dividir_palabras())\n"
            "imprimir(\"   \".dividir_palabras())\n",
            out, sizeof(out));
        const char *p = out;
        int n = 0;
        while ((p = strstr(p, "[]")) != NULL) { n++; p++; }
        AFIRMAR(n >= 2, "dp_vacios");
    }

    /* Otros whitespace: \t y \n */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"uno\\tdos tres\\ncuatro\".dividir_palabras())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"uno\", \"dos\", \"tres\", \"cuatro\"]") != NULL,
                "dp_mixto");
    }

    /* Una sola palabra sin whitespace */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"solo\".dividir_palabras())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"solo\"]") != NULL, "dp_singleton");
    }

    /* rellenar_ceros sin signo */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"5\".rellenar_ceros(4))\n"
            "imprimir(\"12345\".rellenar_ceros(3))\n"
            "imprimir(\"\".rellenar_ceros(3))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0005") != NULL, "rc_sin_signo");
        AFIRMAR(strstr(out, "12345") != NULL, "rc_ya_cabe");
        AFIRMAR(strstr(out, "000") != NULL, "rc_vacia");
    }

    /* rellenar_ceros respeta signo prefijo (paridad str.zfill) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"-5\".rellenar_ceros(4))\n"
            "imprimir(\"+5\".rellenar_ceros(4))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "-005") != NULL, "rc_negativo");
        AFIRMAR(strstr(out, "+005") != NULL, "rc_positivo_explicito");
    }

    /* rellenar_ceros sobre cadena no numerica */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"hola\".rellenar_ceros(8))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0000hola") != NULL, "rc_no_numero");
    }

    /* rellenar_ceros con Unicode (cuenta code-points) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"\xc3\xb1\".rellenar_ceros(4))\n",  /* ñ */
            out, sizeof(out));
        /* ñ es 1 code-point, debe quedar "000ñ" (3 ceros + ñ) */
        AFIRMAR(strstr(out, "000\xc3\xb1") != NULL, "rc_unicode_codepoints");
    }

    /* rellenar_ceros con ancho 0 o menor que cadena (sin cambios) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(\"abc\".rellenar_ceros(0))\n"
            "imprimir(\"abc\".rellenar_ceros(2))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "abc") != NULL, "rc_ancho_pequeno");
    }

    /* Error: rellenar_ceros con ancho negativo */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    \"5\".rellenar_ceros(-1)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "rc_ancho_neg");
    }

    if (fallos == 0) {
        printf("cad_pc: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "cad_pc: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
