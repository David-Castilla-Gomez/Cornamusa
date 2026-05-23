/*
 * Tests de stdlib/matematicas (v1.103) - raiz, log, exp, trig,
 * redondeo - y stdlib/azar.normal (Box-Muller).
 *
 * Las funciones devuelven decimal. Errores tipicos lanzan
 * ErrorDeValor atrapable.
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
        "test_mat_out.txt";
#else
        "/tmp/test_mat_out.txt";
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
    /* raiz y potencia */
    {
        char out[1024];
        ejecutar_capturando(
            "importar matematicas\n"
            "imprimir(matematicas.raiz(16))\n"
            "imprimir(matematicas.raiz(0))\n"
            "imprimir(matematicas.potencia(2, 10))\n"
            "imprimir(matematicas.hipotenusa(3, 4))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "4.0") != NULL, "raiz_16");
        AFIRMAR(strstr(out, "0.0") != NULL, "raiz_0");
        AFIRMAR(strstr(out, "1024.0") != NULL, "potencia");
        AFIRMAR(strstr(out, "5.0") != NULL, "hipotenusa");
    }

    /* ln, log10, log con base, exp */
    {
        char out[1024];
        ejecutar_capturando(
            "importar matematicas\n"
            "imprimir(matematicas.ln(matematicas.E))\n"
            "imprimir(matematicas.log10(1000))\n"
            "imprimir(matematicas.log(8, 2))\n"
            "imprimir(matematicas.exp(0))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "1.0") != NULL, "ln_e");
        AFIRMAR(strstr(out, "3.0") != NULL, "log10_1000");
        AFIRMAR(strstr(out, "1.0") != NULL, "exp_0");
    }

    /* Trigonometria basica */
    {
        char out[1024];
        ejecutar_capturando(
            "importar matematicas\n"
            "imprimir(matematicas.seno(0))\n"
            "imprimir(matematicas.coseno(0))\n"
            "imprimir(matematicas.tangente(0))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "0.0") != NULL, "seno_0");
        AFIRMAR(strstr(out, "1.0") != NULL, "coseno_0");
    }

    /* Inversas: arco_tangente(1) * 4 == PI */
    {
        char out[1024];
        ejecutar_capturando(
            "importar matematicas\n"
            "pi_aprox = matematicas.arco_tangente(1) * 4\n"
            "imprimir(matematicas.absoluto(pi_aprox - matematicas.PI) < 0.0001)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "atan_aprox_pi");
    }

    /* Conversion grados-radianes */
    {
        char out[1024];
        ejecutar_capturando(
            "importar matematicas\n"
            "imprimir(matematicas.grados_a_radianes(180))\n"
            "imprimir(matematicas.radianes_a_grados(matematicas.PI))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "3.14159") != NULL, "180_grados_a_rad");
        AFIRMAR(strstr(out, "180.0") != NULL, "pi_rad_a_grados");
    }

    /* Redondeo */
    {
        char out[1024];
        ejecutar_capturando(
            "importar matematicas\n"
            "imprimir(matematicas.techo(3.2))\n"
            "imprimir(matematicas.suelo(3.8))\n"
            "imprimir(matematicas.redondear(3.5))\n"
            "imprimir(matematicas.redondear(-2.5))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "4.0") != NULL, "techo");
        AFIRMAR(strstr(out, "3.0") != NULL, "suelo");
        AFIRMAR(strstr(out, "-3.0") != NULL, "redondear_neg");
    }

    /* Errores: raiz negativa, ln no positivo, arco_seno fuera de rango */
    {
        char out[1024];
        ejecutar_capturando(
            "importar matematicas\n"
            "intentar:\n"
            "    matematicas.raiz(-1)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"raiz neg\")\n"
            "fin intentar\n"
            "intentar:\n"
            "    matematicas.ln(0)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"ln 0\")\n"
            "fin intentar\n"
            "intentar:\n"
            "    matematicas.arco_seno(2)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"asin fuera\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "raiz neg") != NULL, "err_raiz_neg");
        AFIRMAR(strstr(out, "ln 0") != NULL, "err_ln_no_positivo");
        AFIRMAR(strstr(out, "asin fuera") != NULL, "err_asin_dominio");
    }

    /* azar.normal: con semilla, media y desviacion son razonables */
    {
        char out[1024];
        ejecutar_capturando(
            "importar azar\n"
            "importar matematicas\n"
            "azar.semilla(42)\n"
            "suma = 0.0\n"
            "suma_sq = 0.0\n"
            "n = 5000\n"
            "para i en rango(0, n):\n"
            "    z = azar.normal(0, 1)\n"
            "    suma = suma + z\n"
            "    suma_sq = suma_sq + z * z\n"
            "fin para\n"
            "media = suma / n\n"
            "var = suma_sq / n - media * media\n"
            "desv = matematicas.raiz(var)\n"
            /* Con 5000 muestras la media debe estar a ~< 0.05 de 0
             * y la desv a ~< 0.05 de 1. Tolerancia generosa. */
            "imprimir(matematicas.absoluto(media) < 0.1)\n"
            "imprimir(matematicas.absoluto(desv - 1.0) < 0.1)\n", out, sizeof(out));
        int n_v = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        AFIRMAR(n_v == 2, "normal_media_y_desv_correctas");
    }

    /* azar.normal con mu y sigma distintos */
    {
        char out[1024];
        ejecutar_capturando(
            "importar azar\n"
            "importar matematicas\n"
            "azar.semilla(123)\n"
            "suma = 0.0\n"
            "n = 5000\n"
            "para i en rango(0, n):\n"
            "    suma = suma + azar.normal(100, 5)\n"
            "fin para\n"
            "media = suma / n\n"
            "imprimir(matematicas.absoluto(media - 100) < 0.5)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "normal_mu100_sigma5");
    }

    /* azar.normal con sigma=0 devuelve mu exacto */
    {
        char out[1024];
        ejecutar_capturando(
            "importar azar\n"
            "imprimir(azar.normal(7, 0))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "7") != NULL, "normal_sigma_cero");
    }

    /* azar.normal con sigma negativo lanza ErrorDeValor */
    {
        char out[1024];
        ejecutar_capturando(
            "importar azar\n"
            "intentar:\n"
            "    azar.normal(0, -1)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"err sigma\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err sigma") != NULL, "normal_sigma_neg_lanza");
    }

    if (fallos == 0) {
        printf("matematicas: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "matematicas: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
