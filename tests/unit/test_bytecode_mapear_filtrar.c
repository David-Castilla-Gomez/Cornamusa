/*
 * Tests de mapear/filtrar como builtins globales (v1.195).
 *
 * Hito de infraestructura: primera vez que las nativas C pueden
 * invocar callables Cornamusa, via el hook InvocadorCallable que la
 * VM registra en vm_iniciar (vm_invocar_callable_sync — sub-dispatch
 * sincrono, mismo mecanismo que __hash__/__igual__).
 *
 * Soporta lambdas, funciones nombradas, closures con captura y
 * nativas como callable. Excepciones del callable propagan.
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
        "test_mapear_filtrar_out.txt";
#else
        "/tmp/test_mapear_filtrar_out.txt";
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
    /* mapear con lambda */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(mapear(lambda x: x * 2, [1, 2, 3]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[2, 4, 6]") != NULL, "mapear_lambda");
    }

    /* mapear con funcion nombrada sobre rango */
    {
        char out[256];
        ejecutar_capturando(
            "funcion cuadrado(n):\n"
            "    retornar n * n\n"
            "fin funcion\n"
            "imprimir(mapear(cuadrado, rango(5)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 4, 9, 16]") != NULL, "mapear_funcion");
    }

    /* mapear con NATIVA como callable */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(mapear(cadena, [1, 2, 3]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"1\", \"2\", \"3\"]") != NULL, "mapear_nativa");
    }

    /* filtrar */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(filtrar(lambda x: x % 2 == 0, rango(10)))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 2, 4, 6, 8]") != NULL, "filtrar_pares");
    }

    /* filtrar con predicado sobre cadenas */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(filtrar(lambda s: longitud(s) > 2, [\"a\", \"abc\", \"xy\", \"wxyz\"]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[\"abc\", \"wxyz\"]") != NULL, "filtrar_cadenas");
    }

    /* Encadenado con suma: suma de cuadrados de pares < 10 */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(suma(mapear(lambda x: x * x, filtrar(lambda x: x % 2 == 0, rango(10)))))\n",
            out, sizeof(out));
        /* 0 + 4 + 16 + 36 + 64 = 120 */
        AFIRMAR(strstr(out, "120") != NULL, "encadenado");
    }

    /* Closure con captura de variable externa */
    {
        char out[256];
        ejecutar_capturando(
            "factor = 10\n"
            "imprimir(mapear(lambda x: x * factor, [1, 2]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[10, 20]") != NULL, "closure_captura");
    }

    /* Excepcion en el callable propaga y es atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "funcion explota(x):\n"
            "    lanzar ErrorDeValor(\"boom\")\n"
            "fin funcion\n"
            "intentar:\n"
            "    mapear(explota, [1])\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "excepcion_propaga");
    }

    /* Iterable vacio */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(mapear(lambda x: x, []))\n"
            "imprimir(filtrar(lambda x: verdadero, []))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]\n[]") != NULL, "vacios");
    }

    /* funcionales.mapear sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "imprimir(funcionales.mapear(lambda x: x + 1, [5]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[6]") != NULL, "modulo_compat");
    }

    /* Funcion con default como callable */
    {
        char out[256];
        ejecutar_capturando(
            "funcion conk(x, k=100):\n"
            "    retornar x + k\n"
            "fin funcion\n"
            "imprimir(mapear(conk, [1, 2]))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[101, 102]") != NULL, "callable_con_default");
    }

    /* Tipo no callable -> error atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    mapear(42, [1])\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "no_callable");
    }

    if (fallos == 0) {
        printf("mapear_filtrar: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "mapear_filtrar: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
