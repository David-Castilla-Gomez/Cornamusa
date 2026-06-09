/*
 * Tests de `romper`/`continuar` dentro de `intentar` con `finalmente`
 * (v1.191). Cierra la otra limitacion del v1.190.
 *
 * Antes el finalmente NO se ejecutaba cuando romper/continuar salian
 * del intentar (hacia el bucle externo). Ahora se ejecutan los
 * finalmentes que estan ENTRE el sitio del romper y el bucle objetivo.
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
        "test_romper_finalmente_out.txt";
#else
        "/tmp/test_romper_finalmente_out.txt";
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
    /* romper ejecuta finalmente antes de salir del bucle */
    {
        char out[512];
        ejecutar_capturando(
            "para n en rango(4):\n"
            "    intentar:\n"
            "        si n == 2:\n"
            "            romper\n"
            "        fin si\n"
            "        imprimir(\"body\", n)\n"
            "    finalmente:\n"
            "        imprimir(\"clean\", n)\n"
            "    fin intentar\n"
            "fin para\n"
            "imprimir(\"post\")\n",
            out, sizeof(out));
        /* n=0: body, clean. n=1: body, clean. n=2: clean, romper. */
        AFIRMAR(strstr(out, "body 0\nclean 0\nbody 1\nclean 1\nclean 2\npost") != NULL,
                "romper");
    }

    /* continuar ejecuta finalmente antes de la siguiente iter */
    {
        char out[512];
        ejecutar_capturando(
            "para n en rango(3):\n"
            "    intentar:\n"
            "        si n == 1:\n"
            "            continuar\n"
            "        fin si\n"
            "        imprimir(\"body\", n)\n"
            "    finalmente:\n"
            "        imprimir(\"clean\", n)\n"
            "    fin intentar\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "body 0\nclean 0\nclean 1\nbody 2\nclean 2") != NULL,
                "continuar");
    }

    /* Bucle anidado dentro de intentar — romper sale del bucle pero
     * el finalmente exterior se ejecuta al final del intentar normal */
    {
        char out[512];
        ejecutar_capturando(
            "intentar:\n"
            "    para n en rango(3):\n"
            "        si n == 1:\n"
            "            romper\n"
            "        fin si\n"
            "        imprimir(\"inner\", n)\n"
            "    fin para\n"
            "    imprimir(\"post_para\")\n"
            "finalmente:\n"
            "    imprimir(\"outer_clean\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "inner 0\npost_para\nouter_clean") != NULL,
                "bucle_en_intentar");
    }

    /* Doble anidamiento: romper del bucle externo desde intentar interno */
    {
        char out[512];
        ejecutar_capturando(
            "para a en rango(2):\n"
            "    para b en rango(3):\n"
            "        intentar:\n"
            "            si b == 1:\n"
            "                romper\n"
            "            fin si\n"
            "            imprimir(\"AB\", a, b)\n"
            "        finalmente:\n"
            "            imprimir(\"clean\", a, b)\n"
            "        fin intentar\n"
            "    fin para\n"
            "fin para\n",
            out, sizeof(out));
        /* a=0: b=0 (AB 0 0, clean 0 0), b=1 (clean 0 1, romper).
         * a=1: b=0 (AB 1 0, clean 1 0), b=1 (clean 1 1, romper). */
        AFIRMAR(strstr(out, "AB 0 0\nclean 0 0\nclean 0 1\nAB 1 0\nclean 1 0\nclean 1 1") != NULL,
                "doble");
    }

    /* Sin romper/continuar dentro: regresión, finalmente normal */
    {
        char out[256];
        ejecutar_capturando(
            "para n en rango(2):\n"
            "    intentar:\n"
            "        imprimir(\"body\", n)\n"
            "    finalmente:\n"
            "        imprimir(\"clean\", n)\n"
            "    fin intentar\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "body 0\nclean 0\nbody 1\nclean 1") != NULL,
                "regresion");
    }

    if (fallos == 0) {
        printf("romper_finalmente: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "romper_finalmente: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
