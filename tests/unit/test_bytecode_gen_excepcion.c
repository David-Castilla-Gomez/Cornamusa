/*
 * Tests de excepciones lanzadas DENTRO de generadores y atrapadas
 * por handlers del caller (v1.199).
 *
 * Bug arreglado (el flaky historico test_bytecode_genex_multi_destr,
 * SegFault ~20-25%): vm_generador_paso no fijaba handler_techo, asi
 * que una excepcion no atrapada dentro del generador hacia unwind
 * hacia un handler del CALLER mientras seguiamos dentro del
 * sub-dispatch del generador. El resto del programa se ejecutaba en
 * el sub-loop con frame_techo/modo_yield incoherentes → "Pila vacia
 * (bug del compilador)" o SegFault segun layout del heap.
 *
 * Fix: handler_techo durante el sub-dispatch (mismo patron que
 * vm_ejecutar_dunder_sync de v1.42) + limpieza de frames/slots en el
 * camino de error. La excepcion se relanza en el contexto del caller
 * via RAISE_OR_DIE de OP_ITER_SIGUIENTE, donde el unwind es correcto.
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
        "test_gen_excepcion_out.txt";
#else
        "/tmp/test_gen_excepcion_out.txt";
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
    /* El repro original: genex con destructuring de aridad incorrecta.
     * El programa debe TERMINAR LIMPIO (rc==0) tras atrapar — antes
     * dejaba el stack corrupto y rc!=0 o SegFault. */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    g = (a + b para a, b en [(1, 2), (3,)])\n"
            "    para v en g:\n"
            "        imprimir(v)\n"
            "    fin para\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"err\")\n"
            "fin intentar\n"
            "imprimir(\"post\")\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "rc_limpio");
        AFIRMAR(strstr(out, "3") != NULL, "primer_elemento");
        AFIRMAR(strstr(out, "err") != NULL, "atrapado");
        AFIRMAR(strstr(out, "post") != NULL, "continua_despues");
    }

    /* lanzar explicito dentro de funcion generadora */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion gen():\n"
            "    producir 1\n"
            "    lanzar ErrorDeValor(\"boom\")\n"
            "fin funcion\n"
            "intentar:\n"
            "    para v en gen():\n"
            "        imprimir(v)\n"
            "    fin para\n"
            "atrapar ErrorDeValor como e:\n"
            "    imprimir(\"atrapado:\", cadena(e))\n"
            "fin intentar\n"
            "imprimir(\"fin\")\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "lanzar_rc");
        AFIRMAR(strstr(out, "1") != NULL, "lanzar_primero");
        AFIRMAR(strstr(out, "atrapado: ErrorDeValor: boom") != NULL,
                "lanzar_atrapado");
        AFIRMAR(strstr(out, "fin") != NULL, "lanzar_post");
    }

    /* Excepcion atrapada DENTRO del generador: no debe escapar */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "funcion gen():\n"
            "    intentar:\n"
            "        lanzar ErrorDeValor(\"interna\")\n"
            "    atrapar ErrorDeValor:\n"
            "        producir 99\n"
            "    fin intentar\n"
            "    producir 100\n"
            "fin funcion\n"
            "para v en gen():\n"
            "    imprimir(v)\n"
            "fin para\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "interna_rc");
        AFIRMAR(strstr(out, "99\n100") != NULL, "interna_valores");
    }

    /* Error aritmetico dentro del genex */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "intentar:\n"
            "    g = (10 / d para d en [2, 0, 5])\n"
            "    para v en g:\n"
            "        imprimir(v)\n"
            "    fin para\n"
            "atrapar ErrorAritmetico:\n"
            "    imprimir(\"div0\")\n"
            "fin intentar\n"
            "imprimir(\"sigue\")\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "aritmetico_rc");
        AFIRMAR(strstr(out, "div0") != NULL, "aritmetico_atrapado");
        AFIRMAR(strstr(out, "sigue") != NULL, "aritmetico_post");
    }

    /* Generador agotado por error NO se puede reanudar */
    {
        char out[512];
        int rc = ejecutar_capturando(
            "funcion gen():\n"
            "    producir 1\n"
            "    lanzar ErrorDeValor(\"x\")\n"
            "fin funcion\n"
            "g = gen()\n"
            "intentar:\n"
            "    para v en g:\n"
            "        imprimir(v)\n"
            "    fin para\n"
            "atrapar ErrorDeValor:\n"
            "    imprimir(\"e1\")\n"
            "fin intentar\n"
            "vistos = [v para v en g]\n"
            "imprimir(longitud(vistos))\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "agotado_rc");
        AFIRMAR(strstr(out, "e1") != NULL, "agotado_atrapa");
        AFIRMAR(strstr(out, "0") != NULL, "agotado_vacio");
    }

    /* Estado del programa intacto tras atrapar (variables siguen bien) */
    {
        char out[256];
        int rc = ejecutar_capturando(
            "contador = 42\n"
            "intentar:\n"
            "    para v en (1 / z para z en [0]):\n"
            "        imprimir(v)\n"
            "    fin para\n"
            "atrapar ErrorAritmetico:\n"
            "    pasar\n"
            "fin intentar\n"
            "contador = contador + 1\n"
            "imprimir(contador)\n",
            out, sizeof(out));
        AFIRMAR(rc == 0, "estado_rc");
        AFIRMAR(strstr(out, "43") != NULL, "estado_intacto");
    }

    if (fallos == 0) {
        printf("gen_excepcion: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "gen_excepcion: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
