/*
 * Tests de `enumerar(iterable, inicio=0)` como builtin global (v1.192).
 *
 * Antes solo existia en stdlib/funcionales (requeria importar). Es el
 * idiom mas comun en bucles, asi que se promueve a builtin como rango.
 *
 * Devuelve lista de tuplas (indice, elemento). Eager.
 * La version del modulo funcionales sigue funcionando (sombra propia).
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
        "test_enumerar_builtin_out.txt";
#else
        "/tmp/test_enumerar_builtin_out.txt";
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
    /* Sin import, con destructuring en para */
    {
        char out[256];
        ejecutar_capturando(
            "para i, x en enumerar([\"a\", \"b\", \"c\"]):\n"
            "    imprimir(i, x)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0 a\n1 b\n2 c") != NULL, "lista_basico");
    }

    /* Inicio custom */
    {
        char out[256];
        ejecutar_capturando(
            "para i, x en enumerar([\"x\", \"z\"], 1):\n"
            "    imprimir(i, x)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1 x\n2 z") != NULL, "inicio_custom");
    }

    /* Cadena con UTF-8 multibyte */
    {
        char out[256];
        ejecutar_capturando(
            "para i, ch en enumerar(\"\xc3\xb1u\"):\n"  /* ñu */
            "    imprimir(i, ch)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0 \xc3\xb1\n1 u") != NULL, "cadena_utf8");
    }

    /* Rango */
    {
        char out[256];
        ejecutar_capturando(
            "para i, v en enumerar(rango(10, 13)):\n"
            "    imprimir(i, v)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0 10\n1 11\n2 12") != NULL, "rango");
    }

    /* Tupla */
    {
        char out[256];
        ejecutar_capturando(
            "para i, v en enumerar((7, 8)):\n"
            "    imprimir(i, v)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0 7\n1 8") != NULL, "tupla");
    }

    /* Dicc itera claves */
    {
        char out[256];
        ejecutar_capturando(
            "para i, k en enumerar({\"a\": 1, \"b\": 2}):\n"
            "    imprimir(i, k)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0 a\n1 b") != NULL, "dicc_claves");
    }

    /* Vacio */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(longitud(enumerar([])))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "vacio");
    }

    /* La version del modulo funcionales sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "importar funcionales\n"
            "para i, x en funcionales.enumerar([\"m\"]):\n"
            "    imprimir(i, x)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0 m") != NULL, "modulo_compat");
    }

    /* Como expresion (resultado es lista de tuplas) */
    {
        char out[256];
        ejecutar_capturando(
            "pares = enumerar([\"a\", \"b\"])\n"
            "imprimir(pares)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[(0, \"a\"), (1, \"b\")]") != NULL,
                "como_expresion");
    }

    /* Errores: tipo no iterable */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    enumerar(42)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "no_iterable");
    }

    /* Errores: inicio no entero */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    enumerar([1], \"x\")\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "inicio_invalido");
    }

    if (fallos == 0) {
        printf("enumerar_builtin: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "enumerar_builtin: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
