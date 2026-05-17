/*
 * Tests de los helpers nuevos en stdlib/funcionales.cor (v1.87):
 * agrupar_por, tomar, saltar, combinar, aplanar, unicos.
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
        "test_func87_out.txt";
#else
        "/tmp/test_func87_out.txt";
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
    /* agrupar_por: pares/impares */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "g = funcionales.agrupar_por([1,2,3,4,5,6], lambda x: x % 2)\n"
            "imprimir(g)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "1: [1, 3, 5]") != NULL, "agrupar_impares");
        AFIRMAR(strstr(out, "0: [2, 4, 6]") != NULL, "agrupar_pares");
    }

    /* agrupar_por: por primera letra */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "g = funcionales.agrupar_por([\"ana\",\"alex\",\"bea\",\"ben\"], lambda s: s[0])\n"
            "imprimir(g)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "\"a\":") != NULL, "agrupar_por_letra_a");
        AFIRMAR(strstr(out, "\"b\":") != NULL, "agrupar_por_letra_b");
    }

    /* tomar */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.tomar(3, [10,20,30,40,50]))\n"
            "imprimir(funcionales.tomar(100, [1,2,3]))\n"
            "imprimir(funcionales.tomar(0, [1,2,3]))\n"
            "imprimir(funcionales.tomar(-5, [1,2,3]))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[10, 20, 30]") != NULL, "tomar_normal");
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "tomar_mas_que_disponibles");
        /* tomar(0) y tomar(-N) ambos devuelven []. */
        int n_vacios = 0;
        const char *p = out;
        while ((p = strstr(p, "[]")) != NULL) { n_vacios++; p++; }
        AFIRMAR(n_vacios == 2, "tomar_0_y_negativo_vacios");
    }

    /* tomar funciona con generadores (rango) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.tomar(5, rango(1000)))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 2, 3, 4]") != NULL, "tomar_rango");
    }

    /* saltar */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.saltar(2, [1,2,3,4,5]))\n"
            "imprimir(funcionales.saltar(100, [1,2,3]))\n"
            "imprimir(funcionales.saltar(0, [1,2,3]))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[3, 4, 5]") != NULL, "saltar_normal");
        AFIRMAR(strstr(out, "[]") != NULL, "saltar_mas_que_disponibles");
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "saltar_0_devuelve_todo");
    }

    /* combinar (zip) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.combinar([\"a\",\"b\",\"c\"], [1,2,3]))\n"
            "imprimir(funcionales.combinar([1,2,3,4], [\"x\",\"y\"]))\n"
            "imprimir(funcionales.combinar([], [1,2,3]))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "(\"a\", 1)") != NULL, "combinar_par_1");
        AFIRMAR(strstr(out, "(1, \"x\")") != NULL, "combinar_corta_se_para");
        AFIRMAR(strstr(out, "[]") != NULL, "combinar_uno_vacio");
    }

    /* aplanar */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.aplanar([[1,2],[3,4],[5]]))\n"
            "imprimir(funcionales.aplanar([]))\n"
            "imprimir(funcionales.aplanar([[]]))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3, 4, 5]") != NULL, "aplanar_basico");
    }

    /* unicos */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.unicos([3,1,4,1,5,9,2,6,5,3]))\n"
            "imprimir(funcionales.unicos([]))\n"
            "imprimir(funcionales.unicos([1,1,1,1]))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "[3, 1, 4, 5, 9, 2, 6]") != NULL,
                "unicos_preserva_orden");
        AFIRMAR(strstr(out, "[1]") != NULL, "unicos_todos_iguales");
    }

    /* Composicion: agrupar + ordenar grupos por tamaño. */
    {
        char out[1024];
        ejecutar_capturando(
            "importar funcionales\n"
            "datos = [\"ana\",\"bea\",\"ben\",\"alex\",\"alba\"]\n"
            "g = funcionales.agrupar_por(datos, lambda s: s[0])\n"
            "# Cada grupo tiene su tamaño:\n"
            "para k en claves(g):\n"
            "    imprimir(k, longitud(g[k]))\n"
            "fin para\n", out, sizeof(out));
        AFIRMAR(strstr(out, "a 3") != NULL, "composicion_a_3");
        AFIRMAR(strstr(out, "b 2") != NULL, "composicion_b_2");
    }

    if (fallos == 0) {
        printf("funcionales_v87: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "funcionales_v87: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
