/*
 * Tests de @propiedad con setter (v1.109).
 *
 * Sintaxis Python-style:
 *
 *   clase X:
 *       @propiedad
 *       funcion x(yo):
 *           retornar yo._x
 *       fin funcion
 *
 *       @escritor
 *       funcion x(yo, valor):
 *           yo._x = valor
 *       fin funcion
 *   fin clase
 *
 * `@escritor` aplicado a un metodo con el MISMO NOMBRE que una
 * @propiedad ya definida fusiona el setter en la propiedad
 * existente. Si no existe propiedad previa, error.
 *
 * Asignar a una propiedad sin setter lanza ErrorDeAtributo
 * atrapable.
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
        "test_prop_out.txt";
#else
        "/tmp/test_prop_out.txt";
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
    /* Setter funciona: c.x = 10 invoca el setter */
    {
        char out[2048];
        ejecutar_capturando(
            "clase Caja:\n"
            "    funcion __iniciar__(yo, l):\n"
            "        yo._lado = l\n"
            "    fin funcion\n"
            "    @propiedad\n"
            "    funcion lado(yo):\n"
            "        retornar yo._lado\n"
            "    fin funcion\n"
            "    @escritor\n"
            "    funcion lado(yo, v):\n"
            "        yo._lado = v * 2\n"   /* doblamos para verificar que se llamo */
            "    fin funcion\n"
            "fin clase\n"
            "c = Caja(5)\n"
            "imprimir(c.lado)\n"   /* 5 */
            "c.lado = 10\n"
            "imprimir(c.lado)\n",  /* 20 (10 * 2) */
            out, sizeof(out));
        AFIRMAR(strstr(out, "5\n20") != NULL || strstr(out, "5\r\n20") != NULL,
                "setter_invocado");
    }

    /* Validacion en setter: rechazar valores invalidos */
    {
        char out[2048];
        ejecutar_capturando(
            "clase Edad:\n"
            "    funcion __iniciar__(yo, n):\n"
            "        yo._n = n\n"
            "    fin funcion\n"
            "    @propiedad\n"
            "    funcion n(yo):\n"
            "        retornar yo._n\n"
            "    fin funcion\n"
            "    @escritor\n"
            "    funcion n(yo, v):\n"
            "        si v < 0:\n"
            "            lanzar ErrorDeValor(\"edad negativa\")\n"
            "        fin si\n"
            "        yo._n = v\n"
            "    fin funcion\n"
            "fin clase\n"
            "e = Edad(20)\n"
            "intentar:\n"
            "    e.n = -5\n"
            "atrapar ErrorDeValor como err:\n"
            "    imprimir(\"capturado\")\n"
            "fin intentar\n"
            "imprimir(e.n)\n",   /* sigue 20 porque el setter lanzo */
            out, sizeof(out));
        AFIRMAR(strstr(out, "capturado") != NULL, "validacion_lanza");
        AFIRMAR(strstr(out, "20") != NULL, "validacion_no_modifica");
    }

    /* Propiedad sin setter: asignar lanza ErrorDeAtributo */
    {
        char out[2048];
        ejecutar_capturando(
            "clase Circulo:\n"
            "    funcion __iniciar__(yo, r):\n"
            "        yo._r = r\n"
            "    fin funcion\n"
            "    @propiedad\n"
            "    funcion radio(yo):\n"
            "        retornar yo._r\n"
            "    fin funcion\n"
            "fin clase\n"
            "c = Circulo(3)\n"
            "intentar:\n"
            "    c.radio = 5\n"
            "atrapar ErrorDeAtributo como e:\n"
            "    imprimir(\"solo lectura\")\n"
            "fin intentar\n"
            "imprimir(c.radio)\n",   /* sigue 3 */
            out, sizeof(out));
        AFIRMAR(strstr(out, "solo lectura") != NULL, "sin_setter_lanza");
        AFIRMAR(strstr(out, "3") != NULL, "sin_setter_no_modifica");
    }

    /* @escritor sin @propiedad previa: error */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "clase X:\n"
            "    @escritor\n"
            "    funcion y(yo, v):\n"
            "        yo._y = v\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(\"nunca\")\n",
            out, sizeof(out));
        AFIRMAR(rc != 0, "escritor_solo_falla");
        AFIRMAR(strstr(out, "nunca") == NULL, "escritor_solo_no_imprime");
    }

    /* Atributos normales siguen funcionando (no se rompe el caso sin propiedad) */
    {
        char out[1024];
        ejecutar_capturando(
            "clase Punto:\n"
            "    funcion __iniciar__(yo, x, b):\n"
            "        yo.x = x\n"
            "        yo.b = b\n"   /* atributo normal */
            "    fin funcion\n"
            "fin clase\n"
            "p = Punto(1, 2)\n"
            "p.x = 10\n"
            "p.b = 20\n"
            "imprimir(p.x, p.b)\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10 20") != NULL, "atributos_normales_funcionan");
    }

    /* Setter puede leer otro atributo de yo (closure de yo) */
    {
        char out[1024];
        ejecutar_capturando(
            "clase Termo:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo._celsius = 0\n"
            "    fin funcion\n"
            "    @propiedad\n"
            "    funcion celsius(yo):\n"
            "        retornar yo._celsius\n"
            "    fin funcion\n"
            "    @escritor\n"
            "    funcion celsius(yo, c):\n"
            "        si c < -273:\n"
            "            lanzar ErrorDeValor(\"bajo cero absoluto\")\n"
            "        fin si\n"
            "        yo._celsius = c\n"
            "    fin funcion\n"
            "    @propiedad\n"
            "    funcion fahrenheit(yo):\n"
            "        retornar yo._celsius * 9 / 5 + 32\n"
            "    fin funcion\n"
            "fin clase\n"
            "t = Termo()\n"
            "t.celsius = 100\n"
            "imprimir(t.celsius)\n"     /* 100 */
            "imprimir(t.fahrenheit)\n", /* 212 */
            out, sizeof(out));
        AFIRMAR(strstr(out, "100") != NULL, "termo_celsius");
        AFIRMAR(strstr(out, "212") != NULL, "termo_fahrenheit");
    }

    if (fallos == 0) {
        printf("propiedad_setter: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "propiedad_setter: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
