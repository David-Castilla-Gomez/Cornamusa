/*
 * Tests de `aplanar_profundo(xs)` y `aplanar_hasta(xs, n)` (v1.161).
 *
 * Cornamusa ya tenia `aplanar(xs)` que aplana exactamente UN nivel.
 * Faltaban las variantes:
 *
 *   aplanar_profundo(xs)   - recursivo a profundidad arbitraria
 *                             hasta llegar a hojas no-secuencia.
 *
 *   aplanar_hasta(xs, n)   - aplana exactamente n niveles.
 *                             n=0 devuelve copia, n=1 == aplanar(),
 *                             n grande tiende a aplanar_profundo.
 *
 * Politica: solo lista y tupla se aplanan. Cadenas (que son iterables)
 * se preservan como hojas. Util para JSON/arboles con strings.
 *
 * Sin cambios al nucleo (VM, bytecode, parser, compilador). Solo
 * stdlib usando recursion + tipo().
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
        "test_aplanar_out.txt";
#else
        "/tmp/test_aplanar_out.txt";
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
    /* Regresion: aplanar() de 1 nivel sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar aplanar\n"
            "imprimir(aplanar([[1, 2], [3, 4], [5]]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4, 5]") != NULL, "regr_aplanar");
    }

    /* aplanar_profundo recursivo */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar aplanar_profundo\n"
            "imprimir(aplanar_profundo([1, [2, [3, [4]]], 5]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4, 5]") != NULL, "ap_profundo");
    }

    /* aplanar_profundo con cadenas: NO desempaqueta */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar aplanar_profundo\n"
            "imprimir(aplanar_profundo([1, \"abc\", [2, \"de\"]]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, \"abc\", 2, \"de\"]") != NULL,
                "ap_profundo_cadenas");
    }

    /* aplanar_profundo: ya plana */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar aplanar_profundo\n"
            "imprimir(aplanar_profundo([1, 2, 3]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "ap_profundo_plana");
    }

    /* aplanar_profundo: vacia */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar aplanar_profundo\n"
            "imprimir(aplanar_profundo([]))\n"
            "imprimir(aplanar_profundo([[], [[]]]))\n",
            out, sizeof(out));
        const char *p = out;
        int n = 0;
        while ((p = strstr(p, "[]")) != NULL) { n++; p++; }
        AFIRMAR(n >= 2, "ap_profundo_vacias");
    }

    /* aplanar_profundo: mezcla lista + tupla */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar aplanar_profundo\n"
            "imprimir(aplanar_profundo([1, (2, 3), [4, (5, [6])]]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4, 5, 6]") != NULL,
                "ap_profundo_mixto");
    }

    /* aplanar_hasta: n=0 es identidad (copia) */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar aplanar_hasta\n"
            "imprimir(aplanar_hasta([[[1, 2], [3]], [[4]]], 0))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[[[1, 2], [3]], [[4]]]") != NULL,
                "ah_n0");
    }

    /* aplanar_hasta: n=1 es como aplanar() */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar aplanar_hasta\n"
            "imprimir(aplanar_hasta([[[1, 2], [3]], [[4]]], 1))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[[1, 2], [3], [4]]") != NULL, "ah_n1");
    }

    /* aplanar_hasta: n=2 alcanza completo */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar aplanar_hasta\n"
            "imprimir(aplanar_hasta([[[1, 2], [3]], [[4]]], 2))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4]") != NULL, "ah_n2");
    }

    /* aplanar_hasta: n grande tiende a aplanar_profundo */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar aplanar_hasta\n"
            "imprimir(aplanar_hasta([1, [2, [3, [4, [5]]]]], 99))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4, 5]") != NULL, "ah_n_grande");
    }

    /* aplanar_hasta: n negativo lanza */
    {
        char out[256];
        ejecutar_capturando(
            "desde funcionales importar aplanar_hasta\n"
            "intentar:\n"
            "    aplanar_hasta([1, 2], -1)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "ah_n_neg");
    }

    if (fallos == 0) {
        printf("aplanar: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "aplanar: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
