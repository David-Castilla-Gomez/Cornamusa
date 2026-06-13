/*
 * Tests: los builtins (lista, conjunto, suma, mapear, ordenado,
 * inverso, ...) aceptan instancias iterables (v1.205).
 *
 * Antes solo `para x en obj` iteraba instancias con __iterar__/
 * __siguiente__; los builtins basados en el iterador genérico daban
 * ErrorDeTipo. Ahora iter_nuevo materializa la instancia vía el hook
 * de la VM (vm_materializar_instancia_iterable), que reusa el modelo
 * de iteración de `para`/spread:
 *   - __siguiente__ directo (la instancia es su propio iterador),
 *   - __iterar__ -> instancia con __siguiente__,
 *   - __iterar__ -> iterable nativo / generador.
 * Hereda el blindaje de GC de v1.200 (no UAF aunque __siguiente__
 * dispare el GC) y propaga errores de __siguiente__ (salvo
 * ErrorDeIteracion, que es el fin normal).
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
        "test_inst_iter_out.txt";
#else
        "/tmp/test_inst_iter_out.txt";
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

/* Clase iteradora lazy con __siguiente__ directo (0..n-1). */
#define CONTADOR \
    "clase Contador:\n" \
    "    funcion __iniciar__(yo, n):\n" \
    "        yo.i = 0\n" \
    "        yo.n = n\n" \
    "    fin funcion\n" \
    "    funcion __siguiente__(yo):\n" \
    "        si yo.i >= yo.n:\n" \
    "            lanzar ErrorDeIteracion()\n" \
    "        fin si\n" \
    "        x = yo.i\n" \
    "        yo.i = yo.i + 1\n" \
    "        retornar x\n" \
    "    fin funcion\n" \
    "fin clase\n"

int main(void) {
    /* __siguiente__ directo a través de varios builtins. */
    {
        char out[512];
        int rc = ejecutar_capturando(
            CONTADOR
            "imprimir(\"L\", lista(Contador(3)))\n"
            "imprimir(\"S\", suma(Contador(4)))\n"
            "imprimir(\"MX\", maximo(Contador(5)))\n"
            "imprimir(\"O\", ordenado(Contador(3)))\n"
            "imprimir(\"I\", inverso(Contador(4)))\n"
            "imprimir(\"C\", ordenado(conjunto(Contador(3))))\n"
            "imprimir(\"E\", enumerar(Contador(3)))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "sig_rc");
        AFIRMAR(strstr(out, "L [0, 1, 2]") != NULL, "sig_lista");
        AFIRMAR(strstr(out, "S 6") != NULL, "sig_suma");
        AFIRMAR(strstr(out, "MX 4") != NULL, "sig_maximo");
        AFIRMAR(strstr(out, "O [0, 1, 2]") != NULL, "sig_ordenado");
        AFIRMAR(strstr(out, "I [3, 2, 1, 0]") != NULL, "sig_inverso");
        AFIRMAR(strstr(out, "C [0, 1, 2]") != NULL, "sig_conjunto");
        AFIRMAR(strstr(out, "E [(0, 0), (1, 1), (2, 2)]") != NULL, "sig_enumerar");
    }

    /* __iterar__ que devuelve un iterable nativo (rango). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase Rango3:\n"
            "    funcion __iterar__(yo):\n"
            "        retornar rango(3)\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(\"L\", lista(Rango3()))\n"
            "imprimir(\"T\", tupla(Rango3()))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "iter_nat_rc");
        AFIRMAR(strstr(out, "L [0, 1, 2]") != NULL, "iter_nat_lista");
        AFIRMAR(strstr(out, "T (0, 1, 2)") != NULL, "iter_nat_tupla");
    }

    /* __iterar__ que devuelve una instancia con __siguiente__. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            CONTADOR
            "clase Coleccion:\n"
            "    funcion __iterar__(yo):\n"
            "        retornar Contador(3)\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(\"L\", lista(Coleccion()))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "iter_inst_rc");
        AFIRMAR(strstr(out, "L [0, 1, 2]") != NULL, "iter_inst_lista");
    }

    /* Propagación de error: __siguiente__ lanza un error real a mitad. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase Rota:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo.i = 0\n"
            "    fin funcion\n"
            "    funcion __siguiente__(yo):\n"
            "        yo.i = yo.i + 1\n"
            "        si yo.i == 3:\n"
            "            lanzar ErrorDeValor(\"rota\")\n"
            "        fin si\n"
            "        retornar yo.i\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    lista(Rota())\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"PROP\", cadena(e))\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "prop_rc");
        AFIRMAR(strstr(out, "PROP ErrorDeValor: rota") != NULL, "prop_propaga");
    }

    /* Instancia SIN __iterar__ ni __siguiente__ → ErrorDeTipo honesto. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase Plana:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo.x = 1\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    conjunto(Plana())\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"NOITER\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "noiter_rc");
        AFIRMAR(strstr(out, "NOITER") != NULL, "noiter_atrapado");
    }

    /* GC: __siguiente__ que llama recolectar() a mitad — sin UAF. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase ConGC:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo.i = 0\n"
            "        yo.n = n\n"
            "    fin funcion\n"
            "    funcion __siguiente__(yo):\n"
            "        recolectar()\n"
            "        si yo.i >= yo.n:\n"
            "            lanzar ErrorDeIteracion()\n"
            "        fin si\n"
            "        ax = [yo.i, yo.i * 10]\n"
            "        yo.i = yo.i + 1\n"
            "        retornar ax\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(\"GC\", lista(ConGC(3)))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "gc_rc");
        AFIRMAR(strstr(out, "GC [[0, 0], [1, 10], [2, 20]]") != NULL, "gc_correcto");
    }

    /* Iterable vacío (instancia que se agota de inmediato). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            CONTADOR
            "imprimir(\"V\", lista(Contador(0)))\n"
            "imprimir(\"VS\", suma(Contador(0)))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "vacio_rc");
        AFIRMAR(strstr(out, "V []") != NULL, "vacio_lista");
        AFIRMAR(strstr(out, "VS 0") != NULL, "vacio_suma");
    }

    if (fallos == 0) {
        printf("builtins_instancia_iter: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "builtins_instancia_iter: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
