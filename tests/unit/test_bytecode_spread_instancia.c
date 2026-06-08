/*
 * Tests de spread `*obj` con instancias que tienen `__siguiente__`
 * o `__iterar__` (v1.176). Cierra la ultima limitacion de v1.171.
 *
 * Soporta:
 *   - Instancia con __siguiente__ directo (es su propio iterador).
 *   - Instancia con __iterar__ que devuelve otra instancia con
 *     __siguiente__ (patron Python iter()/next()).
 *
 * Fin del iterador: lanzar ErrorDeIteracion (paridad Python
 * StopIteration).
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
        "test_spread_instancia_out.txt";
#else
        "/tmp/test_spread_instancia_out.txt";
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

#define CLASE_CONTADOR \
    "clase Contador:\n" \
    "    funcion __iniciar__(yo, n):\n" \
    "        yo.n = n\n" \
    "        yo.i = 0\n" \
    "    fin funcion\n" \
    "    funcion __siguiente__(yo):\n" \
    "        si yo.i >= yo.n:\n" \
    "            lanzar ErrorDeIteracion()\n" \
    "        fin si\n" \
    "        v = yo.i\n" \
    "        yo.i = yo.i + 1\n" \
    "        retornar v\n" \
    "    fin funcion\n" \
    "fin clase\n"

int main(void) {
    /* spread basico con __siguiente__ */
    {
        char out[256];
        ejecutar_capturando(
            CLASE_CONTADOR
            "imprimir([*Contador(5)])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 2, 3, 4]") != NULL, "siguiente_basico");
    }

    /* en tupla */
    {
        char out[256];
        ejecutar_capturando(
            CLASE_CONTADOR
            "imprimir((*Contador(3),))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "(0, 1, 2)") != NULL, "tupla");
    }

    /* en conjunto */
    {
        char out[256];
        ejecutar_capturando(
            CLASE_CONTADOR
            "imprimir(longitud({*Contador(4)}))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "4") != NULL, "conjunto");
    }

    /* mezcla con otros elementos */
    {
        char out[256];
        ejecutar_capturando(
            CLASE_CONTADOR
            "imprimir([100, *Contador(3), 200])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[100, 0, 1, 2, 200]") != NULL, "mezcla");
    }

    /* __iterar__ que devuelve self (patron Python iter()/next()) */
    {
        char out[256];
        ejecutar_capturando(
            "clase Rango:\n"
            "    funcion __iniciar__(yo, a, b):\n"
            "        yo.a = a\n"
            "        yo.b = b\n"
            "        yo.i = a\n"
            "    fin funcion\n"
            "    funcion __iterar__(yo):\n"
            "        yo.i = yo.a\n"
            "        retornar yo\n"
            "    fin funcion\n"
            "    funcion __siguiente__(yo):\n"
            "        si yo.i >= yo.b:\n"
            "            lanzar ErrorDeIteracion()\n"
            "        fin si\n"
            "        v = yo.i\n"
            "        yo.i = yo.i + 1\n"
            "        retornar v\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir([*Rango(10, 15)])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[10, 11, 12, 13, 14]") != NULL,
                "iterar_devuelve_self");
    }

    /* Iterador vacio: no error, lista vacia */
    {
        char out[256];
        ejecutar_capturando(
            "clase Vacio:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "    funcion __siguiente__(yo):\n"
            "        lanzar ErrorDeIteracion()\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir([*Vacio()])\n"
            "imprimir([1, *Vacio(), 2])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[]\n[1, 2]") != NULL, "vacio");
    }

    /* Sin __siguiente__ ni __iterar__: ErrorDeTipo atrapable */
    {
        char out[256];
        ejecutar_capturando(
            "clase Mudo:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "fin clase\n"
            "funcion p():\n"
            "    intentar:\n"
            "        xs = [*Mudo()]\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "sin_dunders");
    }

    /* __iterar__ devuelve algo no-iterador */
    {
        char out[256];
        ejecutar_capturando(
            "clase MalIter:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "    funcion __iterar__(yo):\n"
            "        retornar 42\n"
            "    fin funcion\n"
            "fin clase\n"
            "funcion p():\n"
            "    intentar:\n"
            "        xs = [*MalIter()]\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "iterar_invalido");
    }

    /* dos spreads de instancia */
    {
        char out[256];
        ejecutar_capturando(
            CLASE_CONTADOR
            "imprimir([*Contador(2), *Contador(3)])\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[0, 1, 0, 1, 2]") != NULL, "dos_spreads");
    }

    if (fallos == 0) {
        printf("spread_instancia: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "spread_instancia: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
