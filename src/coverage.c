#include "coverage.h"

#include <stdlib.h>
#include <string.h>

static void asegurar_capacidad(CovTracker *c, int linea) {
    if (linea < c->linea_max) return;
    int nuevo_max = c->linea_max == 0 ? 256 : c->linea_max * 2;
    while (nuevo_max <= linea) nuevo_max *= 2;
    size_t nuevo_bytes = (size_t)((nuevo_max + 7) / 8);
    size_t viejo_bytes = (size_t)((c->linea_max + 7) / 8);
    uint8_t *nuevo = realloc(c->bits_tocadas, nuevo_bytes);
    if (!nuevo) return;
    memset(nuevo + viejo_bytes, 0, nuevo_bytes - viejo_bytes);
    c->bits_tocadas = nuevo;
    c->linea_max = nuevo_max;
}

void cov_iniciar(CovTracker *c) {
    memset(c, 0, sizeof(*c));
}

void cov_destruir(CovTracker *c) {
    free(c->bits_tocadas);
    memset(c, 0, sizeof(*c));
}

void cov_activar(CovTracker *c, const Chunk *chunk_objetivo) {
    c->activo = true;
    c->chunk_objetivo = chunk_objetivo;
    c->ultima_linea = -1;
}

void cov_desactivar(CovTracker *c) {
    c->activo = false;
}

void cov_on_linea(CovTracker *c, const Chunk *chunk, int linea) {
    if (!c->activo) return;
    if (chunk != c->chunk_objetivo) return;
    if (linea == c->ultima_linea) return;  /* fast path: misma línea */
    if (linea <= 0) return;
    asegurar_capacidad(c, linea);
    if (linea < c->linea_max) {
        c->bits_tocadas[linea / 8] |= (uint8_t)(1u << (linea & 7));
    }
    c->ultima_linea = linea;
}

static bool bit_set(const uint8_t *bits, int linea) {
    return (bits[linea / 8] & (uint8_t)(1u << (linea & 7))) != 0;
}

void cov_calcular(const CovTracker *c, CovReporte *out) {
    memset(out, 0, sizeof(*out));
    if (!c->chunk_objetivo) return;
    const Chunk *ck = c->chunk_objetivo;

    /* Enumerar las líneas ejecutables (las que tienen al menos un byte
     * de bytecode). chunk->lineas tiene una entrada por byte de codigo;
     * agrupamos las únicas. */
    int max_l = 0;
    for (int i = 0; i < ck->cuenta; i++) {
        if (ck->lineas[i] > max_l) max_l = ck->lineas[i];
    }
    if (max_l <= 0) return;

    /* Bitset auxiliar de líneas ejecutables. */
    size_t nbytes = (size_t)((max_l + 1 + 7) / 8);
    uint8_t *bits_exec = calloc(1, nbytes);
    if (!bits_exec) return;
    for (int i = 0; i < ck->cuenta; i++) {
        int l = ck->lineas[i];
        if (l > 0) bits_exec[l / 8] |= (uint8_t)(1u << (l & 7));
    }

    int ejecutables = 0, tocadas = 0;
    for (int l = 1; l <= max_l; l++) {
        if (!bit_set(bits_exec, l)) continue;
        ejecutables++;
        bool tocada = (l < c->linea_max && bit_set(c->bits_tocadas, l));
        if (tocada) {
            tocadas++;
        } else if (out->n_uncovered < (int)(sizeof(out->uncovered)/sizeof(out->uncovered[0]))) {
            out->uncovered[out->n_uncovered++] = l;
        }
    }
    out->lineas_ejecutables = ejecutables;
    out->lineas_tocadas = tocadas;
    free(bits_exec);
}

void cov_dump(const CovTracker *c, FILE *out, const char *ruta_fuente,
              bool listar_uncovered) {
    CovReporte r;
    cov_calcular(c, &r);
    if (r.lineas_ejecutables == 0) {
        fprintf(out, "(coverage: no hay lineas ejecutables registradas)\n");
        return;
    }
    double pct = 100.0 * (double)r.lineas_tocadas / (double)r.lineas_ejecutables;
    fprintf(out, "\n%s: %.1f%% (%d/%d lineas top-level)\n",
            ruta_fuente ? ruta_fuente : "<programa>",
            pct, r.lineas_tocadas, r.lineas_ejecutables);
    if (listar_uncovered && r.n_uncovered > 0) {
        fprintf(out, "  lineas no cubiertas: ");
        for (int i = 0; i < r.n_uncovered; i++) {
            fprintf(out, "%s%d", i > 0 ? ", " : "", r.uncovered[i]);
        }
        fprintf(out, "\n");
        if (r.n_uncovered >= (int)(sizeof(r.uncovered)/sizeof(r.uncovered[0]))) {
            fprintf(out, "  (truncado a %d primeras lineas no cubiertas)\n",
                    r.n_uncovered);
        }
    }
    fprintf(out,
            "  NOTA v1.75: solo cubre el codigo top-level del archivo "
            "principal.\n"
            "  Cuerpos de funciones/closures van en chunks propios y no "
            "se cuentan aun.\n");
}
