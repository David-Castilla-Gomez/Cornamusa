#include "entorno.h"

#include <stdlib.h>
#include <string.h>

/* Capacidad inicial: pequeña pero distinta de 1 para evitar muchos
   redimensionamientos en programas con varias variables. */
#define CAPACIDAD_INICIAL 8

/* Factor de carga máximo antes de redimensionar (0.75). */
#define MAX_CARGA 0.75

/* ──────────────────────────────────────────────────────────────────
 * Hash FNV-1a 32-bit
 *
 * Hash simple y rápido, suficiente para identificadores cortos.
 * Estilo clox cap. 20: mismas propiedades de distribución uniforme
 * que CityHash o xxHash para casos pequeños, sin dependencias.
 * ────────────────────────────────────────────────────────────────── */

static uint32_t hash_fnv1a(const char *clave, int longitud) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < longitud; i++) {
        hash ^= (uint8_t)clave[i];
        hash *= 16777619u;
    }
    return hash;
}

/* Compara dos claves byte a byte. */
static bool clave_iguales(const char *a, int la, const char *b, int lb) {
    if (la != lb) return false;
    return memcmp(a, b, (size_t)la) == 0;
}

/*
 * Busca el slot apropiado para `clave` en el array de entradas.
 * Devuelve puntero al slot. Si la clave existe, el slot es el que la
 * contiene. Si no existe, es el primer slot vacío en la cadena de
 * probing donde se podría insertar.
 *
 * Asume que la tabla NO está completamente llena (al menos un slot
 * vacío). Esto se garantiza por el factor de carga.
 */
static EntradaEntorno *buscar_slot(EntradaEntorno *entradas, size_t cap,
                                    const char *clave, int longitud) {
    uint32_t indice = hash_fnv1a(clave, longitud) & (uint32_t)(cap - 1);
    for (;;) {
        EntradaEntorno *e = &entradas[indice];
        if (!e->ocupada) {
            return e;
        }
        if (clave_iguales(e->clave, e->longitud_clave, clave, longitud)) {
            return e;
        }
        indice = (indice + 1) & (uint32_t)(cap - 1);
    }
}

/*
 * Crea una nueva tabla con `nueva_cap` slots y rehashea todas las
 * entradas existentes. Libera la tabla antigua (sin destruir Valores
 * — solo se mueven al nuevo array).
 */
static bool redimensionar(Entorno *e, size_t nueva_cap) {
    EntradaEntorno *nueva = (EntradaEntorno *)calloc(nueva_cap,
        sizeof(EntradaEntorno));
    if (nueva == NULL) return false;

    /* Re-insertar todas las entradas ocupadas. */
    for (size_t i = 0; i < e->capacidad; i++) {
        EntradaEntorno *src = &e->entradas[i];
        if (!src->ocupada) continue;
        EntradaEntorno *dst = buscar_slot(nueva, nueva_cap,
                                           src->clave, src->longitud_clave);
        *dst = *src;
    }

    free(e->entradas);
    e->entradas = nueva;
    e->capacidad = nueva_cap;
    return true;
}

/* ──────────────────────────────────────────────────────────────────
 * API pública
 * ────────────────────────────────────────────────────────────────── */

void entorno_iniciar(Entorno *e, Entorno *padre) {
    e->entradas = (EntradaEntorno *)calloc(CAPACIDAD_INICIAL,
        sizeof(EntradaEntorno));
    e->capacidad = e->entradas ? CAPACIDAD_INICIAL : 0;
    e->cuenta = 0;
    e->padre = padre;
}

void entorno_destruir(Entorno *e) {
    if (e == NULL || e->entradas == NULL) return;
    for (size_t i = 0; i < e->capacidad; i++) {
        if (e->entradas[i].ocupada) {
            valor_destruir(&e->entradas[i].valor);
        }
    }
    free(e->entradas);
    e->entradas = NULL;
    e->capacidad = 0;
    e->cuenta = 0;
}

bool entorno_definir(Entorno *e, const char *clave, int longitud, Valor v) {
    if (e->capacidad == 0) return false;

    /* Redimensionar si nos acercamos al factor de carga máximo. */
    if ((double)(e->cuenta + 1) > (double)e->capacidad * MAX_CARGA) {
        if (!redimensionar(e, e->capacidad * 2)) {
            valor_destruir(&v);
            return false;
        }
    }

    EntradaEntorno *slot = buscar_slot(e->entradas, e->capacidad,
                                        clave, longitud);
    if (slot->ocupada) {
        /* Sobrescribir: liberar valor anterior. */
        valor_destruir(&slot->valor);
    } else {
        slot->ocupada = true;
        e->cuenta++;
    }
    slot->clave = clave;
    slot->longitud_clave = longitud;
    slot->valor = v;
    return true;
}

bool entorno_obtener(Entorno *e, const char *clave, int longitud, Valor *out) {
    if (e == NULL) return false;
    if (e->capacidad > 0) {
        EntradaEntorno *slot = buscar_slot(e->entradas, e->capacidad,
                                            clave, longitud);
        if (slot->ocupada) {
            *out = valor_clonar(&slot->valor);
            return true;
        }
    }
    /* No está aquí — buscar en el padre. */
    return entorno_obtener(e->padre, clave, longitud, out);
}

bool entorno_asignar(Entorno *e, const char *clave, int longitud, Valor v) {
    if (e == NULL) {
        valor_destruir(&v);
        return false;
    }
    if (e->capacidad > 0) {
        EntradaEntorno *slot = buscar_slot(e->entradas, e->capacidad,
                                            clave, longitud);
        if (slot->ocupada) {
            valor_destruir(&slot->valor);
            slot->valor = v;
            return true;
        }
    }
    return entorno_asignar(e->padre, clave, longitud, v);
}

bool entorno_existe(Entorno *e, const char *clave, int longitud) {
    if (e == NULL || e->capacidad == 0) return false;
    EntradaEntorno *slot = buscar_slot(e->entradas, e->capacidad,
                                        clave, longitud);
    if (slot->ocupada) return true;
    return entorno_existe(e->padre, clave, longitud);
}
