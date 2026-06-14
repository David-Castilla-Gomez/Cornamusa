/*
 * Tests: el builtin `booleano(obj)` despacha `__booleano__` (v1.207).
 *
 * Antes, `si obj:` / `no obj` despachaban `__booleano__` (v1.41) pero
 * el builtin `booleano(obj)` usaba `valor_es_verdadero`, que para una
 * instancia siempre daba verdadero — inconsistencia. Ahora `booleano()`
 * despacha el dunder (vía el invocador de callables), igual que los
 * contextos de verdad. El resultado de `__booleano__` se coacciona a
 * booleano con las reglas estándar; un error en el dunder se propaga.
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
        "test_bool_dunder_out.txt";
#else
        "/tmp/test_bool_dunder_out.txt";
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

#define CAJA \
    "clase Caja:\n" \
    "    funcion __iniciar__(yo, n):\n" \
    "        yo.n = n\n" \
    "    fin funcion\n" \
    "    funcion __booleano__(yo):\n" \
    "        retornar yo.n > 0\n" \
    "    fin funcion\n" \
    "fin clase\n"

int main(void) {
    /* Despacho de __booleano__ y consistencia con `si`/`no`. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            CAJA
            "imprimir(\"B0\", booleano(Caja(0)))\n"
            "imprimir(\"B5\", booleano(Caja(5)))\n"
            "c0 = Caja(0)\n"
            "si c0:\n"
            "    imprimir(\"SI_MAL\")\n"
            "sino:\n"
            "    imprimir(\"SI_OK\")\n"
            "fin si\n"
            "imprimir(\"NO\", no c0)\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "despacho_rc");
        AFIRMAR(strstr(out, "B0 falso") != NULL, "booleano_falso");
        AFIRMAR(strstr(out, "B5 verdadero") != NULL, "booleano_verdadero");
        AFIRMAR(strstr(out, "SI_OK") != NULL, "consistente_si");
        AFIRMAR(strstr(out, "NO verdadero") != NULL, "consistente_no");
    }

    /* Instancia sin __booleano__ → verdadera (comportamiento anterior). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase Plana:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo.x = 1\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(\"P\", booleano(Plana()))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "plana_rc");
        AFIRMAR(strstr(out, "P verdadero") != NULL, "plana_verdadera");
    }

    /* Regresión: tipos no-instancia siguen igual. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "imprimir(\"Z\", booleano(0))\n"
            "imprimir(\"L0\", booleano([]))\n"
            "imprimir(\"L1\", booleano([1]))\n"
            "imprimir(\"S\", booleano(\"\"))\n"
            "imprimir(\"N\", booleano(nulo))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "regresion_rc");
        AFIRMAR(strstr(out, "Z falso") != NULL, "reg_cero");
        AFIRMAR(strstr(out, "L0 falso") != NULL, "reg_lista_vacia");
        AFIRMAR(strstr(out, "L1 verdadero") != NULL, "reg_lista");
        AFIRMAR(strstr(out, "S falso") != NULL, "reg_cadena_vacia");
        AFIRMAR(strstr(out, "N falso") != NULL, "reg_nulo");
    }

    /* __booleano__ que devuelve un valor no-booleano se coacciona. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase Raro:\n"
            "    funcion __booleano__(yo):\n"
            "        retornar [1, 2]\n"
            "    fin funcion\n"
            "fin clase\n"
            "clase Vacio:\n"
            "    funcion __booleano__(yo):\n"
            "        retornar []\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(\"R\", booleano(Raro()))\n"
            "imprimir(\"V\", booleano(Vacio()))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "coercion_rc");
        AFIRMAR(strstr(out, "R verdadero") != NULL, "coercion_lista_llena");
        AFIRMAR(strstr(out, "V falso") != NULL, "coercion_lista_vacia");
    }

    /* __booleano__ que lanza propaga el error (atrapable). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase Lanza:\n"
            "    funcion __booleano__(yo):\n"
            "        lanzar ErrorDeValor(\"boom\")\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    booleano(Lanza())\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"PROP\", cadena(e))\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "lanza_rc");
        AFIRMAR(strstr(out, "PROP ErrorDeValor: boom") != NULL, "lanza_propaga");
    }

    /* Reentrancia: __booleano__ que a su vez llama booleano() de otra
     * instancia. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            CAJA
            "clase Envoltura:\n"
            "    funcion __iniciar__(yo, caja):\n"
            "        yo.caja = caja\n"
            "    fin funcion\n"
            "    funcion __booleano__(yo):\n"
            "        retornar booleano(yo.caja)\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(\"E0\", booleano(Envoltura(Caja(0))))\n"
            "imprimir(\"E5\", booleano(Envoltura(Caja(5))))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "reentrancia_rc");
        AFIRMAR(strstr(out, "E0 falso") != NULL, "reentrancia_falso");
        AFIRMAR(strstr(out, "E5 verdadero") != NULL, "reentrancia_verdadero");
    }

    /* __booleano__ que devuelve OTRA instancia con __booleano__: debe
     * recursar hasta un no-instancia, igual que `si obj:`. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "clase Falsa:\n"
            "    funcion __booleano__(yo):\n"
            "        retornar falso\n"
            "    fin funcion\n"
            "fin clase\n"
            "clase Envuelve:\n"
            "    funcion __booleano__(yo):\n"
            "        retornar Falsa()\n"
            "    fin funcion\n"
            "fin clase\n"
            "obj = Envuelve()\n"
            "imprimir(\"BL\", booleano(obj))\n"
            "si obj:\n"
            "    imprimir(\"SI verdadero\")\n"
            "sino:\n"
            "    imprimir(\"SI falso\")\n"
            "fin si\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "recursivo_rc");
        AFIRMAR(strstr(out, "BL falso") != NULL, "recursivo_booleano");
        AFIRMAR(strstr(out, "SI falso") != NULL, "recursivo_consistente");
    }

    if (fallos == 0) {
        printf("booleano_dunder: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "booleano_dunder: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
