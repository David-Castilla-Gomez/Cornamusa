/*
 * Tests de comprehensions en CUALQUIER posicion de expresion (v1.198).
 *
 * Bug arreglado: una comprehension compilada con valores temporales
 * en el stack (callee de llamada, operando izquierdo de binario,
 * acumulador de f-string...) calculaba sus slots $comp_acc/$comp_iter
 * desfasados respecto al stack runtime → "estado interno corrupto:
 * OP_ITER_SIGUIENTE sin iterador en slot N".
 *
 * Fix estructural: el compilador trackea `prof_expr` (temporales no
 * registrados como locales) via wrapper de compilador_compilar_expr;
 * EXPR_COMPREHENSION reserva ese numero de slots fantasma para
 * alinear sus slots con el stack real. Compensaciones manuales en
 * los sitios con pushes directos no trackeados (f-string, kwargs,
 * paths spread) o consumos pre-subcompilacion (ternaria, logica).
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
        "test_comp_en_expr_out.txt";
#else
        "/tmp/test_comp_en_expr_out.txt";
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
    /* Comprehension como arg de llamada en funcion (callee huerfano) */
    {
        char out[256];
        ejecutar_capturando(
            "funcion fa(xs):\n"
            "    retornar suma([v * 2 para v en xs])\n"
            "fin funcion\n"
            "imprimir(fa([1, 2, 3]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "12") != NULL, "arg_en_funcion");
    }

    /* En top-level */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(suma([v para v en [5, 6]]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "11") != NULL, "arg_top_level");
    }

    /* Operando derecho de binario */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(100 + suma([v para v en [1, 2]]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "103") != NULL, "binario");
    }

    /* Method call + *args (el repro original de v1.182) */
    {
        char out[256];
        ejecutar_capturando(
            "funcion test1(prefijo, *args):\n"
            "    cuerpo = \", \".unir([cadena(x) para x en args])\n"
            "    retornar prefijo + \": \" + cuerpo\n"
            "fin funcion\n"
            "imprimir(test1(\"nums\", 1, 2, 3))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "nums: 1, 2, 3") != NULL, "method_args");
    }

    /* Spread con comprehension inline (v1.177 sigue OK) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([*[x*x para x en rango(4)], 100])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 4, 9, 100]") != NULL, "spread_regresion");
    }

    /* Kwarg con comprehension */
    {
        char out[256];
        ejecutar_capturando(
            "funcion fk(a, claves=nulo):\n"
            "    retornar (a, claves)\n"
            "fin funcion\n"
            "imprimir(fk(1, claves=[k para k en \"ab\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, [\"a\", \"b\"])") != NULL, "kwarg");
    }

    /* F-string con partes previas */
    {
        char out[256];
        ejecutar_capturando(
            "n = 3\n"
            "imprimir(f\"antes {[x para x en rango(n)]} despues\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "antes [0, 1, 2] despues") != NULL, "fstring");
    }

    /* Ternaria con comprehension en rama */
    {
        char out[256];
        ejecutar_capturando(
            "v = verdadero\n"
            "imprimir([x para x en [1, 2]] si v sino [])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2]") != NULL, "ternaria");
    }

    /* Logica con comprehension a la derecha */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(falso o longitud([z para z en [1]]) > 0)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "logica");
    }

    /* Comprehension anidada como arg dentro de comprehension */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([suma([j para j en rango(i)]) para i en rango(4)])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 0, 1, 3]") != NULL, "anidada");
    }

    /* Dict y set comprehension como arg */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud({k: 1 para k en \"abc\"}))\n"
            "imprimir(longitud({c para c en \"aabbc\"}))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3\n3") != NULL, "dict_set_comp");
    }

    /* Indice con comprehension */
    {
        char out[256];
        ejecutar_capturando(
            "matriz = [[1, 2], [3, 4]]\n"
            "imprimir(matriz[suma([v para v en [0, 1]])])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[3, 4]") != NULL, "indice");
    }

    /* Asignacion normal sigue OK (regresion del caso comun) */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [x * 3 para x en rango(3)]\n"
            "imprimir(xs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 3, 6]") != NULL, "asignacion_regresion");
    }

    if (fallos == 0) {
        printf("comp_en_expr: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "comp_en_expr: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
