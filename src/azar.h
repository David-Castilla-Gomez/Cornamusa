/*
 * Cornamusa v1.26 — PRNG xoshiro256**.
 *
 * Estado global del proceso (single-threaded, simple). Sembrado
 * inicial con el reloj al primer uso. Los built-ins `azar_decimal`,
 * `azar_entero` y `azar_semilla` lo manipulan.
 *
 * xoshiro256** es un PRNG de calidad estadística probada (passes BigCrush)
 * con período 2^256 - 1 y muy poco estado (256 bits). Mejor que `rand()`
 * de libc para casi cualquier uso.
 *
 * Referencia: https://prng.di.unimi.it/
 */
#ifndef CORNAMUSA_AZAR_H
#define CORNAMUSA_AZAR_H

#include <stdint.h>

/* Re-siembra el estado a partir de `semilla`. Usa SplitMix64 para
   expandir los 64 bits a los 4 uint64_t del estado. */
void azar_sembrar(uint64_t semilla);

/* Asegura que el PRNG esté sembrado (con el tiempo si nunca lo fue). */
void azar_asegurar_sembrado(void);

/* Devuelve el siguiente uint64 pseudoaleatorio. */
uint64_t azar_siguiente_u64(void);

/* Devuelve un double uniforme en [0, 1). 53 bits de mantisa. */
double azar_decimal(void);

/* Devuelve un entero uniforme en [a, b], inclusive en ambos extremos.
   Asume a <= b. */
int64_t azar_entero_en(int64_t a, int64_t b);

#endif /* CORNAMUSA_AZAR_H */
