/*
 * Smoke test mínimo: verifica que el proyecto enlaza correctamente y que
 * las constantes de versión están definidas. Sin este test el build de
 * tests no tendría nada que correr.
 */

#include <stdio.h>
#include <string.h>

#include "common.h"

static int fallos = 0;

#define AFIRMAR(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "FALLO en %s:%d: %s\n", __FILE__, __LINE__, #cond);\
            fallos++;                                                          \
        }                                                                      \
    } while (0)

int main(void) {
    AFIRMAR(strlen(CORNAMUSA_VERSION) > 0);
    AFIRMAR(CORNAMUSA_VERSION_MAJOR == 0);
    AFIRMAR(CORNAMUSA_VERSION_MINOR == 11);
    AFIRMAR(CORNAMUSA_VERSION_PATCH == 0);

    if (fallos == 0) {
        printf("smoke: todos los asserts pasan\n");
        return 0;
    }
    fprintf(stderr, "smoke: %d falló(s)\n", fallos);
    return 1;
}
