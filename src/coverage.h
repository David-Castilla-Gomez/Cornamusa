#ifndef CORNAMUSA_COVERAGE_H
#define CORNAMUSA_COVERAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "chunk.h"

/*
 * Coverage tool (v1.75). Registra qué líneas del archivo principal
 * han sido ejecutadas durante una corrida. Reusa la misma arquitectura
 * de hook al dispatch loop que el profiler:
 *   - cuando inactivo, los hooks son ~no-op.
 *   - cuando activo, mantiene un bitset "linea N tocada".
 *
 * Solo cubre el chunk principal (el que vm_ejecutar recibe directamente).
 * Funciones/closures dentro del mismo archivo emiten bytecode en chunks
 * separados pero comparten `lineas[]` con el archivo fuente — esos
 * chunks no se rastrean en esta versión.
 *
 * NOTA importante: las funciones definidas dentro del programa
 * principal compilan su cuerpo en un chunk PROPIO (FuncionBC.chunk),
 * no en el chunk top-level. Por tanto líneas DENTRO de funciones no
 * se marcan en esta primera version — limitación declarada. Se
 * documenta en el dump.
 */

typedef struct CovTracker {
    bool activo;
    const Chunk *chunk_objetivo;
    /* Bitset: 1 bit por línea hasta `linea_max`. linea 0 no se usa. */
    uint8_t *bits_tocadas;
    int linea_max;                /* tamaño actual del bitset en líneas */
    int ultima_linea;             /* última línea registrada (para evitar work redundante) */
} CovTracker;

void cov_iniciar(CovTracker *c);
void cov_destruir(CovTracker *c);

/* Activa el tracker para el chunk dado. NO toma posesión; el chunk
   debe vivir al menos hasta cov_dump. */
void cov_activar(CovTracker *c, const Chunk *chunk_objetivo);
void cov_desactivar(CovTracker *c);

/* Hook. Llamado por la VM en cada iteración del dispatch loop.
   No-op si inactivo o si chunk != chunk_objetivo. Es barato cuando
   la línea no ha cambiado respecto a la anterior (caso comun). */
void cov_on_linea(CovTracker *c, const Chunk *chunk, int linea);

/* Calcula stats finales y vuelca a `out`. `ruta_fuente` se usa solo
   para etiquetar. */
typedef struct CovReporte {
    int lineas_ejecutables;
    int lineas_tocadas;
    /* Líneas no tocadas, primeras N (para el listado). */
    int n_uncovered;
    int uncovered[256];
} CovReporte;

void cov_calcular(const CovTracker *c, CovReporte *out);
void cov_dump(const CovTracker *c, FILE *out, const char *ruta_fuente,
              bool listar_uncovered);

#endif /* CORNAMUSA_COVERAGE_H */
