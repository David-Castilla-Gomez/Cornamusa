/*
 * Tests del modulo stdlib/estadisticas (v1.117).
 *
 * Cubre: centralidad (media, mediana, moda, multimodal,
 * media_armonica, media_geometrica), dispersion (varianza,
 * desviacion muestral y poblacional, amplitud), posicion
 * (percentil, cuartiles) y correlacion (covarianza, Pearson,
 * regresion lineal).
 *
 * Patron: ejecutar fragmento Cornamusa que importa el modulo y
 * imprime un valor concreto; verificar substring en stdout. Para
 * floats el match es por prefijo redondeado (5.37, 13.4, etc).
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
        "test_estad_out.txt";
#else
        "/tmp/test_estad_out.txt";
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
    /* media aritmetica */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.media([10, 20, 30, 40, 50]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "30") != NULL, "media_simple");
    }

    /* mediana lista impar */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.mediana([5, 1, 3, 2, 4]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "mediana_impar");
    }

    /* mediana lista par => promedio */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.mediana([1, 2, 3, 4]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2.5") != NULL, "mediana_par");
    }

    /* mediana_baja / mediana_alta en lista par */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.mediana_baja([1, 2, 3, 4]))\n"
            "imprimir(estadisticas.mediana_alta([1, 2, 3, 4]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "mediana_baja_par");
        AFIRMAR(strstr(out, "3") != NULL, "mediana_alta_par");
    }

    /* moda: valor mas frecuente */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.moda([1, 2, 2, 3, 3, 3, 4]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "moda_basica");
    }

    /* multimodal: empate => dos modas */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "ms = estadisticas.multimodal([1, 1, 2, 2, 3])\n"
            "imprimir(longitud(ms))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "multimodal_dos_modas");
    }

    /* media armonica de [2, 4, 8] = 24/7 ~ 3.428 */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.media_armonica([2, 4, 8]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3.42") != NULL, "media_armonica");
    }

    /* media geometrica de [2, 8] = sqrt(16) = 4 */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.media_geometrica([2, 8]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "4") != NULL, "media_geometrica");
    }

    /* varianza muestral de [2, 4, 4, 4, 5, 5, 7, 9]
       media=5, suma((x-5)^2)=32, varianza=32/7 ≈ 4.571 */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.varianza([2, 4, 4, 4, 5, 5, 7, 9]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "4.57") != NULL, "varianza_muestral");
    }

    /* varianza poblacional de [2, 4, 4, 4, 5, 5, 7, 9] = 32/8 = 4 */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.varianza_pob([2, 4, 4, 4, 5, 5, 7, 9]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "4") != NULL, "varianza_poblacional");
    }

    /* desviacion poblacional = sqrt(4) = 2 */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.desviacion_pob([2, 4, 4, 4, 5, 5, 7, 9]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "desviacion_poblacional");
    }

    /* amplitud */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.amplitud([3, 7, 1, 9, 5]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "8") != NULL, "amplitud");
    }

    /* percentil 0 => min, 100 => max, 50 => mediana */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "xs = [1, 2, 3, 4, 5]\n"
            "imprimir(estadisticas.percentil(xs, 0))\n"
            "imprimir(estadisticas.percentil(xs, 100))\n"
            "imprimir(estadisticas.percentil(xs, 50))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "percentil_min");
        AFIRMAR(strstr(out, "5") != NULL, "percentil_max");
        AFIRMAR(strstr(out, "3") != NULL, "percentil_mediana");
    }

    /* cuartiles */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "q = estadisticas.cuartiles([1, 2, 3, 4, 5, 6, 7, 8, 9])\n"
            "imprimir(q[0])\n"
            "imprimir(q[1])\n"
            "imprimir(q[2])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3") != NULL, "Q1");
        AFIRMAR(strstr(out, "5") != NULL, "Q2_mediana");
        AFIRMAR(strstr(out, "7") != NULL, "Q3");
    }

    /* correlacion perfecta positiva = 1.0 */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.correlacion([1,2,3,4,5], [2,4,6,8,10]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1") != NULL, "correlacion_perfecta");
    }

    /* correlacion perfecta negativa = -1.0 */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "imprimir(estadisticas.correlacion([1,2,3,4,5], [10,8,6,4,2]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "-1") != NULL, "correlacion_negativa");
    }

    /* regresion lineal y = 2x + 0 */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "r = estadisticas.regresion_lineal([1,2,3,4,5], [2,4,6,8,10])\n"
            "imprimir(r[\"pendiente\"])\n"
            "imprimir(r[\"intercepto\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "regresion_pendiente");
        AFIRMAR(strstr(out, "0") != NULL, "regresion_intercepto");
    }

    /* lista vacia lanza ErrorDeValor atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "intentar:\n"
            "    estadisticas.media([])\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "media_vacia_lanza");
    }

    /* varianza muestral con n=1 lanza */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "intentar:\n"
            "    estadisticas.varianza([42])\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "varianza_n1_lanza");
    }

    /* media_armonica con 0 o negativo lanza */
    {
        char out[256];
        ejecutar_capturando(
            "importar estadisticas\n"
            "intentar:\n"
            "    estadisticas.media_armonica([1, 0, 3])\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "media_armonica_cero");
    }

    /* resumen devuelve dict con claves esperadas */
    {
        char out[512];
        ejecutar_capturando(
            "importar estadisticas\n"
            "r = estadisticas.resumen([1, 2, 3, 4, 5])\n"
            "imprimir(r[\"n\"])\n"
            "imprimir(r[\"min\"])\n"
            "imprimir(r[\"max\"])\n"
            "imprimir(r[\"media\"])\n"
            "imprimir(r[\"mediana\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "resumen_n");
        AFIRMAR(strstr(out, "1") != NULL, "resumen_min");
        AFIRMAR(strstr(out, "3") != NULL, "resumen_media_mediana");
    }

    if (fallos == 0) {
        printf("estadisticas: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "estadisticas: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
