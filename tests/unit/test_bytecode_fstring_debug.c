/*
 * Tests de f-string debug format `f"{x=}"` (v1.112).
 *
 * Sintaxis: dentro de una interpolacion `{expr=}` se emite literal
 * "expr=" (la expresion tal cual escribio el usuario) seguido del
 * valor formateado. Espacios antes/despues del `=` se preservan.
 * Sin ambiguar con operadores `==`, `!=`, `<=`, `>=`.
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
        "test_fdbg_out.txt";
#else
        "/tmp/test_fdbg_out.txt";
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
    /* Basico: f"{x=}" → "x=5" */
    {
        char out[256];
        ejecutar_capturando(
            "x = 5\n"
            "imprimir(f\"{x=}\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "x=5") != NULL, "basico");
    }

    /* Espacios alrededor del = se preservan */
    {
        char out[256];
        ejecutar_capturando(
            "x = 5\n"
            "imprimir(f\"{x = }\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "x = 5") != NULL, "espacios_preservados");
    }

    /* Expresion compuesta: f"{x*2=}" → "x*2=10" */
    {
        char out[256];
        ejecutar_capturando(
            "x = 5\n"
            "imprimir(f\"{x*2=}\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "x*2=10") != NULL, "compuesta");
    }

    /* Combinacion debug + spec: f"{x=:>5}" → "x=    5" */
    {
        char out[256];
        ejecutar_capturando(
            "x = 5\n"
            "imprimir(f\"{x=:>5}\")\n", out, sizeof(out));
        /* Esperamos "x=    5" (4 espacios de padding antes del 5) */
        AFIRMAR(strstr(out, "x=    5") != NULL, "debug_con_spec");
    }

    /* Multiples debug en una sola f-string */
    {
        char out[256];
        ejecutar_capturando(
            "a = 1\n"
            "b = 2\n"
            "imprimir(f\"{a=}, {b=}\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "a=1, b=2") != NULL, "multiples");
    }

    /* Mezcla debug + no-debug + literal */
    {
        char out[256];
        ejecutar_capturando(
            "x = 5\n"
            "suma = 10\n"
            "imprimir(f\"DEBUG: {x=} resultado={suma}\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "DEBUG: x=5 resultado=10") != NULL,
                "mezcla_debug_normal");
    }

    /* Operador == NO debe interpretarse como debug */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{1 == 2}\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "falso") != NULL, "operador_igualdad_no_debug");
        AFIRMAR(strstr(out, "==") == NULL, "operador_no_emite_literal");
    }

    /* Operadores !=, <=, >= tampoco */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{1 != 2}\")\n"
            "imprimir(f\"{1 <= 2}\")\n"
            "imprimir(f\"{2 >= 1}\")\n", out, sizeof(out));
        int n_v = 0;
        const char *p = out;
        while ((p = strstr(p, "verdadero")) != NULL) { n_v++; p++; }
        AFIRMAR(n_v == 3, "operadores_relacionales_no_debug");
    }

    /* Debug con cadena */
    {
        char out[256];
        ejecutar_capturando(
            "nombre = \"Ana\"\n"
            "imprimir(f\"{nombre=}\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "nombre=Ana") != NULL, "cadena_debug");
    }

    /* Debug con acceso a atributo */
    {
        char out[256];
        ejecutar_capturando(
            "clase P:\n"
            "    funcion __iniciar__(yo):\n"
            "        yo.x = 42\n"
            "    fin funcion\n"
            "fin clase\n"
            "p = P()\n"
            "imprimir(f\"{p.x=}\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "p.x=42") != NULL, "atributo_debug");
    }

    /* Debug con expresion mas larga */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir(f\"{longitud([1,2,3])=}\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "longitud([1,2,3])=3") != NULL, "expr_compleja_debug");
    }

    /* Mismo programa funciona sin = (compatibilidad hacia atras) */
    {
        char out[256];
        ejecutar_capturando(
            "x = 5\n"
            "imprimir(f\"valor: {x}\")\n", out, sizeof(out));
        AFIRMAR(strstr(out, "valor: 5") != NULL, "sin_debug_compat");
    }

    if (fallos == 0) {
        printf("fstring_debug: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "fstring_debug: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
