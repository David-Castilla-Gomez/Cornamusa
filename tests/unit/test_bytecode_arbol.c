/*
 * Tests de archivos.eliminar_arbol (rm -rf) y archivos.crear_arbol
 * (mkdir -p) v1.102. Pure-Cornamusa sobre las nativas FS de v1.97/v1.99.
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
        "test_arbol_out.txt";
#else
        "/tmp/test_arbol_out.txt";
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
    /* crear_arbol con varios niveles */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.crear_arbol(\"_t_arb_a/x/y/z\")\n"
            "imprimir(archivos.es_directorio(\"_t_arb_a\"))\n"
            "imprimir(archivos.es_directorio(\"_t_arb_a/x\"))\n"
            "imprimir(archivos.es_directorio(\"_t_arb_a/x/y\"))\n"
            "imprimir(archivos.es_directorio(\"_t_arb_a/x/y/z\"))\n"
            /* cleanup */
            "archivos.eliminar_arbol(\"_t_arb_a\")\n", out, sizeof(out));
        int n_v = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        AFIRMAR(n_v == 4, "crear_arbol_cuatro_niveles");
    }

    /* crear_arbol idempotente */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.crear_arbol(\"_t_arb_b\")\n"
            "archivos.crear_arbol(\"_t_arb_b\")\n"
            "imprimir(archivos.es_directorio(\"_t_arb_b\"))\n"
            "archivos.eliminar_arbol(\"_t_arb_b\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "crear_arbol_idempotente");
    }

    /* eliminar_arbol recursivo con archivos dentro */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.crear_arbol(\"_t_arb_c/sub1/sub2\")\n"
            "archivos.escribir(\"_t_arb_c/raiz.txt\", \"a\")\n"
            "archivos.escribir(\"_t_arb_c/sub1/medio.txt\", \"b\")\n"
            "archivos.escribir(\"_t_arb_c/sub1/sub2/hoja.txt\", \"c\")\n"
            "imprimir(archivos.existe(\"_t_arb_c/raiz.txt\"))\n"
            "archivos.eliminar_arbol(\"_t_arb_c\")\n"
            "imprimir(archivos.es_directorio(\"_t_arb_c\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "eliminar_arbol_pre_existe");
        AFIRMAR(strstr(out, "falso") != NULL, "eliminar_arbol_post_no_existe");
    }

    /* eliminar_arbol guardrails */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "intentar:\n"
            "    archivos.eliminar_arbol(\"\")\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"err vacia\")\n"
            "fin intentar\n"
            "intentar:\n"
            "    archivos.eliminar_arbol(\"/\")\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"err raiz\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err vacia") != NULL, "guardrail_vacia");
        AFIRMAR(strstr(out, "err raiz") != NULL, "guardrail_raiz");
    }

    /* eliminar_arbol sobre archivo regular (fallback) */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.escribir(\"_t_arb_d.txt\", \"x\")\n"
            "archivos.eliminar_arbol(\"_t_arb_d.txt\")\n"
            "imprimir(archivos.existe(\"_t_arb_d.txt\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "falso") != NULL, "eliminar_arbol_archivo_fallback");
    }

    /* eliminar_arbol sobre ruta inexistente lanza */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "intentar:\n"
            "    archivos.eliminar_arbol(\"_no_existe_arbol\")\n"
            "atrapar ErrorDeIO como e:\n"
            "    imprimir(\"err io\")\n"
            "fin intentar\n", out, sizeof(out));
        AFIRMAR(strstr(out, "err io") != NULL, "eliminar_arbol_inexistente");
    }

    /* Metodos en Ruta */
    {
        char out[1024];
        ejecutar_capturando(
            "importar ruta\n"
            "importar archivos\n"
            "r = ruta.Ruta(\"_t_arb_e/p/q\")\n"
            "r.crear_arbol()\n"
            "imprimir(r.es_directorio())\n"
            "ruta.Ruta(\"_t_arb_e\").eliminar_arbol()\n"
            "imprimir(archivos.es_directorio(\"_t_arb_e\"))\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "ruta_crear_arbol");
        AFIRMAR(strstr(out, "falso") != NULL, "ruta_eliminar_arbol");
    }

    /* crear_arbol acepta separadores \\ y / mezclados */
    {
        char out[1024];
        ejecutar_capturando(
            "importar archivos\n"
            "archivos.crear_arbol(\"_t_arb_f\\\\sub\\\\dir\")\n"
            "imprimir(archivos.es_directorio(\"_t_arb_f/sub/dir\"))\n"
            "archivos.eliminar_arbol(\"_t_arb_f\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "crear_arbol_backslash");
    }

    if (fallos == 0) {
        printf("arbol: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "arbol: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
