/*
 * Cornamusa v1.26 — PRNG xoshiro256** (implementación).
 *
 * Estado de proceso. La VM no lo necesita en su struct: el estado vive
 * aquí y persiste entre llamadas. Hilos: el lenguaje no es
 * multihilo todavía, así que static está bien.
 *
 * Algoritmo (Blackman & Vigna, 2018, dominio público según su nota):
 *   uint64_t result = rotl(state[1] * 5, 7) * 9;
 *   uint64_t t = state[1] << 17;
 *   state[2] ^= state[0];
 *   state[3] ^= state[1];
 *   state[1] ^= state[2];
 *   state[0] ^= state[3];
 *   state[2] ^= t;
 *   state[3] = rotl(state[3], 45);
 *
 * SplitMix64 expande la semilla a los 4 lanes.
 */

#include "azar.h"

#include <time.h>

static uint64_t s[4];
static int sembrado = 0;

static inline uint64_t rotl64(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

/* SplitMix64: PRNG simple para sembrar el estado de xoshiro. */
static uint64_t splitmix64(uint64_t *state) {
    *state += 0x9E3779B97F4A7C15ULL;
    uint64_t z = *state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

void azar_sembrar(uint64_t semilla) {
    uint64_t sm = semilla;
    s[0] = splitmix64(&sm);
    s[1] = splitmix64(&sm);
    s[2] = splitmix64(&sm);
    s[3] = splitmix64(&sm);
    /* Asegurar que el estado no sea todo-cero (improbable pero
       posible si semilla=0 cae justo, aunque SplitMix lo evita). */
    if ((s[0] | s[1] | s[2] | s[3]) == 0) s[0] = 1;
    sembrado = 1;
}

void azar_asegurar_sembrado(void) {
    if (sembrado) return;
    /* Combinar segundos + ns para más entropía inicial. */
    uint64_t semilla = (uint64_t)time(NULL);
#ifdef CLOCK_MONOTONIC
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        semilla ^= (uint64_t)ts.tv_nsec << 32;
    }
#else
    semilla ^= (uint64_t)(uintptr_t)&sembrado;  /* ASLR como entropía extra */
#endif
    azar_sembrar(semilla);
}

uint64_t azar_siguiente_u64(void) {
    azar_asegurar_sembrado();
    const uint64_t result = rotl64(s[1] * 5, 7) * 9;
    const uint64_t t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl64(s[3], 45);
    return result;
}

double azar_decimal(void) {
    /* 53 bits altos como mantisa de un double en [0, 1). */
    return (double)(azar_siguiente_u64() >> 11) * (1.0 / 9007199254740992.0);
}

int64_t azar_entero_en(int64_t a, int64_t b) {
    if (a >= b) return a;
    uint64_t rango = (uint64_t)(b - a) + 1;
    /* Rechazo: descartar valores en la zona alta de bias. `lim` es el
       múltiplo más grande de `rango` que cabe en uint64. Si r < lim,
       entonces r % rango es perfectamente uniforme. */
    uint64_t lim = (UINT64_MAX / rango) * rango;
    uint64_t r;
    do {
        r = azar_siguiente_u64();
    } while (r >= lim);
    return a + (int64_t)(r % rango);
}
