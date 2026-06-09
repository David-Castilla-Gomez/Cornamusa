/*
 * Tests de conversores `!r`, `!s` y `!a` en f-strings (v1.186).
 *
 * Paridad Python:
 *   f"{x!r}" → repr(x)
 *   f"{x!s}" → str(x) (igual que f"{x}" por defecto)
 *   f"{x!a}" → ascii(x) (alias de repr en Cornamusa)
 *
 * Funciona combinado con fmt spec:
 *   f"{x!r:>10}" → repr alineado a derecha en ancho 10
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
        "test_fstring_conversor_out.txt";
#else
        "/tmp/test_fstring_conversor_out.txt";
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
    /* !r en cadena -> con comillas */
    {
        char out[256];
        ejecutar_capturando(
            "s = \"hola\"\n"
            "imprimir(f\"{s!r}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "\"hola\"") != NULL, "repr_cadena");
    }

    /* !s en cadena -> sin comillas (igual que sin conv) */
    {
        char out[256];
        ejecutar_capturando(
            "s = \"hola\"\n"
            "imprimir(f\"{s!s}\")\n"
            "imprimir(f\"{s}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "hola\nhola") != NULL, "str_igual_default");
    }

    /* !a alias de repr en Cornamusa */
    {
        char out[256];
        ejecutar_capturando(
            "s = \"hola\"\n"
            "imprimir(f\"{s!a}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "\"hola\"") != NULL, "ascii_repr");
    }

    /* Conversor + spec */
    {
        char out[256];
        ejecutar_capturando(
            "s = \"abc\"\n"
            "imprimir(f\"{s!r:>10}\")\n"
            "imprimir(f\"{s!s:<10}|\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "     \"abc\"") != NULL, "repr_alineado");
        AFIRMAR(strstr(out, "abc       |") != NULL, "str_alineado");
    }

    /* !r en diccionario */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"k\": 1}\n"
            "imprimir(f\"{d!r}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "{\"k\": 1}") != NULL, "repr_dicc");
    }

    /* !r en entero (idéntico a str) */
    {
        char out[256];
        ejecutar_capturando(
            "n = 42\n"
            "imprimir(f\"{n!r}\")\n"
            "imprimir(f\"{n!s}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "42\n42") != NULL, "entero_conv");
    }

    /* !r en tupla */
    {
        char out[256];
        ejecutar_capturando(
            "t = (1, \"x\", 3)\n"
            "imprimir(f\"{t!r}\")\n",
            out, sizeof(out));
        /* repr de tupla con strings interna debe mostrar comillas */
        AFIRMAR(strstr(out, "(1, \"x\", 3)") != NULL, "tupla_repr");
    }

    /* !r con lista */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{[1, 2, 3]!r}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "[1, 2, 3]") != NULL, "lista_repr");
    }

    /* Multiples conversores en una f-string */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"a={\"x\"!r} b={42!s}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "a=\"x\" b=42") != NULL, "multi");
    }

    /* Conversor invalido -> error de sintaxis */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{x!q}\")\n",
            out, sizeof(out));
        /* Programa no se ejecuta. */
        AFIRMAR(strlen(out) == 0, "conversor_invalido");
    }

    /* != no se confunde con conversor */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{1 != 2}\")\n",
            out, sizeof(out));
        AFIRMAR(strstr(out, "verdadero") != NULL, "no_confunde_neq");
    }

    if (fallos == 0) {
        printf("fstring_conversor: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "fstring_conversor: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
