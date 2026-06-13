/*
 * Tests de `atrapar (T1, T2, ...)` — múltiples tipos de excepción en
 * un solo manejador (v1.202).
 *
 * Antes solo se admitía `atrapar Tipo` con un identificador simple;
 * `atrapar (A, B)` daba ErrorDeCompilacion. Ahora el tipo puede ser
 * una tupla de identificadores con semántica OR (atrapa si la clase
 * de la excepción coincide con CUALQUIERA).
 *
 * Implementación compiler-only: cadena de COMPROBAR_TIPO_EXC con
 * SALTAR_SI_FALSO/SALTAR (sin opcode nuevo). El caso de un solo tipo
 * debe seguir emitiendo el mismo bytecode — de ahí los tests de
 * regresión del camino de un tipo.
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
        "test_atrapar_multi_out.txt";
#else
        "/tmp/test_atrapar_multi_out.txt";
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
    /* Match del primero, del medio y del último de la tupla. */
    {
        char out[512];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    lanzar ErrorDeTipo(\"t\")\n"
            "atrapar (ErrorDeTipo, ErrorDeValor, ErrorDeIndice) como e:\n"
            "    imprimir(\"P\", cadena(e))\n"
            "fin intentar\n"
            "intentar:\n"
            "    lanzar ErrorDeValor(\"v\")\n"
            "atrapar (ErrorDeTipo, ErrorDeValor, ErrorDeIndice) como e:\n"
            "    imprimir(\"M\", cadena(e))\n"
            "fin intentar\n"
            "intentar:\n"
            "    lanzar ErrorDeIndice(\"i\")\n"
            "atrapar (ErrorDeTipo, ErrorDeValor, ErrorDeIndice) como e:\n"
            "    imprimir(\"U\", cadena(e))\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "match_rc");
        AFIRMAR(strstr(out, "P ErrorDeTipo: t") != NULL, "match_primero");
        AFIRMAR(strstr(out, "M ErrorDeValor: v") != NULL, "match_medio");
        AFIRMAR(strstr(out, "U ErrorDeIndice: i") != NULL, "match_ultimo");
    }

    /* Ningún match en el multi-tipo → cae al siguiente atrapador. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    lanzar ErrorDeClave(\"k\")\n"
            "atrapar (ErrorDeTipo, ErrorDeValor):\n"
            "    imprimir(\"ZZNOENTRA\")\n"
            "atrapar ErrorDeClave:\n"
            "    imprimir(\"SIGUIENTE\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "fallback_rc");
        AFIRMAR(strstr(out, "SIGUIENTE") != NULL, "fallback_handler");
        AFIRMAR(strstr(out, "ZZNOENTRA") == NULL, "fallback_no_entra_multi");
    }

    /* Sin alias + re-lanzar (lanzar sin valor) dentro de multi-tipo. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion f():\n"
            "    intentar:\n"
            "        lanzar ErrorDeValor(\"re\")\n"
            "    atrapar (ErrorDeTipo, ErrorDeValor):\n"
            "        lanzar\n"
            "    fin intentar\n"
            "fin funcion\n"
            "intentar:\n"
            "    f()\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"RERAISE\", cadena(e))\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "reraise_rc");
        AFIRMAR(strstr(out, "RERAISE ErrorDeValor: re") != NULL, "reraise_ok");
    }

    /* finalmente + multi-tipo: el finalmente corre tras el atrapar. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    lanzar ErrorDeIndice(\"ix\")\n"
            "atrapar (ErrorDeValor, ErrorDeIndice):\n"
            "    imprimir(\"CUERPO\")\n"
            "finalmente:\n"
            "    imprimir(\"FINALMENTE\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "finalmente_rc");
        AFIRMAR(strstr(out, "CUERPO\nFINALMENTE") != NULL, "finalmente_orden");
    }

    /* Regresión: el caso de un solo tipo sigue funcionando idéntico. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    lanzar ErrorDeValor(\"solo\")\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"PRIMERO\", cadena(e))\n"
            "fin intentar\n"
            "intentar:\n"
            "    lanzar ErrorDeTipo(\"x\")\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"ZZNOENTRA\")\n"
            "atrapar ErrorDeTipo:\n"
            "    imprimir(\"SEGUNDO\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "unico_rc");
        AFIRMAR(strstr(out, "PRIMERO ErrorDeValor: solo") != NULL, "unico_match");
        AFIRMAR(strstr(out, "SEGUNDO") != NULL, "unico_fallback");
        AFIRMAR(strstr(out, "ZZNOENTRA") == NULL, "unico_no_falso_match");
    }

    /* El estado del programa sigue sano tras varios multi-atrapar
     * (equilibrio del stack: ni descartar de más ni de menos el bool). */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "total = 0\n"
            "para i en rango(5):\n"
            "    intentar:\n"
            "        lanzar ErrorDeValor(\"x\")\n"
            "    atrapar (ErrorDeTipo, ErrorDeValor, ErrorDeIndice):\n"
            "        total = total + 1\n"
            "    fin intentar\n"
            "fin para\n"
            "imprimir(\"TOTAL\", total)\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "stack_rc");
        AFIRMAR(strstr(out, "TOTAL 5") != NULL, "stack_equilibrado");
    }

    /* Un solo tipo entre paréntesis SIN coma: `atrapar (X)` == `atrapar X`
     * (EXPR_GRUPO transparente, no una tupla). Con y sin alias. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    lanzar ErrorDeValor(\"g\")\n"
            "atrapar (ErrorDeValor) como e:\n"
            "    imprimir(\"GRUPO\", cadena(e))\n"
            "fin intentar\n"
            "intentar:\n"
            "    lanzar ErrorDeTipo(\"g2\")\n"
            "atrapar (ErrorDeTipo):\n"
            "    imprimir(\"GRUPO2\")\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "grupo_rc");
        AFIRMAR(strstr(out, "GRUPO ErrorDeValor: g") != NULL, "grupo_alias");
        AFIRMAR(strstr(out, "GRUPO2") != NULL, "grupo_sin_alias");
    }

    /* Tupla con coma final `(X,)` también es un solo tipo válido. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    lanzar ErrorDeIndice(\"c\")\n"
            "atrapar (ErrorDeIndice,) como e:\n"
            "    imprimir(\"COMA\", cadena(e))\n"
            "fin intentar\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "coma_rc");
        AFIRMAR(strstr(out, "COMA ErrorDeIndice: c") != NULL, "coma_final");
    }

    if (fallos == 0) {
        printf("atrapar_multi: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "atrapar_multi: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
