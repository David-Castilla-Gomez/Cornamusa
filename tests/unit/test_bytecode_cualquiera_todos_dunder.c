/*
 * Tests: cualquiera()/todos() despachan __booleano__ (v1.208).
 *
 * Continuación de v1.207 (booleano() despacha __booleano__). Antes
 * cualquiera()/todos() usaban valor_es_verdadero crudo, así que una
 * instancia con __booleano__ siempre contaba como verdadera, dando
 * resultados silenciosamente incorrectos. Ahora usan el helper
 * compartido `evaluar_verdad`, consistente con booleano()/si/no.
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
        "test_any_all_dunder_out.txt";
#else
        "/tmp/test_any_all_dunder_out.txt";
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
    /* todos()/cualquiera() despachan __booleano__. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            CAJA
            "imprimir(\"T10\", todos([Caja(1), Caja(0)]))\n"
            "imprimir(\"T12\", todos([Caja(1), Caja(2)]))\n"
            "imprimir(\"C00\", cualquiera([Caja(0), Caja(0)]))\n"
            "imprimir(\"C03\", cualquiera([Caja(0), Caja(3)]))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "despacho_rc");
        AFIRMAR(strstr(out, "T10 falso") != NULL, "todos_falso");
        AFIRMAR(strstr(out, "T12 verdadero") != NULL, "todos_verdadero");
        AFIRMAR(strstr(out, "C00 falso") != NULL, "cualquiera_falso");
        AFIRMAR(strstr(out, "C03 verdadero") != NULL, "cualquiera_verdadero");
    }

    /* Consistencia con booleano() (un elemento). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            CAJA
            "imprimir(\"K0\", booleano(Caja(0)) == todos([Caja(0)]))\n"
            "imprimir(\"K5\", booleano(Caja(5)) == cualquiera([Caja(5)]))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "consistencia_rc");
        AFIRMAR(strstr(out, "K0 verdadero") != NULL, "consistencia_falso");
        AFIRMAR(strstr(out, "K5 verdadero") != NULL, "consistencia_verdadero");
    }

    /* Regresión: valores no-instancia y vacíos. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "imprimir(\"A\", todos([1, 1, 1]))\n"
            "imprimir(\"B\", todos([1, 0, 1]))\n"
            "imprimir(\"C\", cualquiera([0, 0, 0]))\n"
            "imprimir(\"D\", cualquiera([0, 0, 5]))\n"
            "imprimir(\"E\", todos([]))\n"
            "imprimir(\"F\", cualquiera([]))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "regresion_rc");
        AFIRMAR(strstr(out, "A verdadero") != NULL, "reg_todos_v");
        AFIRMAR(strstr(out, "B falso") != NULL, "reg_todos_f");
        AFIRMAR(strstr(out, "C falso") != NULL, "reg_cualq_f");
        AFIRMAR(strstr(out, "D verdadero") != NULL, "reg_cualq_v");
        AFIRMAR(strstr(out, "E verdadero") != NULL, "reg_todos_vacio");
        AFIRMAR(strstr(out, "F falso") != NULL, "reg_cualq_vacio");
    }

    /* Generador de instancias (regresión v1.200 + despacho). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            CAJA
            "funcion gen():\n"
            "    producir Caja(5)\n"
            "    producir Caja(0)\n"
            "fin funcion\n"
            "imprimir(\"G\", todos(gen()))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "generador_rc");
        AFIRMAR(strstr(out, "G falso") != NULL, "generador_falso");
    }

    /* __booleano__ que lanza propaga el error (atrapable), con
     * corto-circuito limpio. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            CAJA
            "clase Lanza:\n"
            "    funcion __booleano__(yo):\n"
            "        lanzar ErrorDeValor(\"boom\")\n"
            "    fin funcion\n"
            "fin clase\n"
            "intentar:\n"
            "    todos([Caja(1), Lanza()])\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"PROP\", cadena(e))\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "lanza_rc");
        AFIRMAR(strstr(out, "PROP ErrorDeValor: boom") != NULL, "lanza_propaga");
    }

    /* Corto-circuito: cualquiera() para en el primer verdadero (no
     * evalúa el __booleano__ que lanzaría después). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            CAJA
            "clase Lanza:\n"
            "    funcion __booleano__(yo):\n"
            "        lanzar ErrorDeValor(\"no-deberia\")\n"
            "    fin funcion\n"
            "fin clase\n"
            "imprimir(\"CC\", cualquiera([Caja(5), Lanza()]))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "corto_rc");
        AFIRMAR(strstr(out, "CC verdadero") != NULL, "corto_circuito");
    }

    if (fallos == 0) {
        printf("cualquiera_todos_dunder: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "cualquiera_todos_dunder: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
