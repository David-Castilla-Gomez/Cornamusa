#include "depurador.h"

#include <stdlib.h>
#include <string.h>

static void indexar_lineas(Depurador *d) {
    /* Cuenta lineas y registra offset de inicio de cada una. */
    int n = 1;
    for (int i = 0; i < d->fuente_len; i++) {
        if (d->fuente[i] == '\n') n++;
    }
    d->n_lineas = n;
    d->lineas_offset = malloc(sizeof(int) * (size_t)(n + 1));
    if (!d->lineas_offset) { d->n_lineas = 0; return; }
    d->lineas_offset[0] = 0;
    d->lineas_offset[1] = 0;  /* linea 1 empieza en offset 0 */
    int linea = 2;
    for (int i = 0; i < d->fuente_len; i++) {
        if (d->fuente[i] == '\n' && linea <= n) {
            d->lineas_offset[linea++] = i + 1;
        }
    }
    /* Sentinela al final para simplificar logica. */
    if (linea <= n + 1) d->lineas_offset[linea] = d->fuente_len;
}

void depurador_iniciar(Depurador *d) {
    memset(d, 0, sizeof(*d));
    d->ultima_linea = -1;
    d->ultimo_n_frames = -1;
}

void depurador_destruir(Depurador *d) {
    free(d->fuente);
    free(d->lineas_offset);
    memset(d, 0, sizeof(*d));
}

void depurador_activar(Depurador *d, const char *fuente, const char *ruta) {
    int len = (int)strlen(fuente);
    d->fuente = malloc((size_t)len + 1);
    if (d->fuente) {
        memcpy(d->fuente, fuente, (size_t)len + 1);
        d->fuente_len = len;
        indexar_lineas(d);
    }
    d->ruta = ruta;
    d->activo = true;
    /* Pausa en la primera linea para que el usuario tenga chance de
     * poner breakpoints antes de que empiece. */
    d->modo = DEP_PASO;
    d->frame_objetivo = 0;
    d->ultima_linea = -1;
}

void depurador_desactivar(Depurador *d) {
    d->activo = false;
}
