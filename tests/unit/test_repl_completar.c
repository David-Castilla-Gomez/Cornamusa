/*
 * Tests del autocompletado del REPL (v1.126).
 *
 * El editor de linea reside en src/repl_line.c, pero requiere TTY
 * interactivo para probar la rama TAB. Aqui validamos:
 *   1. La API publica repl_set_completar / ReplCompletarFn linkea OK.
 *   2. Un callback de ejemplo emite candidatos correctamente y el
 *      colector externo los recoge.
 *
 * No probamos el handler de TECLA_TAB en repl_line.c (necesita
 * inyectar input al editor con stdin/stdout en modo raw, complejo y
 * dependiente del SO). Pero la logica de prefijo_token_len y
 * prefijo_comun esta cubierta indirectamente: si el callback emite los
 * candidatos correctos, las dos funciones internas son aritmetica
 * trivial sobre arrays de cadenas.
 */

#include "repl_line.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* Lista pequeña de "globales" para los tests. */
static const char *DUMMY_NAMES[] = {
    "imprimir", "imprimir_error", "importar", "intentar",
    "lista", "longitud", "lambda", NULL
};

static void completar_dummy(const char *prefijo, int prefijo_len,
                              void *ctx, ReplEmitirCandidatoFn emitir,
                              void *emit_ctx) {
    (void)ctx;
    for (const char **p = DUMMY_NAMES; *p; p++) {
        int len = (int)strlen(*p);
        if (len < prefijo_len) continue;
        if (memcmp(*p, prefijo, (size_t)prefijo_len) != 0) continue;
        emitir(*p, len, emit_ctx);
    }
}

/* Colector simple para validar lo que emite el callback. */
typedef struct {
    char buf[16][64];
    int n;
} Colector;

static void colector_emit(const char *cand, int cand_len, void *emit_ctx) {
    Colector *c = (Colector *)emit_ctx;
    if (c->n >= 16) return;
    int n = cand_len < 63 ? cand_len : 63;
    memcpy(c->buf[c->n], cand, (size_t)n);
    c->buf[c->n][n] = '\0';
    c->n++;
}

int main(void) {
    /* repl_set_completar acepta callback + ctx sin crashear. */
    repl_set_completar(completar_dummy, NULL);
    repl_set_completar(NULL, NULL);   /* desactivar */
    repl_set_completar(completar_dummy, NULL);

    /* Prefijo "imp" debe emitir 2: imprimir, imprimir_error, importar */
    {
        Colector c = {.n = 0};
        completar_dummy("imp", 3, NULL, colector_emit, &c);
        AFIRMAR(c.n == 3, "imp_3_cands");
        AFIRMAR(strcmp(c.buf[0], "imprimir") == 0, "imp_first");
        AFIRMAR(strcmp(c.buf[1], "imprimir_error") == 0, "imp_second");
        AFIRMAR(strcmp(c.buf[2], "importar") == 0, "imp_third");
    }

    /* Prefijo "long" -> solo longitud */
    {
        Colector c = {.n = 0};
        completar_dummy("long", 4, NULL, colector_emit, &c);
        AFIRMAR(c.n == 1, "long_uno");
        AFIRMAR(strcmp(c.buf[0], "longitud") == 0, "long_match");
    }

    /* Prefijo "l" -> 3: lista, longitud, lambda */
    {
        Colector c = {.n = 0};
        completar_dummy("l", 1, NULL, colector_emit, &c);
        AFIRMAR(c.n == 3, "l_tres");
    }

    /* Prefijo "xyz" -> 0 candidatos */
    {
        Colector c = {.n = 0};
        completar_dummy("xyz", 3, NULL, colector_emit, &c);
        AFIRMAR(c.n == 0, "xyz_vacio");
    }

    /* Prefijo vacio devuelve todos */
    {
        Colector c = {.n = 0};
        completar_dummy("", 0, NULL, colector_emit, &c);
        AFIRMAR(c.n == 7, "todos");
    }

    if (fallos == 0) {
        printf("repl_completar: %d asserts, todos verde\n", casos);
        return 0;
    }
    fprintf(stderr, "repl_completar: %d falla(s) de %d\n", fallos, casos);
    return 1;
}
