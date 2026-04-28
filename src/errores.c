#include "errores.h"

#include <stdlib.h>
#include <string.h>

static char *duplicar(const char *texto) {
    if (texto == NULL) return NULL;
    size_t len = strlen(texto);
    char *copia = (char *)malloc(len + 1);
    if (copia == NULL) return NULL;
    memcpy(copia, texto, len + 1);
    return copia;
}

void error_iniciar(Error *e, const char *categoria) {
    e->categoria = categoria;
    e->archivo = NULL;
    e->linea = 0;
    e->columna_inicio = 0;
    e->columna_fin = 0;
    e->mensaje = NULL;
    e->sugerencia = NULL;
}

void error_destruir(Error *e) {
    free(e->mensaje);
    free(e->sugerencia);
    e->mensaje = NULL;
    e->sugerencia = NULL;
}

void error_set_mensaje(Error *e, const char *texto) {
    free(e->mensaje);
    e->mensaje = duplicar(texto);
}

void error_set_sugerencia(Error *e, const char *texto) {
    free(e->sugerencia);
    e->sugerencia = duplicar(texto);
}

void error_imprimir(const Error *e, FILE *salida) {
    const char *archivo = e->archivo ? e->archivo : "<repl>";

    if (e->linea > 0) {
        fprintf(salida, "%s en %s:%d:%d\n",
                e->categoria, archivo, e->linea, e->columna_inicio);
    } else {
        fprintf(salida, "%s en %s\n", e->categoria, archivo);
    }

    if (e->mensaje) {
        fprintf(salida, "    %s\n", e->mensaje);
    }

    if (e->sugerencia) {
        fprintf(salida, "sugerencia: %s\n", e->sugerencia);
    }
}
