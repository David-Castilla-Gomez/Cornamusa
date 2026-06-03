/*
 * Tests de walrus operator `:=` (v1.113).
 *
 * Sintaxis: `nombre := valor` es una expresion. Asigna `valor` a
 * `nombre` y deja el valor en stack como resultado de la expresion.
 *
 * Permite patrones como:
 *   si (n := f()) > 0: ...      # usar el valor recien computado
 *   mientras (x := siguiente()) != nulo: ...
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
        "test_walrus_out.txt";
#else
        "/tmp/test_walrus_out.txt";
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
    /* Walrus en top-level: crea global y devuelve valor */
    {
        char out[256];
        ejecutar_capturando(
            "(n := 5)\n"
            "imprimir(n)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "5") != NULL, "walrus_global");
    }

    /* Valor de la expresion walrus se puede usar */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir((n := 10) * 2)\n"
            "imprimir(n)\n", out, sizeof(out));
        /* Esperamos "20" (10*2) y "10" (valor de n tras la asignacion) */
        AFIRMAR(strstr(out, "20") != NULL, "walrus_valor_en_expr");
        AFIRMAR(strstr(out, "10") != NULL, "walrus_global_persiste");
    }

    /* Walrus en condicion de `si` */
    {
        char out[256];
        ejecutar_capturando(
            "si (n := 7) > 5:\n"
            "    imprimir(\"grande:\", n)\n"
            "sino:\n"
            "    imprimir(\"chico:\", n)\n"
            "fin si\n", out, sizeof(out));
        AFIRMAR(strstr(out, "grande: 7") != NULL, "walrus_en_si");
    }

    /* Walrus en mientras (variable existente reasignada) */
    {
        char out[512];
        ejecutar_capturando(
            "contador = 0\n"
            "x = 1\n"
            "mientras (v := x) <= 3:\n"
            "    contador = contador + v\n"
            "    x = x + 1\n"
            "fin mientras\n"
            "imprimir(contador)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "6") != NULL, "walrus_en_mientras");
    }

    /* Walrus dentro de funcion: nuevo local */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f(x):\n"
            "    si (cuad := x * x) > 10:\n"
            "        retornar cuad\n"
            "    fin si\n"
            "    retornar -cuad\n"
            "fin funcion\n"
            "imprimir(f(2))\n"
            "imprimir(f(5))\n", out, sizeof(out));
        /* f(2): cuad=4, no > 10, retorna -4 */
        /* f(5): cuad=25, > 10, retorna 25 */
        AFIRMAR(strstr(out, "-4") != NULL, "walrus_func_local_chico");
        AFIRMAR(strstr(out, "25") != NULL, "walrus_func_local_grande");
    }

    /* Walrus reasigna variable existente en funcion */
    {
        char out[256];
        ejecutar_capturando(
            "funcion f():\n"
            "    x = 1\n"
            "    si (x := 100) > 50:\n"
            "        imprimir(\"reasignado:\", x)\n"
            "    fin si\n"
            "fin funcion\n"
            "f()\n", out, sizeof(out));
        AFIRMAR(strstr(out, "reasignado: 100") != NULL, "walrus_reasigna_local");
    }

    /* Walrus con expresion compuesta */
    {
        char out[256];
        ejecutar_capturando(
            "lista = [1, 2, 3, 4, 5]\n"
            "si (n := longitud(lista)) > 3:\n"
            "    imprimir(f\"lista grande {n=}\")\n"
            "fin si\n", out, sizeof(out));
        AFIRMAR(strstr(out, "lista grande n=5") != NULL, "walrus_funcion_call");
    }

    /* Walrus anidado en expresion mas grande */
    {
        char out[256];
        ejecutar_capturando(
            "resultado = 3 + (x := 10) * 2\n"
            "imprimir(resultado)\n"
            "imprimir(x)\n", out, sizeof(out));
        /* 3 + 10 * 2 = 23 */
        AFIRMAR(strstr(out, "23") != NULL, "walrus_anidado_aritmetico");
        AFIRMAR(strstr(out, "10") != NULL, "walrus_x_global");
    }

    /* Walrus en arg de funcion */
    {
        char out[256];
        ejecutar_capturando(
            "imprimir((nombre := \"Ana\"))\n"
            "imprimir(\"hola \" + nombre)\n", out, sizeof(out));
        AFIRMAR(strstr(out, "Ana") != NULL, "walrus_cadena");
        AFIRMAR(strstr(out, "hola Ana") != NULL, "walrus_cadena_usado");
    }

    /* Sintaxis distinta: := vs : (no rompe parsing de dict literal) */
    {
        char out[256];
        ejecutar_capturando(
            "d = {\"clave\": 42}\n"
            "imprimir(d[\"clave\"])\n", out, sizeof(out));
        AFIRMAR(strstr(out, "42") != NULL, "dict_literal_no_se_rompe");
    }

    /* Compatibilidad: codigo sin := sigue funcionando */
    {
        char out[256];
        ejecutar_capturando(
            "x = 10\n"
            "si x > 5:\n"
            "    imprimir(\"ok\")\n"
            "fin si\n", out, sizeof(out));
        AFIRMAR(strstr(out, "ok") != NULL, "compat_sin_walrus");
    }

    if (fallos == 0) {
        printf("walrus: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "walrus: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
