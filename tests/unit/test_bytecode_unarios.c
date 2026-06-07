/*
 * Tests de unarios `~x` (bitwise NOT) en bytecode y dunders
 * `__negar__` / `__tilde__` en instancias (v1.167).
 *
 * Antes de v1.167:
 *   - `~x` con variable -> "operador unario no soportado en bytecode"
 *   - `-instancia` siempre fallaba con ErrorDeTipo (no llamaba dunder)
 *
 * Despues:
 *   - `~x` funciona con cualquier expresion entera/booleana
 *   - Si la clase define `__negar__(yo)` o `__tilde__(yo)`, el operador
 *     unario lo invoca.
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
        "test_unarios_out.txt";
#else
        "/tmp/test_unarios_out.txt";
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
    /* ~x con variable (regresion: antes fallaba en bytecode) */
    {
        char out[256];
        ejecutar_capturando(
            "x = 5\n"
            "imprimir(~x)\n"
            "imprimir(~~x)\n"
            "imprimir(~0)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "-6\n5\n-1") != NULL, "tilde_variable");
    }

    /* ~x sobre booleano */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(~verdadero)\n"
            "imprimir(~falso)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "-2\n-1") != NULL, "tilde_booleano");
    }

    /* ~x sobre expresion compleja */
    {
        char out[256];
        ejecutar_capturando(
            "a = 10\n"
            "b = 3\n"
            "imprimir(~(a - b))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "-8") != NULL, "tilde_expr");
    }

    /* ~x sobre bignum */
    {
        char out[256];
        ejecutar_capturando(
            "n = 10**20\n"
            "imprimir(~n)\n",
            /* ~10^20 = -(10^20 + 1) */
            out, sizeof(out));
        AFIRMAR(strstr(out, "-100000000000000000001") != NULL, "tilde_bignum");
    }

    /* __tilde__ en instancia */
    {
        char out[512];
        ejecutar_capturando(
            "clase Mascara:\n"
            "    funcion __iniciar__(yo, bits):\n"
            "        yo.bits = bits\n"
            "    fin funcion\n"
            "    funcion __tilde__(yo):\n"
            "        retornar Mascara(~yo.bits)\n"
            "    fin funcion\n"
            "    funcion __cadena__(yo):\n"
            "        retornar f\"M({yo.bits})\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "m = Mascara(5)\n"
            "imprimir(~m)\n"
            "imprimir(~~m)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "M(-6)\nM(5)") != NULL, "tilde_dunder");
    }

    /* __negar__ en instancia */
    {
        char out[512];
        ejecutar_capturando(
            "clase Vector:\n"
            "    funcion __iniciar__(yo, dx, dy):\n"
            "        yo.dx = dx\n"
            "        yo.dy = dy\n"
            "    fin funcion\n"
            "    funcion __negar__(yo):\n"
            "        retornar Vector(-yo.dx, -yo.dy)\n"
            "    fin funcion\n"
            "    funcion __cadena__(yo):\n"
            "        retornar f\"V({yo.dx},{yo.dy})\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "v = Vector(3, -4)\n"
            "imprimir(-v)\n"
            "imprimir(-(-v))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "V(-3,4)\nV(3,-4)") != NULL, "negar_dunder");
    }

    /* Sin __negar__ -> ErrorDeTipo atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "clase Caja:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "fin clase\n"
            "funcion p():\n"
            "    intentar:\n"
            "        -Caja()\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "sin_dunder_error");
    }

    /* Sin __tilde__ -> ErrorDeTipo atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "clase Caja:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "fin clase\n"
            "funcion p():\n"
            "    intentar:\n"
            "        ~Caja()\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "sin_tilde_error");
    }

    /* Constant folding sigue funcionando (sin recurrir a OP_TILDE_BIT) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(~7)\n"      /* literal -> folded */
            "imprimir(-15)\n",     /* literal negativo */
            out, sizeof(out));
        AFIRMAR(strstr(out, "-8\n-15") != NULL, "constant_fold");
    }

    if (fallos == 0) {
        printf("unarios: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "unarios: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
