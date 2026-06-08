/*
 * Tests de spread `*gen` con generadores en literales (v1.175).
 *
 * Limitacion documentada en v1.171: spread con generador daba
 * ErrorDeTipo. Ahora OP_LISTA_EXTENDER y OP_CONJUNTO_EXTENDER
 * reanudan el generador via vm_generador_paso hasta agotarlo.
 *
 * Esto cubre tambien tupla (que internamente usa OP_LISTA_EXTENDER
 * + OP_LISTA_A_TUPLA) y conjunto.
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
        "test_spread_generador_out.txt";
#else
        "/tmp/test_spread_generador_out.txt";
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
    /* spread basico de generador en lista */
    {
        char out[256];
        ejecutar_capturando(
            "funcion gen():\n"
            "    producir 10\n"
            "    producir 20\n"
            "    producir 30\n"
            "fin funcion\n"
            "imprimir([*gen()])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[10, 20, 30]") != NULL, "lista_basico");
    }

    /* spread de generador entre otros elementos */
    {
        char out[256];
        ejecutar_capturando(
            "funcion gen():\n"
            "    producir 1\n"
            "    producir 2\n"
            "fin funcion\n"
            "imprimir([99, *gen(), 100])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[99, 1, 2, 100]") != NULL, "entre_elementos");
    }

    /* Generador vacio */
    {
        char out[256];
        ejecutar_capturando(
            "funcion vacio():\n"
            "    si falso:\n"
            "        producir 1\n"
            "    fin si\n"
            "fin funcion\n"
            "imprimir([*vacio()])\n"
            "imprimir([1, *vacio(), 2])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]\n[1, 2]") != NULL, "generador_vacio");
    }

    /* spread de generador en tupla */
    {
        char out[256];
        ejecutar_capturando(
            "funcion gen():\n"
            "    producir 1\n"
            "    producir 2\n"
            "    producir 3\n"
            "fin funcion\n"
            "imprimir((*gen(), 99))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(1, 2, 3, 99)") != NULL, "tupla");
    }

    /* spread de generador en conjunto */
    {
        char out[256];
        ejecutar_capturando(
            "funcion gen():\n"
            "    producir 1\n"
            "    producir 2\n"
            "    producir 1\n"
            "fin funcion\n"
            "imprimir(longitud({*gen()}))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2") != NULL, "conjunto_dedup");
    }

    /* Generador con bucle dentro */
    {
        char out[256];
        ejecutar_capturando(
            "funcion conta(n):\n"
            "    para i en rango(n):\n"
            "        producir i * i\n"
            "    fin para\n"
            "fin funcion\n"
            "imprimir([*conta(5)])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 4, 9, 16]") != NULL, "conta_cuadrados");
    }

    /* Multiples generadores en mismo literal */
    {
        char out[256];
        ejecutar_capturando(
            "funcion par():\n"
            "    producir 0\n"
            "    producir 2\n"
            "fin funcion\n"
            "funcion impar():\n"
            "    producir 1\n"
            "    producir 3\n"
            "fin funcion\n"
            "imprimir([*par(), *impar()])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 2, 1, 3]") != NULL, "dos_generadores");
    }

    /* Generador con destructuring en for + spread */
    {
        char out[256];
        ejecutar_capturando(
            "funcion pares():\n"
            "    producir (1, \"a\")\n"
            "    producir (2, \"b\")\n"
            "fin funcion\n"
            "ps = [*pares()]\n"
            "imprimir(longitud(ps))\n"
            "imprimir(ps[0])\n"
            "imprimir(ps[1])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "2\n(1, \"a\")\n(2, \"b\")") != NULL,
                "tuplas_producidas");
    }

    /* genex */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir([*(x * 10 para x en rango(3))])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 10, 20]") != NULL, "genex");
    }

    if (fallos == 0) {
        printf("spread_generador: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "spread_generador: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
