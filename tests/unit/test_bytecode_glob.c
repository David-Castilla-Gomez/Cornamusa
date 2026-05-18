/*
 * Tests del matcher glob y de la busqueda recursiva en stdlib/ruta
 * (v1.100).
 *
 * Glob soporta solo `*` (cero o mas caracteres) y `?` (uno). No
 * soporta `[abc]`, `**`, alternancias. Para mas necesidades usar
 * stdlib/regex.
 *
 * `recorrer(dir)` devuelve lista de Rutas recursivas DFS.
 * `encontrar(dir, patron)` filtra por glob sobre el nombre.
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
        "test_glob_out.txt";
#else
        "/tmp/test_glob_out.txt";
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
    /* Glob basico: extension */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(ruta.Ruta(\"hola.cor\").coincide(\"*.cor\"))\n"
            "imprimir(ruta.Ruta(\"hola.cor\").coincide(\"*.txt\"))\n"
            "imprimir(ruta.Ruta(\"foo.tar.gz\").coincide(\"*.gz\"))\n"
            "imprimir(ruta.Ruta(\"hola.cor\").coincide(\"*\"))\n", out, sizeof(out));
        /* V, F, V, V */
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v == 3 && n_f == 1, "glob_extension_basico");
    }

    /* Glob con ? (un caracter) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(ruta.Ruta(\"abc\").coincide(\"???\"))\n"
            "imprimir(ruta.Ruta(\"abc\").coincide(\"??\"))\n"
            "imprimir(ruta.Ruta(\"abc\").coincide(\"????\"))\n"
            "imprimir(ruta.Ruta(\"abc\").coincide(\"a?c\"))\n"
            "imprimir(ruta.Ruta(\"abc\").coincide(\"a?d\"))\n", out, sizeof(out));
        /* V, F, F, V, F */
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v == 2 && n_f == 3, "glob_interrogacion");
    }

    /* Glob con prefijo + asterisco intermedio */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(ruta.Ruta(\"test_main.c\").coincide(\"test_*.c\"))\n"
            "imprimir(ruta.Ruta(\"main_test.c\").coincide(\"test_*.c\"))\n"
            "imprimir(ruta.Ruta(\"test_.c\").coincide(\"test_*.c\"))\n"
            "imprimir(ruta.Ruta(\"test_largo_nombre.c\").coincide(\"test_*.c\"))\n",
            out, sizeof(out));
        /* V, F, V (*=vacio match), V */
        int n_v = 0, n_f = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        p = out;
        while ((p = strstr(p, "falso")) != NULL) { n_f++; p++; }
        AFIRMAR(n_v == 3 && n_f == 1, "glob_prefijo_asterisco");
    }

    /* Glob: patron solo asteriscos matchea cualquier cosa, vacios incluso */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(ruta.Ruta(\"\").coincide(\"*\"))\n"
            "imprimir(ruta.Ruta(\"x\").coincide(\"*\"))\n"
            "imprimir(ruta.Ruta(\"x\").coincide(\"**\"))\n", out, sizeof(out));
        int n_v = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        AFIRMAR(n_v == 3, "glob_solo_asteriscos");
    }

    /* recorrer un directorio: cuenta esperada (en repo: examples/
     * tiene >40 .cor files, no hay subdirs) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(longitud(ruta.recorrer(\"examples\")) > 50)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "recorrer_examples_mas_de_50");
    }

    /* encontrar *.cor en examples */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "cors = ruta.encontrar(\"examples\", \"*.cor\")\n"
            "imprimir(longitud(cors) > 50)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "encontrar_cor_examples");
    }

    /* encontrar recursivo: test_*.c en tests/ (busca en tests/unit/) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "tests = ruta.encontrar(\"tests\", \"test_*.c\")\n"
            "imprimir(longitud(tests) > 10)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "encontrar_recursivo");
    }

    /* recorrer sobre archivo (no directorio) -> lista vacia */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "imprimir(longitud(ruta.recorrer(\"README.md\")))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "0") != NULL, "recorrer_archivo_lista_vacia");
    }

    /* Metodos sobre Ruta */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "r = ruta.Ruta(\"stdlib\")\n"
            "cors = r.encontrar(\"*.cor\")\n"
            "imprimir(longitud(cors) > 15)\n"
            "todos = r.recorrer()\n"
            "imprimir(longitud(todos) > 15)\n", out, sizeof(out));
        int n_v = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        AFIRMAR(n_v == 2, "metodos_ruta_recorrer_encontrar");
    }

    /* encontrar devuelve Rutas (no cadenas) que se pueden encadenar */
    {
        char out[2048];
        ejecutar_capturando(
            "importar ruta\n"
            "rs = ruta.encontrar(\"stdlib\", \"*.cor\")\n"
            "primero = rs[0]\n"
            "imprimir(tipo(primero))\n"
            "imprimir(primero.es_archivo())\n", out, sizeof(out));
        AFIRMAR(strstr(out, "instancia") != NULL, "encontrar_devuelve_ruta");
        AFIRMAR(strstr(out, "verdadero") != NULL, "ruta_encadenable");
    }

    if (fallos == 0) {
        printf("glob: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "glob: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
