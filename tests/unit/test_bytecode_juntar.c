/*
 * Tests de `juntar` (zip) y `juntar_mas_largo` (zip_longest) en
 * stdlib (v1.146).
 *
 * Cornamusa ya tenia `producto`, `concatenar`, `ventana`,
 * `pares_consecutivos`, `dividir_en` y muchas mas en
 * `stdlib/iteradores.cor`. Faltaba la operacion `zip` clasica:
 * combinar varios iterables en paralelo posicional.
 *
 * v1.146 anade dos:
 *   - `juntar(*iterables)`: detiene cuando el MAS CORTO se agota.
 *     Aprovecha `*args` en funcion libre (v1.22).
 *   - `juntar_mas_largo(iterables: lista, relleno=nulo)`: detiene
 *     cuando el MAS LARGO se agota; los acabados rellenan con
 *     `relleno`. Firma con lista explicita en vez de `*args`
 *     porque Cornamusa no admite combinar variadicos con defaults
 *     (validacion del compilador desde v1.24).
 *
 * Sin cambios al nucleo. Solo stdlib aprovechando features ya
 * existentes.
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
        "test_juntar_out.txt";
#else
        "/tmp/test_juntar_out.txt";
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
    /* juntar: 2 iterables longitud igual */
    {
        char out[256];
        ejecutar_capturando(
            "desde iteradores importar juntar\n"
            "imprimir(juntar([1, 2, 3], [\"a\", \"b\", \"c\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, \"a\"), (2, \"b\"), (3, \"c\")]") != NULL,
                "juntar_2_igual");
    }

    /* juntar: el mas corto manda */
    {
        char out[256];
        ejecutar_capturando(
            "desde iteradores importar juntar\n"
            "imprimir(juntar([1, 2], [10, 20, 30, 40]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, 10), (2, 20)]") != NULL,
                "juntar_corto_manda");
    }

    /* juntar: 3 iterables */
    {
        char out[256];
        ejecutar_capturando(
            "desde iteradores importar juntar\n"
            "imprimir(juntar([1, 2], [10, 20], [\"a\", \"b\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, 10, \"a\"), (2, 20, \"b\")]") != NULL,
                "juntar_3_iterables");
    }

    /* juntar: vacio */
    {
        char out[256];
        ejecutar_capturando(
            "desde iteradores importar juntar\n"
            "imprimir(juntar())\n"
            "imprimir(juntar([], [1, 2]))\n"
            "imprimir(juntar([1, 2], []))\n",
            out, sizeof(out));
        const char *p = out;
        int n = 0;
        while ((p = strstr(p, "[]")) != NULL) { n++; p++; }
        AFIRMAR(n >= 3, "juntar_vacios");
    }

    /* juntar acepta tuplas y cadenas */
    {
        char out[256];
        ejecutar_capturando(
            "desde iteradores importar juntar\n"
            "imprimir(juntar((1, 2, 3), \"abc\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, \"a\"), (2, \"b\"), (3, \"c\")]") != NULL,
                "juntar_tipos_mixtos");
    }

    /* juntar acepta rango (iterable lazy) */
    {
        char out[256];
        ejecutar_capturando(
            "desde iteradores importar juntar\n"
            "imprimir(juntar(rango(0, 3), [\"a\", \"b\", \"c\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(0, \"a\"), (1, \"b\"), (2, \"c\")]") != NULL,
                "juntar_rango");
    }

    /* juntar_mas_largo: relleno con nulo */
    {
        char out[256];
        ejecutar_capturando(
            "desde iteradores importar juntar_mas_largo\n"
            "imprimir(juntar_mas_largo([[1, 2], [10, 20, 30]]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, 10), (2, 20), (nulo, 30)]") != NULL,
                "juntar_mas_largo_nulo");
    }

    /* juntar_mas_largo: relleno custom */
    {
        char out[256];
        ejecutar_capturando(
            "desde iteradores importar juntar_mas_largo\n"
            "imprimir(juntar_mas_largo([[1, 2], [10, 20, 30]], relleno=0))\n"
            "imprimir(juntar_mas_largo([[1, 2, 3], [10]], relleno=\"x\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(1, 10), (2, 20), (0, 30)]") != NULL,
                "jml_relleno_0");
        AFIRMAR(strstr(out, "[(1, 10), (2, \"x\"), (3, \"x\")]") != NULL,
                "jml_relleno_x");
    }

    /* Caso real: indice + valor (idiomatico de Python `enumerate`) */
    {
        char out[256];
        ejecutar_capturando(
            "desde iteradores importar juntar\n"
            "xs = [\"a\", \"b\", \"c\"]\n"
            "salida = []\n"
            "para par en juntar(rango(0, longitud(xs)), xs):\n"
            "    agregar(salida, par)\n"
            "fin para\n"
            "imprimir(salida)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(0, \"a\"), (1, \"b\"), (2, \"c\")]") != NULL,
                "juntar_idx_valor");
    }

    if (fallos == 0) {
        printf("juntar: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "juntar: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
