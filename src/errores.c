#include "errores.h"

#include <stdlib.h>
#include <string.h>

#include "lexer.h"

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

/*
 * Localiza el inicio de la línea N (1-indexed) en `fuente` y devuelve
 * su longitud (sin incluir el '\n' final). Si la línea no existe
 * (línea > líneas en archivo), devuelve NULL y *longitud = 0.
 */
static const char *encontrar_linea(const char *fuente, int linea_buscada,
                                   int *longitud) {
    *longitud = 0;
    if (fuente == NULL || linea_buscada < 1) return NULL;

    const char *inicio_linea = fuente;
    int linea_actual = 1;
    while (linea_actual < linea_buscada && *fuente != '\0') {
        if (*fuente == '\n') {
            linea_actual++;
            inicio_linea = fuente + 1;
        }
        fuente++;
    }
    if (linea_actual != linea_buscada) return NULL;

    /* Calcular longitud hasta el siguiente \n o EOF. */
    const char *fin = inicio_linea;
    while (*fin != '\0' && *fin != '\n') fin++;
    *longitud = (int)(fin - inicio_linea);
    return inicio_linea;
}

void error_imprimir(const Error *e, const char *fuente,
                    int longitud_span, FILE *salida) {
    const char *archivo = e->archivo ? e->archivo : "<repl>";

    if (e->linea > 0) {
        fprintf(salida, "%s en %s:%d:%d\n",
                e->categoria, archivo, e->linea, e->columna_inicio);
    } else {
        fprintf(salida, "%s en %s\n", e->categoria, archivo);
    }

    /* Contexto de la línea con caret, si tenemos fuente y posición. */
    if (fuente != NULL && e->linea > 0 && e->columna_inicio > 0) {
        int len_linea = 0;
        const char *linea = encontrar_linea(fuente, e->linea, &len_linea);
        if (linea != NULL) {
            fprintf(salida, "    %.*s\n", len_linea, linea);

            /* Caret indicators: 4 espacios de margen + (columna-1) espacios + carets. */
            fputs("    ", salida);
            for (int i = 0; i < e->columna_inicio - 1; i++) fputc(' ', salida);
            int n_carets = longitud_span > 0 ? longitud_span : 1;
            for (int i = 0; i < n_carets; i++) fputc('^', salida);
            fputc('\n', salida);
        }
    }

    if (e->mensaje) {
        fprintf(salida, "%s\n", e->mensaje);
    }

    if (e->sugerencia) {
        fprintf(salida, "sugerencia: %s\n", e->sugerencia);
    }
}

void error_imprimir_token(const struct Token *token,
                          const char *fuente,
                          const char *archivo,
                          FILE *salida) {
    if (token == NULL || token->tipo != TT_ERROR) return;

    Error e;
    error_iniciar(&e, "ErrorDeSintaxis");
    e.archivo = archivo;
    e.linea = token->linea;
    e.columna_inicio = token->columna;
    e.columna_fin = token->columna + token->longitud;
    /* No usamos error_set_mensaje (que duplica): apuntamos directamente
       al mensaje estático del token. Para evitar que error_destruir
       intente liberarlo, dejamos e.mensaje en NULL y formateamos aquí. */
    e.mensaje = NULL;

    /* Imprimir la cabecera + línea + caret manualmente sin malloc. */
    fprintf(salida, "%s en %s:%d:%d\n",
        e.categoria,
        e.archivo ? e.archivo : "<repl>",
        e.linea, e.columna_inicio);

    if (fuente != NULL) {
        int len_linea = 0;
        const char *linea = encontrar_linea(fuente, e.linea, &len_linea);
        if (linea != NULL) {
            fprintf(salida, "    %.*s\n", len_linea, linea);
            fputs("    ", salida);
            for (int i = 0; i < e.columna_inicio - 1; i++) fputc(' ', salida);
            int n = token->longitud > 0 ? token->longitud : 1;
            for (int i = 0; i < n; i++) fputc('^', salida);
            fputc('\n', salida);
        }
    }

    if (token->mensaje) {
        fprintf(salida, "%s\n", token->mensaje);
    }

    error_destruir(&e);
}
