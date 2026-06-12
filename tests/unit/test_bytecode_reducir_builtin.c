/*
 * Tests de reducir(f, iterable, inicial?) como builtin global (v1.197).
 * Completa la triada map/filter/reduce sobre el invocador de v1.195.
 *
 * Semantica functools.reduce de Python:
 *   - Sin inicial: primer elemento como semilla. Vacio -> ErrorDeValor.
 *   - Con inicial: semilla explicita; vacio devuelve inicial.
 *   - f recibe (acumulado, elemento).
 *   - Con un solo elemento sin inicial, f NO se invoca.
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
        "test_reducir_builtin_out.txt";
#else
        "/tmp/test_reducir_builtin_out.txt";
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
    /* Sin inicial: primer elemento como semilla */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(reducir(lambda a, b: a + b, [1, 2, 3, 4]))\n"
            "imprimir(reducir(lambda a, b: a * b, [1, 2, 3, 4, 5]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10\n120") != NULL, "sin_inicial");
    }

    /* Con inicial */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(reducir(lambda a, b: a + b, [1, 2, 3], 100))\n"
            "imprimir(reducir(lambda a, b: a + b, [], 42))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "106\n42") != NULL, "con_inicial");
    }

    /* Orden de args: (acumulado, elemento) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(reducir(lambda acc, x: acc + cadena(x), [1, 2, 3], \"\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "123") != NULL, "orden_args");
    }

    /* Sobre rango y cadena */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(reducir(lambda a, b: a + b, rango(1, 11)))\n"
            "imprimir(reducir(lambda a, b: b + a, \"abc\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "55\ncba") != NULL, "rango_y_cadena");
    }

    /* Vacio sin inicial -> ErrorDeValor */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    reducir(lambda a, b: a, [])\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "vacio_sin_inicial");
    }

    /* Un elemento sin inicial: f NO se invoca */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(reducir(lambda a, b: a / 0, [99]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "99") != NULL, "singleton_no_invoca");
    }

    /* Excepcion de f propaga */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    reducir(lambda a, b: a / 0, [1, 2])\n"
            "atrapar ErrorAritmetico:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "excepcion_propaga");
    }

    /* Funcion nombrada como f */
    {
        char out[256];
        ejecutar_capturando(
            "funcion mayor(a, b):\n"
            "    si a > b:\n"
            "        retornar a\n"
            "    sino:\n"
            "        retornar b\n"
            "    fin si\n"
            "fin funcion\n"
            "imprimir(reducir(mayor, [3, 7, 2, 5]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "7") != NULL, "funcion_nombrada");
    }

    /* Composicion con otros builtins */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(reducir(lambda a, b: a + b, mapear(lambda x: x * x, rango(4))))\n",
            out, sizeof(out));
        /* 0 + 1 + 4 + 9 = 14 */
        AFIRMAR(strstr(out, "14") != NULL, "composicion");
    }

    /* funcionales.reducir sigue (con inicial obligatorio) */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.reducir(lambda a, b: a + b, [1, 2], 10))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "13") != NULL, "modulo_compat");
    }

    if (fallos == 0) {
        printf("reducir_builtin: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "reducir_builtin: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
