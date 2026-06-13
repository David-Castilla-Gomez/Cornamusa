/*
 * Regresión: el alias `como e` debe resolver a la excepción aunque el
 * cuerpo del `intentar` declare locals (v1.203).
 *
 * Bug: `compilar_intentar` capturaba `n_locales_handler` = n_locales
 * TRAS compilar el cuerpo del intentar, que incluye los locals
 * declarados dentro. Como el runtime descarta esos locals al hacer
 * unwind (la excepción queda en el slot `n_locales_entrada`, antes del
 * cuerpo), el alias quedaba N slots por encima de la excepción real
 * (off-by-N): `como e` leía basura / corrompía el stack. Sólo se
 * manifestaba dentro de una función y con ≥1 local en el cuerpo del
 * intentar (en top-level los "locals" son globales).
 *
 * Fix: `n_locales_handler = n_locales_entrada`.
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
        "test_intentar_locals_out.txt";
#else
        "/tmp/test_intentar_locals_out.txt";
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

/* Genera una función con `nloc` locals en el cuerpo del intentar y un
 * atrapador con alias; verifica que el alias resuelve a la excepción. */
static void caso_n_locals(int nloc) {
    char fuente[2048];
    char locals[1024] = {0};
    for (int k = 1; k <= nloc; k++) {
        char linea[64];
        snprintf(linea, sizeof(linea), "        v%d = %d\n", k, k);
        strcat(locals, linea);
    }
    snprintf(fuente, sizeof(fuente),
        "funcion c():\n"
        "    intentar:\n"
        "%s"
        "        lanzar ErrorDeTipo(\"MSG\")\n"
        "    atrapar ErrorDeTipo como e:\n"
        "        ax = e\n"
        "        imprimir(cadena(ax))\n"
        "    fin intentar\n"
        "fin funcion\n"
        "c()\n",
        locals);

    char out[256];
    char etiqueta[32];
    snprintf(etiqueta, sizeof(etiqueta), "nlocals_%d", nloc);
    int rc = ejecutar_capturando(fuente, out, sizeof(out));
    AFIRMAR(rc == 0, etiqueta);
    AFIRMAR(strstr(out, "ErrorDeTipo: MSG") != NULL, etiqueta);
}

