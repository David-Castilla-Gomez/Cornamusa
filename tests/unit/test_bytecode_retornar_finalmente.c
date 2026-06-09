/*
 * Tests de `retornar` dentro de `intentar` con `finalmente` (v1.190).
 *
 * Antes (limitacion documentada en v0.8.3): el finalmente NO se
 * ejecutaba cuando el retornar salia del intentar. Ahora se ejecutan
 * todos los finalmentes pendientes ANTES del retornar, en orden
 * inner-most-first (paridad Python).
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
        "test_retornar_finalmente_out.txt";
#else
        "/tmp/test_retornar_finalmente_out.txt";
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
    /* Basico: finalmente se ejecuta antes del return */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    intentar:\n"
            "        retornar 42\n"
            "    finalmente:\n"
            "        imprimir(\"cleanup\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "imprimir(f())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "cleanup\n42") != NULL, "basico");
    }

    /* Anidado: inner cleanup primero, luego outer */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    intentar:\n"
            "        intentar:\n"
            "            retornar 99\n"
            "        finalmente:\n"
            "            imprimir(\"inner\")\n"
            "        fin intentar\n"
            "    finalmente:\n"
            "        imprimir(\"outer\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "imprimir(f())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "inner\nouter\n99") != NULL, "anidado");
    }

    /* Retornar nulo */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    intentar:\n"
            "        retornar\n"
            "    finalmente:\n"
            "        imprimir(\"limpio\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "imprimir(f())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "limpio\nnulo") != NULL, "retornar_nulo");
    }

    /* Sin retornar: salida normal sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    intentar:\n"
            "        imprimir(\"body\")\n"
            "    finalmente:\n"
            "        imprimir(\"clean\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "f()\n"
            "imprimir(\"post\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "body\nclean\npost") != NULL, "salida_normal");
    }

    /* Con atrapar — el flujo de excepcion sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    intentar:\n"
            "        intentar:\n"
            "            lanzar ErrorDeValor(\"oops\")\n"
            "        finalmente:\n"
            "            imprimir(\"inner\")\n"
            "        fin intentar\n"
            "    atrapar ErrorDeValor:\n"
            "        imprimir(\"atrapado\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "f()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "inner\natrapado") != NULL, "excepcion");
    }

    /* Retornar valor calculado */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f(x):\n"
            "    intentar:\n"
            "        retornar x * 2 + 1\n"
            "    finalmente:\n"
            "        imprimir(\"clean\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "imprimir(f(5))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "clean\n11") != NULL, "valor_calculado");
    }

    /* Finalmente con efectos en el local del finalmente */
    {
        char out[256];
        ejecutar_capturando(
            "estado = []\n"
            "funcion f():\n"
            "    intentar:\n"
            "        retornar 1\n"
            "    finalmente:\n"
            "        estado.agregar(\"x\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "imprimir(f())\n"
            "imprimir(estado)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "1\n[\"x\"]") != NULL, "efectos");
    }

    if (fallos == 0) {
        printf("retornar_finalmente: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "retornar_finalmente: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
