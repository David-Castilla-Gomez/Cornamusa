/*
 * Tests de `copia(x)` y `copia_profunda(x)` (v1.165).
 *
 * copia(): nuevo contenedor con referencias a elementos
 * (mutar la copia no afecta al original, pero mutar un sub-objeto
 * del original SI se ve en la copia).
 *
 * copia_profunda(): recursivo, totalmente independiente. Tolera
 * ciclos via memoizador.
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
        "test_copia_out.txt";
#else
        "/tmp/test_copia_out.txt";
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
    /* copia(lista): nuevo contenedor; mutar copia no afecta original */
    {
        char out[256];
        ejecutar_capturando(
            "a = [1, 2, 3]\n"
            "b = copia(a)\n"
            "b.agregar(4)\n"
            "imprimir(a)\n"
            "imprimir(b)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]\n[1, 2, 3, 4]") != NULL, "lista_indep");
    }

    /* copia(lista): shallow — elementos siguen compartidos */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [[1, 2], [3, 4]]\n"
            "ys = copia(xs)\n"
            "ys[0].agregar(99)\n"
            "imprimir(xs)\n"
            "imprimir(ys)\n",
            out, sizeof(out));
        /* Las dos sub-listas son compartidas */
        AFIRMAR(strstr(out, "[[1, 2, 99], [3, 4]]\n[[1, 2, 99], [3, 4]]") != NULL,
                "shallow_comparte_subs");
    }

    /* copia_profunda(lista): independencia total */
    {
        char out[256];
        ejecutar_capturando(
            "xs = [[1, 2], [3, 4]]\n"
            "zs = copia_profunda(xs)\n"
            "zs[0].agregar(99)\n"
            "imprimir(xs)\n"
            "imprimir(zs)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[[1, 2], [3, 4]]\n[[1, 2, 99], [3, 4]]") != NULL,
                "deep_independiente");
    }

    /* copia(dicc) */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"k\": 1, \"j\": 2}\n"
            "d2 = copia(d)\n"
            "d2[\"k\"] = 99\n"
            "imprimir(d[\"k\"])\n"
            "imprimir(d2[\"k\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1\n99") != NULL, "dicc_indep");
    }

    /* copia(dicc) shallow: valores compartidos */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"k\": [1, 2]}\n"
            "d2 = copia(d)\n"
            "d2[\"k\"].agregar(99)\n"
            "imprimir(d[\"k\"])\n"
            "imprimir(d2[\"k\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 99]\n[1, 2, 99]") != NULL,
                "dicc_shallow_comparte_val");
    }

    /* copia_profunda(dicc) */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"k\": [1, 2]}\n"
            "d3 = copia_profunda(d)\n"
            "d3[\"k\"].agregar(99)\n"
            "imprimir(d[\"k\"])\n"
            "imprimir(d3[\"k\"])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2]\n[1, 2, 99]") != NULL,
                "dicc_deep_independiente");
    }

    /* copia(conjunto) */
    {
        char out[256];
        ejecutar_capturando(
            "s = {1, 2, 3}\n"
            "s2 = copia(s)\n"
            "s2.agregar(99)\n"
            "imprimir(longitud(s))\n"
            "imprimir(longitud(s2))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3\n4") != NULL, "conjunto_indep");
    }

    /* copia(frozen) preserva frozen */
    {
        char out[256];
        ejecutar_capturando(
            "f = congelar({1, 2})\n"
            "f2 = copia(f)\n"
            "funcion p():\n"
            "    intentar:\n"
            "        f2.agregar(3)\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "copia_de_frozen_sigue_frozen");
    }

    /* copia de inmutable es no-op semantico */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(copia(42))\n"
            "imprimir(copia(\"hola\"))\n"
            "imprimir(copia((1, 2, 3)))\n"
            "imprimir(copia(nulo))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "42\nhola\n(1, 2, 3)\nnulo") != NULL, "inmutables");
    }

    /* Ciclos: copia_profunda NO cuelga */
    {
        char out[256];
        ejecutar_capturando(
            "ciclo = [1, 2]\n"
            "ciclo.agregar(ciclo)\n"
            "c2 = copia_profunda(ciclo)\n"
            "imprimir(longitud(c2))\n"
            "imprimir(c2[0])\n"
            "imprimir(c2[1])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3\n1\n2") != NULL, "ciclo_sin_cuelgue");
    }

    /* copia_profunda de tupla anidada */
    {
        char out[256];
        ejecutar_capturando(
            "t = ([1, 2], [3, 4])\n"
            "t2 = copia_profunda(t)\n"
            "t2[0].agregar(99)\n"
            "imprimir(t[0])\n"
            "imprimir(t2[0])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2]\n[1, 2, 99]") != NULL, "tupla_deep");
    }

    /* copia_profunda de dicc con clave tupla */
    {
        char out[256];
        ejecutar_capturando(
            "d = {(1, 2): [10, 20]}\n"
            "d2 = copia_profunda(d)\n"
            "d2[(1, 2)].agregar(99)\n"
            "imprimir(d[(1, 2)])\n"
            "imprimir(d2[(1, 2)])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[10, 20]\n[10, 20, 99]") != NULL,
                "dicc_clave_tupla");
    }

    /* Aridad */
    {
        char out[256];
        ejecutar_capturando(
            "intentar:\n"
            "    copia()\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok1\")\n"
            "fin intentar\n"
            "intentar:\n"
            "    copia_profunda(1, 2)\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"ok2\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok1") != NULL, "aridad_copia");
        AFIRMAR(strstr(out, "ok2") != NULL, "aridad_profunda");
    }

    if (fallos == 0) {
        printf("copia: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "copia: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
