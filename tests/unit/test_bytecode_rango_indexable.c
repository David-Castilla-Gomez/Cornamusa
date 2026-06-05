/*
 * Tests de OP_INDICE y OP_REBANADA sobre VAL_RANGO (v1.131).
 *
 * Antes: `rango(0, 10)[3]` daba "ErrorDeTipo: 'rango' no es indexable",
 * y `rango(0, 10)[2:5]` daba "'rango' no soporta slicing". El
 * destructuring `a, *m, b = rango(0, 5)` (v1.129) fallaba por la
 * limitacion de OP_REBANADA.
 *
 * v1.131:
 *   OP_INDICE sobre VAL_RANGO: calcula `inicio + i * paso` con indices
 *   negativos desde el final. Solo si inicio/fin/paso caben en int64
 *   (caso 99%; bignum masivos rechazados con error claro).
 *
 *   OP_REBANADA sobre VAL_RANGO: materializa el rango en una Lista
 *   temporal y reusa el codigo existente de slicing de listas.
 *
 * Con esto, el destructuring star sobre rango (v1.129) funciona sin
 * envolver en lista(rango(...)).
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
        "test_rango_idx_out.txt";
#else
        "/tmp/test_rango_idx_out.txt";
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
    /* OP_INDICE: positivo desde 0 */
    {
        char out[256];
        ejecutar_capturando(
            "r = rango(0, 10)\n"
            "imprimir(r[0], r[5], r[9])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0 5 9") != NULL, "indice_positivo");
    }

    /* OP_INDICE: negativo desde el final */
    {
        char out[256];
        ejecutar_capturando(
            "r = rango(0, 10)\n"
            "imprimir(r[-1], r[-10])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "9 0") != NULL, "indice_negativo");
    }

    /* OP_INDICE: rango con paso > 1 */
    {
        char out[256];
        ejecutar_capturando(
            "r = rango(0, 20, 3)\n"
            "imprimir(r[0], r[2], r[-1])\n",
            out, sizeof(out));
        /* valores: 0, 3, 6, 9, 12, 15, 18. */
        AFIRMAR(strstr(out, "0 6 18") != NULL, "indice_paso");
    }

    /* OP_INDICE: rango descendente */
    {
        char out[256];
        ejecutar_capturando(
            "r = rango(10, 0, -1)\n"
            "imprimir(r[0], r[9])\n",
            out, sizeof(out));
        /* valores: 10, 9, ..., 1. */
        AFIRMAR(strstr(out, "10 1") != NULL, "indice_descendente");
    }

    /* OP_REBANADA: slice basico */
    {
        char out[256];
        ejecutar_capturando(
            "r = rango(0, 10)\n"
            "imprimir(r[2:5])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[2, 3, 4]") != NULL, "slice_basico");
    }

    /* OP_REBANADA: slice con paso */
    {
        char out[256];
        ejecutar_capturando(
            "r = rango(0, 10)\n"
            "imprimir(r[::2])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 2, 4, 6, 8]") != NULL, "slice_paso");
    }

    /* OP_REBANADA: invertido */
    {
        char out[256];
        ejecutar_capturando(
            "r = rango(0, 5)\n"
            "imprimir(r[::-1])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[4, 3, 2, 1, 0]") != NULL, "slice_invertido");
    }

    /* Destructuring star sobre rango — la motivacion de v1.131 */
    {
        char out[256];
        ejecutar_capturando(
            "a, *m, b = rango(0, 5)\n"
            "imprimir(a, m, b)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0 [1, 2, 3] 4") != NULL, "destr_star_rango");
    }

    /* Destructuring sin star sobre rango */
    {
        char out[256];
        ejecutar_capturando(
            "p, q, r, s = rango(10, 14)\n"
            "imprimir(p, q, r, s)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10 11 12 13") != NULL, "destr_no_star_rango");
    }

    /* longitud(rango) sigue funcionando (regresion) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud(rango(0, 10)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10") != NULL, "longitud_rango");
    }

    if (fallos == 0) {
        printf("rango_idx: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "rango_idx: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
