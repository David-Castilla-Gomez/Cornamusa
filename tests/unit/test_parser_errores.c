/*
 * Tests del mecanismo de captura de errores del parser (v1.53).
 *
 * Verifica que el parser pueda acumular errores como datos
 * estructurados (linea, columna, mensaje) en lugar de imprimirlos a
 * stderr. Usado por el LSP para emitir diagnostics.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "ast.h"
#include "lexer.h"
#include "parser.h"

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

static int parsear_capturando(const char *fuente, ErroresParser *out) {
    Lexer l;
    lexer_iniciar(&l, fuente, "<test>");
    Arena a;
    arena_iniciar(&a, 4096);
    Parser p;
    parser_iniciar(&p, &l, &a, fuente, "<test>");
    p.capturar_errores = true;
    p.errores_capturados = out;
    int n;
    Sent **sents = parser_parsear_programa(&p, &n);
    (void)sents;
    int tuvo = p.tuvo_error ? 1 : 0;
    arena_destruir(&a);
    return tuvo;
}

int main(void) {
    /* Programa valido: no debe haber errores capturados. */
    {
        ErroresParser e = {0};
        int tuvo = parsear_capturando(
            "funcion f(n):\n"
            "    retornar n + 1\n"
            "fin funcion\n", &e);
        AFIRMAR(tuvo == 0, "valido_no_error");
        AFIRMAR(e.n == 0, "valido_n_cero");
        parser_errores_liberar(&e);
    }

    /* Error de sintaxis simple: token inesperado. */
    {
        ErroresParser e = {0};
        int tuvo = parsear_capturando("funcion (", &e);
        AFIRMAR(tuvo == 1, "syntax_tuvo_error");
        AFIRMAR(e.n >= 1, "syntax_capturado");
        AFIRMAR(e.items[0].linea == 1, "syntax_linea_1");
        AFIRMAR(e.items[0].mensaje != NULL, "syntax_mensaje_no_nulo");
        AFIRMAR(strlen(e.items[0].mensaje) > 0, "syntax_mensaje_no_vacio");
        parser_errores_liberar(&e);
    }

    /* Varios errores en lineas distintas (parser intenta recuperarse). */
    {
        ErroresParser e = {0};
        int tuvo = parsear_capturando(
            "x = 1 +\n"        /* parser error en linea 1 */
            "y = 2 +\n"        /* parser error en linea 2 */
            "imprimir(x)\n", &e);
        AFIRMAR(tuvo == 1, "varios_tuvo_error");
        AFIRMAR(e.n >= 1, "varios_capturado_al_menos_uno");
        parser_errores_liberar(&e);
    }

    /* Liberacion correcta — no segfault, no leak (segun valgrind). */
    {
        ErroresParser e = {0};
        parsear_capturando("invalid syntax (((", &e);
        parser_errores_liberar(&e);
        AFIRMAR(e.items == NULL, "liberar_items_null");
        AFIRMAR(e.n == 0, "liberar_n_cero");
        AFIRMAR(e.capacidad == 0, "liberar_cap_cero");
    }

    /* Sin captura: comportamiento clasico (no se llena la lista,
     * pero el parser detecta el error). */
    {
        Lexer l;
        lexer_iniciar(&l, "funcion (", "<test>");
        Arena a;
        arena_iniciar(&a, 4096);
        Parser p;
        parser_iniciar(&p, &l, &a, "funcion (", "<test>");
        /* Default: capturar_errores = false. */
        int n;
        /* Suprimimos stderr durante este test para no contaminar el
         * output. freopen es la forma portable de redirigir stderr. */
        const char *devnull =
#ifdef _WIN32
            "nul";
#else
            "/dev/null";
#endif
        FILE *prev_stderr = freopen(devnull, "w", stderr);
        Sent **sents = parser_parsear_programa(&p, &n);
        (void)sents;
        (void)prev_stderr;
        /* Restaurar stderr a la consola (no perfecto pero suficiente
         * para tests CI). */
        freopen("CON", "w", stderr);
        AFIRMAR(p.tuvo_error == true, "sin_captura_tuvo_error");
        AFIRMAR(p.errores_capturados == NULL, "sin_captura_lista_nula");
        arena_destruir(&a);
    }

    if (fallos == 0) {
        printf("parser_errores: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "parser_errores: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