int main(void) {
    /* Matriz: 0..6 locals en el cuerpo del intentar (antes ≥1 corrompía). */
    for (int nloc = 0; nloc <= 6; nloc++) caso_n_locals(nloc);

    /* Con finalmente + locals. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion a():\n"
            "    intentar:\n"
            "        v1 = 1\n"
            "        lanzar ErrorDeValor(\"FA\")\n"
            "    atrapar ErrorDeValor como e:\n"
            "        imprimir(\"A:\", cadena(e))\n"
            "    finalmente:\n"
            "        imprimir(\"A-fin\")\n"
            "    fin intentar\n"
            "fin funcion\n"
            "a()\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "finalmente_rc");
        AFIRMAR(strstr(out, "A: ErrorDeValor: FA") != NULL, "finalmente_alias");
        AFIRMAR(strstr(out, "A-fin") != NULL, "finalmente_corre");
    }

    /* Multi-atrapador con locals: el segundo coincide, ambos con alias. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion d():\n"
            "    intentar:\n"
            "        v1 = 1\n"
            "        v2 = 2\n"
            "        v3 = 3\n"
            "        lanzar ErrorDeValor(\"DV\")\n"
            "    atrapar ErrorDeTipo como e:\n"
            "        imprimir(\"NO\")\n"
            "    atrapar ErrorDeValor como e:\n"
            "        imprimir(\"D:\", cadena(e))\n"
            "    fin intentar\n"
            "fin funcion\n"
            "d()\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "multi_rc");
        AFIRMAR(strstr(out, "D: ErrorDeValor: DV") != NULL, "multi_alias");
    }

    /* Dos bloques intentar consecutivos en la misma función (sin fuga
     * de estado entre ellos). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion h():\n"
            "    intentar:\n"
            "        v1 = 1\n"
            "        lanzar ErrorDeTipo(\"H1\")\n"
            "    atrapar ErrorDeTipo como e:\n"
            "        imprimir(\"H1:\", cadena(e))\n"
            "    fin intentar\n"
            "    intentar:\n"
            "        w1 = 10\n"
            "        w2 = 20\n"
            "        lanzar ErrorDeValor(\"H2\")\n"
            "    atrapar ErrorDeValor como e:\n"
            "        imprimir(\"H2:\", cadena(e))\n"
            "    fin intentar\n"
            "fin funcion\n"
            "h()\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "consec_rc");
        AFIRMAR(strstr(out, "H1: ErrorDeTipo: H1") != NULL, "consec_primero");
        AFIRMAR(strstr(out, "H2: ErrorDeValor: H2") != NULL, "consec_segundo");
    }

    /* Interacción con atrapar multi-tipo (v1.202) + locals. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion m():\n"
            "    intentar:\n"
            "        v1 = 1\n"
            "        lanzar ErrorDeValor(\"MV\")\n"
            "    atrapar (ErrorDeTipo, ErrorDeValor) como e:\n"
            "        imprimir(\"M:\", cadena(e))\n"
            "    fin intentar\n"
            "fin funcion\n"
            "m()\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "multitipo_rc");
        AFIRMAR(strstr(out, "M: ErrorDeValor: MV") != NULL, "multitipo_alias");
    }

    /* El alias usado varias veces + local en el cuerpo del atrapar. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion g():\n"
            "    intentar:\n"
            "        v1 = 1\n"
            "        lanzar ErrorDeIndice(\"GI\")\n"
            "    atrapar ErrorDeIndice como e:\n"
            "        msg = cadena(e)\n"
            "        imprimir(cadena(e), \"|\", msg)\n"
            "    fin intentar\n"
            "fin funcion\n"
            "g()\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "doble_rc");
        AFIRMAR(strstr(out, "ErrorDeIndice: GI | ErrorDeIndice: GI") != NULL,
                "doble_uso");
    }

    /* Función CON parámetros: la base de locals (n_locales_entrada) es
     * mayor que 1; el alias debe anclarse a esa base, no al slot 1. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion c(p1, p2, p3):\n"
            "    intentar:\n"
            "        v1 = p1 + p2\n"
            "        lanzar ErrorDeValor(\"PAR\")\n"
            "    atrapar ErrorDeValor como e:\n"
            "        imprimir(\"PARAMS:\", cadena(e), p3)\n"
            "    fin intentar\n"
            "fin funcion\n"
            "c(1, 2, 3)\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "params_rc");
        AFIRMAR(strstr(out, "PARAMS: ErrorDeValor: PAR 3") != NULL, "params_alias");
    }

    /* intentar ANIDADO con locals en ambos niveles. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion c():\n"
            "    intentar:\n"
            "        ext = 1\n"
            "        intentar:\n"
            "            int1 = 2\n"
            "            lanzar ErrorDeIndice(\"INNER\")\n"
            "        atrapar ErrorDeIndice como ei:\n"
            "            imprimir(\"INNER:\", cadena(ei))\n"
            "            lanzar ErrorDeValor(\"OUTER\")\n"
            "        fin intentar\n"
            "    atrapar ErrorDeValor como eo:\n"
            "        imprimir(\"OUTER:\", cadena(eo))\n"
            "    fin intentar\n"
            "fin funcion\n"
            "c()\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "anidado_rc");
        AFIRMAR(strstr(out, "INNER: ErrorDeIndice: INNER") != NULL, "anidado_interno");
        AFIRMAR(strstr(out, "OUTER: ErrorDeValor: OUTER") != NULL, "anidado_externo");
    }

    /* SIN alias + locals + re-lanzar (lanzar sin valor) debe re-lanzar
     * la excepción correcta. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion f():\n"
            "    intentar:\n"
            "        v1 = 1\n"
            "        v2 = 2\n"
            "        lanzar ErrorDeValor(\"RE\")\n"
            "    atrapar ErrorDeValor:\n"
            "        lanzar\n"
            "    fin intentar\n"
            "fin funcion\n"
            "intentar:\n"
            "    f()\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"RELANZA:\", cadena(e))\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "sinalias_rc");
        AFIRMAR(strstr(out, "RELANZA: ErrorDeValor: RE") != NULL, "sinalias_relanza");
    }

    if (fallos == 0) {
        printf("intentar_locals: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "intentar_locals: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
