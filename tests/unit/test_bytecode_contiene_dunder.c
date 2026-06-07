/*
 * Tests del dunder `__contiene__(yo, x)` para `x en obj` con
 * instancias (v1.168).
 *
 * Antes de v1.168, `x en obj` con `obj` instancia siempre dab
 * ErrorDeTipo (membership solo soportado en cadenas, listas, tuplas,
 * dicc, conjuntos, rangos). Ahora si la clase define `__contiene__`,
 * el operador `en` delega.
 *
 * Receptor (yo) es el OBJETO, no el elemento — orden Python:
 * `x in obj` -> `obj.__contains__(x)`.
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
        "test_contiene_dunder_out.txt";
#else
        "/tmp/test_contiene_dunder_out.txt";
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
    /* __contiene__ basico: rango de pares */
    {
        char out[512];
        ejecutar_capturando(
            "clase Pares:\n"
            "    funcion __iniciar__(yo, limite):\n"
            "        yo.limite = limite\n"
            "    fin funcion\n"
            "    funcion __contiene__(yo, n):\n"
            "        retornar n % 2 == 0 y n >= 0 y n <= yo.limite\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = Pares(10)\n"
            "imprimir(4 en p)\n"
            "imprimir(5 en p)\n"
            "imprimir(0 en p)\n"
            "imprimir(12 en p)\n"
            "imprimir(-2 en p)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso\nverdadero\nfalso\nfalso") != NULL,
                "pares_membership");
    }

    /* Composicion con `no` */
    {
        char out[256];
        ejecutar_capturando(
            "clase S:\n"
            "    funcion __iniciar__(yo):\n"
            "        pasar\n"
            "    fin funcion\n"
            "    funcion __contiene__(yo, x):\n"
            "        retornar x == 42\n"
            "    fin funcion\n"
            "fin clase\n"
            "s = S()\n"
            "imprimir(no (42 en s))\n"
            "imprimir(no (99 en s))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "falso\nverdadero") != NULL, "negacion");
    }

    /* Sin __contiene__ -> ErrorDeTipo atrapable (sin caer en
     * dispatch normal de OP_EN que solo soporta colecciones) */
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
            "        1 en Vacia()\n"
            "    atrapar ErrorDeTipo:\n"
            "        imprimir(\"ok\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "p()\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "sin_dunder_error");
    }

    /* En condicional */
    {
        char out[256];
        ejecutar_capturando(
            "clase Modulo:\n"
            "    funcion __iniciar__(yo, m):\n"
            "        yo.m = m\n"
            "    fin funcion\n"
            "    funcion __contiene__(yo, n):\n"
            "        retornar n % yo.m == 0\n"
            "    fin funcion\n"
            "fin clase\n"
            "tres = Modulo(3)\n"
            "para i en rango(10):\n"
            "    si i en tres:\n"
            "        imprimir(i)\n"
            "    fin si\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "0\n3\n6\n9") != NULL, "en_condicional");
    }

    /* Composicion con y/o */
    {
        char out[256];
        ejecutar_capturando(
            "clase Conj:\n"
            "    funcion __iniciar__(yo, xs):\n"
            "        yo.xs = xs\n"
            "    fin funcion\n"
            "    funcion __contiene__(yo, x):\n"
            "        retornar x en yo.xs\n"
            "    fin funcion\n"
            "fin clase\n"
            "a = Conj([1, 2, 3])\n"
            "b = Conj([3, 4, 5])\n"
            "imprimir(3 en a y 3 en b)\n"
            "imprimir(1 en a o 1 en b)\n"
            "imprimir(99 en a o 99 en b)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nverdadero\nfalso") != NULL,
                "composicion_logica");
    }

    /* Receptor correcto (instancia, no escalar): asegurar que el
     * dispatch usa el DERECHO, no el izquierdo. Si hubiera bug donde
     * usaramos izq, entero NUNCA tendria __contiene__ y caeria a
     * dispatch normal que falla con instancia derecha. */
    {
        char out[256];
        ejecutar_capturando(
            "clase Marca:\n"
            "    funcion __iniciar__(yo, etiqueta):\n"
            "        yo.etiqueta = etiqueta\n"
            "    fin funcion\n"
            "    funcion __contiene__(yo, otro):\n"
            "        retornar otro == yo.etiqueta\n"
            "    fin funcion\n"
            "fin clase\n"
            "m = Marca(\"hola\")\n"
            "imprimir(\"hola\" en m)\n"
            "imprimir(\"adios\" en m)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nfalso") != NULL, "receptor_correcto");
    }

    /* Membership con tipos colectivos no se rompe (paridad) */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(3 en [1, 2, 3])\n"
            "imprimir(\"c\" en \"caja\")\n"
            "imprimir(2 en {1, 2, 3})\n"
            "imprimir(\"k\" en {\"k\": 1})\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero\nverdadero\nverdadero\nverdadero") != NULL,
                "tipos_colectivos");
    }

    if (fallos == 0) {
        printf("contiene_dunder: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "contiene_dunder: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
