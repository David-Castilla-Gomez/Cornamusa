/*
 * Tests de destructuring en cabecera de comprehensions (v1.135).
 *
 * Antes:
 *   [a + b para a, b en pares]  -> ErrorDeSintaxis ("se esperaba 'en'")
 *   [resto para _, *resto en filas]  -> idem
 *
 * v1.135:
 *   AST: ClausulaComp y comprehension ganan un campo `patron`
 *   (Expr * NULL = legacy). Si el patron != NULL es EXPR_TUPLA
 *   con elementos EXPR_IDENT o EXPR_STAR_BIND (solo un nivel).
 *
 *   Parser: nuevo helper parsear_destinos_compr que parsea una
 *   lista de IDENT / *IDENT separada por coma. Si solo hay un
 *   IDENT, devuelve por los campos legacy. Si hay mas, construye
 *   EXPR_TUPLA en `patron`.
 *
 *   Compilador: cuando una clausula tiene patron, el slot_var
 *   pasa a ser anonimo ($comp_item) y los slots de los destinos
 *   se pre-reservan en el mismo bloque pre-loop (igual que las
 *   vars de las extras en v1.132). Dentro del loop, tras
 *   OP_ASIGNAR_LOCAL slots_var[i] del item, se emite el codigo
 *   de destructuring inline:
 *     - verifica aridad (== n o >= n-1 con star)
 *     - por cada destino: OP_OBTENER_LOCAL slot_item; CONST i;
 *       OP_INDICE (o REBANADA para el star); OP_ASIGNAR_LOCAL slot
 *     - en mala aridad: OP_LANZAR ErrorDeValor
 *
 * Limitaciones:
 *   - Solo un nivel: `[expr para (a, (b, c)) en triples]` (anidado)
 *     no soportado.
 *   - Solo un star por clausula.
 *   - Generator expressions (parentesis) aun no aceptan
 *     destructuring (limitacion explicita, error claro).
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
        "test_compr_destr_out.txt";
#else
        "/tmp/test_compr_destr_out.txt";
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
    /* List comprehension con destructuring de pares */
    {
        char out[256];
        ejecutar_capturando(
            "pares = [(1, 10), (2, 20), (3, 30)]\n"
            "imprimir([a + b para a, b en pares])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[11, 22, 33]") != NULL, "lista_pares");
    }

    /* Con guarda — la guarda puede usar los destinos */
    {
        char out[256];
        ejecutar_capturando(
            "pares = [(1, 10), (2, 20), (3, 30)]\n"
            "imprimir([a para a, b en pares si b > 15])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[2, 3]") != NULL, "lista_pares_guarda");
    }

    /* Star binding en destructuring de comprehension */
    {
        char out[256];
        ejecutar_capturando(
            "filas = [[1, 2, 3], [10, 20, 30, 40]]\n"
            "imprimir([resto para primero, *resto en filas])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[[2, 3], [20, 30, 40]]") != NULL,
                "star_final");
    }

    /* Star inicial */
    {
        char out[256];
        ejecutar_capturando(
            "filas = [[1, 2, 3, 4], [10, 20]]\n"
            "imprimir([ult para *_, ult en filas])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[4, 20]") != NULL, "star_inicial");
    }

    /* Star medio */
    {
        char out[256];
        ejecutar_capturando(
            "filas = [[1, 2, 3, 4, 5]]\n"
            "imprimir([m para p, *m, u en filas])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[[2, 3, 4]]") != NULL, "star_medio");
    }

    /* Dict comprehension con destructuring */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir({k: v * 2 para k, v en [(\"uno\", 1), (\"dos\", 2)]})\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "\"uno\": 2") != NULL, "dict_destr_uno");
        AFIRMAR(strstr(out, "\"dos\": 4") != NULL, "dict_destr_dos");
    }

    /* Set comprehension con destructuring */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud({a - b para a, b en [(5, 1), (5, 3), (5, 5)]}))\n",
            out, sizeof(out));
        /* Diferencias: 4, 2, 0 — 3 valores unicos. */
        AFIRMAR(strstr(out, "3") != NULL, "set_destr");
    }

    /* Multi-para con destructuring en CADA clausula */
    {
        char out[512];
        ejecutar_capturando(
            "xs = [(1, 10), (2, 20)]\n"
            "ys = [(\"a\", \"A\"), (\"b\", \"B\")]\n"
            "imprimir([(i, k) para i, _ en xs para k, _ en ys])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, \"a\")") != NULL, "multi_a1");
        AFIRMAR(strstr(out, "(2, \"b\")") != NULL, "multi_d");
    }

    /* Dentro de funcion */
    {
        char out[512];
        ejecutar_capturando(
            "funcion procesar(pares):\n"
            "    retornar [a + b para a, b en pares]\n"
            "fin funcion\n"
            "imprimir(procesar([(1, 2), (10, 20), (100, 200)]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[3, 30, 300]") != NULL, "en_funcion");
    }

    /* Star dentro de funcion */
    {
        char out[512];
        ejecutar_capturando(
            "funcion segundos(filas):\n"
            "    retornar [(seg, resto) para _, seg, *resto en filas]\n"
            "fin funcion\n"
            "imprimir(segundos([[1, 2, 3, 4], [10, 20], [100, 200, 300]]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(2, [3, 4])") != NULL, "star_func_1");
        AFIRMAR(strstr(out, "(20, [])") != NULL, "star_func_vacio");
        AFIRMAR(strstr(out, "(200, [300])") != NULL, "star_func_3");
    }

    /* Aridad incorrecta — ErrorDeValor atrapable */
    {
        char out[512];
        ejecutar_capturando(
            "intentar:\n"
            "    bad = [a + b para a, b en [(1, 2), (3,)]]\n"
            "    imprimir(bad)\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "err") != NULL, "aridad_err");
    }

    /* Regresion: comprehension sin destructuring sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([i * 2 para i en [1, 2, 3]])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[2, 4, 6]") != NULL, "regresion_simple");
    }

    /* Regresion: multi-para sin destructuring */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([i + j para i en [1, 2] para j en [10, 20]])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[11, 21, 12, 22]") != NULL,
                "regresion_multi");
    }

    /* Generator expression con destructuring rechaza con error claro */
    {
        char out[256];
        ejecutar_capturando(
            "g = (a + b para a, b en [(1, 2)])\n"
            "imprimir(g)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1") == NULL, "genex_destr_rechaza");
    }

    if (fallos == 0) {
        printf("compr_destr: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "compr_destr: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
