/*
 * Tests de operadores bitwise binarios (`&`, `|`, `^`, `<<`, `>>`)
 * en bytecode (v1.170) + dunders correspondientes en instancias.
 *
 * Antes de v1.170, estos operadores funcionaban con literales por
 * constant folding pero fallaban con variables ("operador binario
 * no soportado en bytecode v0.6 sesion 2"). Mismo bug latente que
 * `~x` resuelto en v1.167 pero para binarios.
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
        "test_bitwise_out.txt";
#else
        "/tmp/test_bitwise_out.txt";
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
    /* Bitwise con variables (regresion: antes fallaba) */
    {
        char out[256];
        ejecutar_capturando(
            "a = 12\n"
            "b = 10\n"
            "imprimir(a & b)\n"
            "imprimir(a | b)\n"
            "imprimir(a ^ b)\n"
            "imprimir(a << 2)\n"
            "imprimir(a >> 1)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "8\n14\n6\n48\n6") != NULL, "bitwise_variables");
    }

    /* Constant folding sigue funcionando con literales */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(0b1100 & 0b1010)\n"
            "imprimir(0b1100 | 0b1010)\n"
            "imprimir(0b1100 ^ 0b1010)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "8\n14\n6") != NULL, "bitwise_literales");
    }

    /* Con bignum */
    {
        char out[256];
        ejecutar_capturando(
            "n = 10**40\n"
            "imprimir((n & 0xFF) >= 0)\n"
            "imprimir((n | 1) > n)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nverdadero") != NULL, "bitwise_bignum");
    }

    /* __bit_y__ en instancia */
    {
        char out[256];
        ejecutar_capturando(
            "clase Bits:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "    funcion __bit_y__(yo, otro):\n"
            "        retornar Bits(yo.n & otro.n)\n"
            "    fin funcion\n"
            "    funcion __cadena__(yo):\n"
            "        retornar f\"B({yo.n})\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(Bits(12) & Bits(10))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "B(8)") != NULL, "dunder_bit_y");
    }

    /* __bit_o__ en instancia */
    {
        char out[256];
        ejecutar_capturando(
            "clase Bits:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "    funcion __bit_o__(yo, otro):\n"
            "        retornar Bits(yo.n | otro.n)\n"
            "    fin funcion\n"
            "    funcion __cadena__(yo):\n"
            "        retornar f\"B({yo.n})\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(Bits(12) | Bits(10))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "B(14)") != NULL, "dunder_bit_o");
    }

    /* __bit_xor__ */
    {
        char out[256];
        ejecutar_capturando(
            "clase Bits:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "    funcion __bit_xor__(yo, otro):\n"
            "        retornar Bits(yo.n ^ otro.n)\n"
            "    fin funcion\n"
            "    funcion __cadena__(yo):\n"
            "        retornar f\"B({yo.n})\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(Bits(12) ^ Bits(10))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "B(6)") != NULL, "dunder_bit_xor");
    }

    /* __despl_izq__ / __despl_der__ */
    {
        char out[256];
        ejecutar_capturando(
            "clase Bits:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "    funcion __despl_izq__(yo, otro):\n"
            "        retornar Bits(yo.n << otro)\n"
            "    fin funcion\n"
            "    funcion __despl_der__(yo, otro):\n"
            "        retornar Bits(yo.n >> otro)\n"
            "    fin funcion\n"
            "    funcion __cadena__(yo):\n"
            "        retornar f\"B({yo.n})\"\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(Bits(3) << 4)\n"
            "imprimir(Bits(64) >> 2)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "B(48)\nB(16)") != NULL, "dunder_despl");
    }

    /* Sin dunder -> ErrorDeTipo atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "clase Vacia:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "fin clase\n"
            "funcion p():\n"
            "    intentar:\n"
            "        Vacia() & 5\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "sin_dunder_error");
    }

    /* Precedencia: & tiene precedencia mas baja que comparaciones?
     * En Python &|^ tienen precedencia entre comparaciones y +/-.
     * Validamos que la parser usa la precedencia correcta. */
    {
        char out[256];
        ejecutar_capturando(
            "a = 0b1100\n"
            "b = 0b1010\n"
            "imprimir((a & b) == 8)\n"
            "imprimir((a | b) | 1)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\n15") != NULL, "precedencia");
    }

    if (fallos == 0) {
        printf("bitwise: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "bitwise: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
