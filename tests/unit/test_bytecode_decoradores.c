/*
 * Tests del azucar @decorador (v1.72).
 *
 * Verifica:
 *   - Decorador simple: f = dec(f).
 *   - Stacking: @a + @b + f → f = a(b(f)).
 *   - Decorador con argumentos (factory): @retry(3) + f → f = retry(3)(f).
 *   - Error de sintaxis: @x sin funcion despues lanza ErrorDeSintaxis.
 *   - Decoradores en funcion anidada (local) funcionan igual.
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
        "test_dec_out.txt";
#else
        "/tmp/test_dec_out.txt";
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
    } else {
        rc = -2;  /* error de sintaxis */
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
    /* Test 1: decorador simple aplica una vez. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "funcion dec(f):\n"
            "    funcion w(x):\n"
            "        retornar f(x) + 100\n"
            "    fin funcion\n"
            "    retornar w\n"
            "fin funcion\n"
            "@dec\n"
            "funcion ident(x):\n"
            "    retornar x\n"
            "fin funcion\n"
            "imprimir(ident(5))\n", out, sizeof(out));
        AFIRMAR(rc == 0, "simple_ejecuta");
        AFIRMAR(strstr(out, "105") != NULL, "simple_resultado_105");
    }

    /* Test 2: stacking @a + @b aplica el mas cercano primero.
     * f = a(b(f)). Para x=5: b suma 1 → 6, a multiplica *2 → 12. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "funcion a(f):\n"
            "    funcion w(x):\n"
            "        retornar f(x) * 2\n"
            "    fin funcion\n"
            "    retornar w\n"
            "fin funcion\n"
            "funcion b(f):\n"
            "    funcion w(x):\n"
            "        retornar f(x) + 1\n"
            "    fin funcion\n"
            "    retornar w\n"
            "fin funcion\n"
            "@a\n"
            "@b\n"
            "funcion ident(x):\n"
            "    retornar x\n"
            "fin funcion\n"
            "imprimir(ident(5))\n", out, sizeof(out));
        AFIRMAR(rc == 0, "stacking_ejecuta");
        AFIRMAR(strstr(out, "12") != NULL, "stacking_12");
    }

    /* Test 3: orden inverso de stacking. @b + @a → f = b(a(f)).
     * Para x=5: a multiplica *2 → 10, b suma 1 → 11. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "funcion a(f):\n"
            "    funcion w(x):\n"
            "        retornar f(x) * 2\n"
            "    fin funcion\n"
            "    retornar w\n"
            "fin funcion\n"
            "funcion b(f):\n"
            "    funcion w(x):\n"
            "        retornar f(x) + 1\n"
            "    fin funcion\n"
            "    retornar w\n"
            "fin funcion\n"
            "@b\n"
            "@a\n"
            "funcion ident(x):\n"
            "    retornar x\n"
            "fin funcion\n"
            "imprimir(ident(5))\n", out, sizeof(out));
        AFIRMAR(rc == 0, "stacking_inverso_ejecuta");
        AFIRMAR(strstr(out, "11") != NULL, "stacking_inverso_11");
    }

    /* Test 4: decorador con argumentos (factory). */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "funcion rep(n):\n"
            "    funcion dec(f):\n"
            "        funcion w(s):\n"
            "            r = \"\"\n"
            "            para i en rango(n):\n"
            "                r = r + f(s)\n"
            "            fin para\n"
            "            retornar r\n"
            "        fin funcion\n"
            "        retornar w\n"
            "    fin funcion\n"
            "    retornar dec\n"
            "fin funcion\n"
            "@rep(3)\n"
            "funcion eco(s):\n"
            "    retornar s\n"
            "fin funcion\n"
            "imprimir(eco(\"ja\"))\n", out, sizeof(out));
        AFIRMAR(rc == 0, "factory_ejecuta");
        AFIRMAR(strstr(out, "jajaja") != NULL, "factory_jajaja");
    }

    /* Test 5: @ sin funcion despues lanza ErrorDeSintaxis. */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "@x\n"
            "y = 1\n", out, sizeof(out));
        AFIRMAR(rc == -2, "at_sin_funcion_es_error_sintaxis");
    }

    /* Test 6: decorador dentro de funcion anidada (local). */
    {
        char out[1024];
        int rc = ejecutar_capturando(
            "funcion exterior():\n"
            "    funcion dec(f):\n"
            "        funcion w(x):\n"
            "            retornar f(x) - 10\n"
            "        fin funcion\n"
            "        retornar w\n"
            "    fin funcion\n"
            "    @dec\n"
            "    funcion interior(x):\n"
            "        retornar x\n"
            "    fin funcion\n"
            "    retornar interior(50)\n"
            "fin funcion\n"
            "imprimir(exterior())\n", out, sizeof(out));
        AFIRMAR(rc == 0, "local_dec_ejecuta");
        AFIRMAR(strstr(out, "40") != NULL, "local_dec_40");
    }

    if (fallos == 0) {
        printf("decoradores: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "decoradores: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
