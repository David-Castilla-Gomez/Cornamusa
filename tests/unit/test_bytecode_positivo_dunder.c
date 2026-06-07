/*
 * Tests del dunder `__positivo__(yo)` para `+instancia` (v1.169).
 *
 * Cierra la trilogia unaria iniciada en v1.167 (__negar__, __tilde__)
 * + v1.168 (__contiene__). El bytecode ahora emite OP_POSITIVO en
 * lugar de no-op para que la VM despache si la clase lo define.
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
        "test_positivo_dunder_out.txt";
#else
        "/tmp/test_positivo_dunder_out.txt";
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
    /* __positivo__ basico: devuelve absoluto */
    {
        char out[512];
        ejecutar_capturando(
            "clase Cifra:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "    funcion __positivo__(yo):\n"
            "        retornar Cifra(absoluto(yo.n))\n"
            "    fin funcion\n"
            "    funcion __cadena__(yo):\n"
            "        retornar f\"C({yo.n})\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "a = Cifra(-5)\n"
            "imprimir(+a)\n"
            "b = Cifra(7)\n"
            "imprimir(+b)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "C(5)\nC(7)") != NULL, "positivo_absoluto");
    }

    /* `+x` con entero sigue siendo identidad (no-op semantico) */
    {
        char out[256];
        ejecutar_capturando(
            "x = 42\n"
            "imprimir(+x)\n"
            "imprimir(+(-7))\n"
            "imprimir(+0)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "42\n-7\n0") != NULL, "identidad_numerica");
    }

    /* `+x` con decimal */
    {
        char out[256];
        ejecutar_capturando(
            "x = 3.14\n"
            "imprimir(+x)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "3.14") != NULL, "identidad_decimal");
    }

    /* Sin __positivo__ -> tambien es no-op para la instancia */
    {
        char out[256];
        ejecutar_capturando(
            "clase Marca:\n"
            "    funcion __iniciar__(yo, etiqueta):\n"
            "        yo.etiqueta = etiqueta\n"
            "    fin funcion\n"
            "    funcion __cadena__(yo):\n"
            "        retornar yo.etiqueta\n"
            "    fin funcion\n"
            "fin clase\n"
            "m = Marca(\"hola\")\n"
            "imprimir(+m)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "hola") != NULL, "sin_dunder_no_op");
    }

    /* Composicion: -(+x) */
    {
        char out[256];
        ejecutar_capturando(
            "clase Num:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "    funcion __positivo__(yo):\n"
            "        retornar Num(absoluto(yo.n))\n"
            "    fin funcion\n"
            "    funcion __negar__(yo):\n"
            "        retornar Num(-yo.n)\n"
            "    fin funcion\n"
            "    funcion __cadena__(yo):\n"
            "        retornar f\"N({yo.n})\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "v = Num(-3)\n"
            "imprimir(+v)\n"
            "imprimir(-(+v))\n"
            "imprimir(+(-v))\n",
            out, sizeof(out));
        /* -3 -> +-3=N(3); -N(3)=N(-3); -(-3)=N(3); +N(3)=N(3) */
        AFIRMAR(strstr(out, "N(3)\nN(-3)\nN(3)") != NULL,
                "composicion_unarios");
    }

    /* Folding sigue funcionando: +5 literal no emite OP_POSITIVO */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(+5)\n"
            "imprimir(+-7)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "5\n-7") != NULL, "constant_fold");
    }

    if (fallos == 0) {
        printf("positivo_dunder: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "positivo_dunder: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
