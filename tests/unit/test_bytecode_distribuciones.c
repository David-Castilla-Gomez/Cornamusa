/*
 * Tests de distribuciones adicionales en azar (v1.110): exponencial,
 * binomial, poisson + constantes TAU/INFINITO/NO_NUMERO y predicados
 * es_infinito/es_no_numero/es_finito en matematicas.
 *
 * Verificacion estadistica: para cada distribucion, la media empirica
 * sobre N muestras con semilla fija debe estar dentro de tolerancia
 * del valor teorico.
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
        "test_dist_out.txt";
#else
        "/tmp/test_dist_out.txt";
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
    /* Constantes: TAU = 2*PI */
    {
        char out[512];
        ejecutar_capturando(
            "importar matematicas\n"
            "imprimir(matematicas.TAU == 2 * matematicas.PI)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "TAU_es_2_PI");
    }

    /* INFINITO y predicados */
    {
        char out[512];
        ejecutar_capturando(
            "importar matematicas\n"
            "imprimir(matematicas.es_infinito(matematicas.INFINITO))\n"
            "imprimir(matematicas.es_infinito(5))\n"
            "imprimir(matematicas.es_no_numero(matematicas.NO_NUMERO))\n"
            "imprimir(matematicas.es_no_numero(5))\n"
            "imprimir(matematicas.es_finito(matematicas.INFINITO))\n"
            "imprimir(matematicas.es_finito(5))\n", out, sizeof(out));
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v == 3 && n_f == 3, "predicados_inf_nan");
    }

    /* NaN != NaN (regla IEEE 754) */
    {
        char out[512];
        ejecutar_capturando(
            "importar matematicas\n"
            "x = matematicas.NO_NUMERO\n"
            "imprimir(x == x)\n", out, sizeof(out));
        /* NaN != NaN: x == x debe ser falso */
        AFIRMAR(strstr(out, "falso") != NULL, "nan_diferente_de_si_mismo");
    }

    /* Exponencial: media empirica ~= 1/tasa */
    {
        char out[1024];
        ejecutar_capturando(
            "importar azar\n"
            "importar matematicas\n"
            "azar.semilla(42)\n"
            "n = 5000\n"
            "suma = 0.0\n"
            "para i en rango(0, n):\n"
            "    suma = suma + azar.exponencial(2.0)\n"
            "fin para\n"
            "media = suma / n\n"
            /* Media teorica = 1/2 = 0.5; tolerancia generosa */
            "imprimir(matematicas.absoluto(media - 0.5) < 0.05)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "exponencial_media");
    }

    /* Exponencial siempre devuelve >= 0 */
    {
        char out[512];
        ejecutar_capturando(
            "importar azar\n"
            "azar.semilla(7)\n"
            "min_v = 1e30\n"
            "para i en rango(0, 1000):\n"
            "    x = azar.exponencial(1.5)\n"
            "    si x < min_v:\n"
            "        min_v = x\n"
            "    fin si\n"
            "fin para\n"
            "imprimir(min_v >= 0)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "exponencial_no_negativa");
    }

    /* Binomial: media empirica ~= n*p */
    {
        char out[1024];
        ejecutar_capturando(
            "importar azar\n"
            "importar matematicas\n"
            "azar.semilla(123)\n"
            "n = 2000\n"
            "suma = 0\n"
            "para i en rango(0, n):\n"
            "    suma = suma + azar.binomial(100, 0.3)\n"
            "fin para\n"
            "media = suma / 1.0 / n\n"
            /* n*p = 30; tolerancia 0.5 */
            "imprimir(matematicas.absoluto(media - 30) < 0.5)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "binomial_media");
    }

    /* Binomial: con p=0 siempre devuelve 0 */
    {
        char out[512];
        ejecutar_capturando(
            "importar azar\n"
            "para i en rango(0, 100):\n"
            "    si azar.binomial(10, 0) != 0:\n"
            "        imprimir(\"FAIL\")\n"
            "    fin si\n"
            "fin para\n"
            "imprimir(\"OK\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "OK") != NULL && strstr(out, "FAIL") == NULL,
                "binomial_p_cero");
    }

    /* Binomial: con p=1 siempre devuelve n */
    {
        char out[512];
        ejecutar_capturando(
            "importar azar\n"
            "para i en rango(0, 100):\n"
            "    si azar.binomial(7, 1) != 7:\n"
            "        imprimir(\"FAIL\")\n"
            "    fin si\n"
            "fin para\n"
            "imprimir(\"OK\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "OK") != NULL && strstr(out, "FAIL") == NULL,
                "binomial_p_uno");
    }

    /* Poisson: media empirica ~= media teorica */
    {
        char out[1024];
        ejecutar_capturando(
            "importar azar\n"
            "importar matematicas\n"
            "azar.semilla(999)\n"
            "n = 5000\n"
            "suma = 0\n"
            "para i en rango(0, n):\n"
            "    suma = suma + azar.poisson(7)\n"
            "fin para\n"
            "media = suma / 1.0 / n\n"
            /* media = 7; tolerancia 0.2 */
            "imprimir(matematicas.absoluto(media - 7) < 0.2)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "poisson_media");
    }

    /* Poisson: con media=0 siempre devuelve 0 */
    {
        char out[512];
        ejecutar_capturando(
            "importar azar\n"
            "para i en rango(0, 100):\n"
            "    si azar.poisson(0) != 0:\n"
            "        imprimir(\"FAIL\")\n"
            "    fin si\n"
            "fin para\n"
            "imprimir(\"OK\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "OK") != NULL && strstr(out, "FAIL") == NULL,
                "poisson_media_cero");
    }

    /* Errores: rechazos */
    {
        char out[1024];
        ejecutar_capturando(
            "importar azar\n"
            "intentar:\n"
            "    azar.exponencial(0)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"exp 0\")\n"
            "fin intentar\n"
            "intentar:\n"
            "    azar.binomial(10, 1.5)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"binom p\")\n"
            "fin intentar\n"
            "intentar:\n"
            "    azar.binomial(-5, 0.5)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"binom n\")\n"
            "fin intentar\n"
            "intentar:\n"
            "    azar.poisson(-1)\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"pois\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "exp 0") != NULL, "err_exponencial_cero");
        AFIRMAR(strstr(out, "binom p") != NULL, "err_binomial_p_alto");
        AFIRMAR(strstr(out, "binom n") != NULL, "err_binomial_n_neg");
        AFIRMAR(strstr(out, "pois") != NULL, "err_poisson_neg");
    }

    if (fallos == 0) {
        printf("distribuciones: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "distribuciones: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
