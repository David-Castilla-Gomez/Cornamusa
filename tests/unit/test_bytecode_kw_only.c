/*
 * Tests de parametros keyword-only despues de *args (v1.182).
 *
 * Antes: `def f(*args, kw=default)` daba "variadicos no se combinan
 * con defaults". Ahora soportado, paridad con Python:
 *   - Llamada posicional: los extras van a *args; kw-only usa default.
 *   - Llamada con keyword: kw=val sobrescribe el default.
 *
 * Restriccion v1.182: kw-only obligatorios (sin default) no soportados.
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
        "test_kw_only_out.txt";
#else
        "/tmp/test_kw_only_out.txt";
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
    /* sumar(*args, inicial=0) — caso basico */
    {
        char out[256];
        ejecutar_capturando(
            "funcion sumar(*args, inicial=0):\n"
            "    t = inicial\n"
            "    para xx en args:\n"
            "        t = t + xx\n"
            "    fin para\n"
            "    retornar t\n"
            "fin funcion\n"
            "imprimir(sumar(1, 2, 3))\n"
            "imprimir(sumar(1, 2, 3, inicial=10))\n"
            "imprimir(sumar(inicial=100))\n"
            "imprimir(sumar())\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "6\n16\n100\n0") != NULL, "sumar_kw_only");
    }

    /* Multiples kw-only */
    {
        char out[256];
        ejecutar_capturando(
            "funcion log_evt(*args, nivel=\"INFO\", canal=\"d\"):\n"
            "    imprimir(nivel, canal, longitud(args))\n"
            "fin funcion\n"
            "log_evt(\"a\", \"b\")\n"
            "log_evt(\"x\", nivel=\"ERR\")\n"
            "log_evt(\"e\", nivel=\"WARN\", canal=\"auth\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "INFO d 2") != NULL, "log_default");
        AFIRMAR(strstr(out, "ERR d 1") != NULL, "log_nivel");
        AFIRMAR(strstr(out, "WARN auth 1") != NULL, "log_full");
    }

    /* Params fijos + *args + kw-only */
    {
        char out[256];
        ejecutar_capturando(
            "funcion mezclar(prefijo, *args, sep=\"-\"):\n"
            "    cuerpo = \"\"\n"
            "    primero = verdadero\n"
            "    para xx en args:\n"
            "        si no primero:\n"
            "            cuerpo = cuerpo + sep\n"
            "        fin si\n"
            "        cuerpo = cuerpo + cadena(xx)\n"
            "        primero = falso\n"
            "    fin para\n"
            "    retornar prefijo + \":\" + cuerpo\n"
            "fin funcion\n"
            "imprimir(mezclar(\"a\", 1, 2, 3))\n"
            "imprimir(mezclar(\"b\", \"x\", \"y\", sep=\"|\"))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "a:1-2-3") != NULL, "fijo_kw_only_default");
        AFIRMAR(strstr(out, "b:x|y") != NULL, "fijo_kw_only_override");
    }

    /* Sin args posicionales + kw-only */
    {
        char out[256];
        ejecutar_capturando(
            "funcion solo_kw(*args, x=10):\n"
            "    retornar x + longitud(args)\n"
            "fin funcion\n"
            "imprimir(solo_kw())\n"
            "imprimir(solo_kw(1, 2))\n"
            "imprimir(solo_kw(1, 2, x=100))\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "10\n12\n102") != NULL, "solo_kw");
    }

    if (fallos == 0) {
        printf("kw_only: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "kw_only: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
