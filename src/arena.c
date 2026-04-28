#include "arena.h"

#include <stdlib.h>
#include <string.h>

struct BloqueArena {
    BloqueArena *siguiente;
    size_t capacidad;
    size_t usado;
    char datos[];           /* flexible array member */
};

/* Alineación: 8 bytes (suficiente para cualquier puntero o double en
   plataformas 64-bit). */
#define ALINEACION 8u
#define ALINEAR(n)   (((n) + (ALINEACION - 1)) & ~(size_t)(ALINEACION - 1))

static BloqueArena *crear_bloque(size_t capacidad) {
    BloqueArena *b = (BloqueArena *)malloc(sizeof(BloqueArena) + capacidad);
    if (b == NULL) return NULL;
    b->siguiente = NULL;
    b->capacidad = capacidad;
    b->usado = 0;
    return b;
}

void arena_iniciar(Arena *a, size_t tamano_bloque) {
    if (tamano_bloque < 256) tamano_bloque = 256;
    a->tamano_bloque = tamano_bloque;
    a->primero = crear_bloque(tamano_bloque);
    a->actual = a->primero;
    a->total_alocado = 0;
}

void *arena_alocar(Arena *a, size_t bytes) {
    if (bytes == 0) bytes = 1; /* devolver puntero único, no NULL */
    size_t alineado = ALINEAR(bytes);

    BloqueArena *b = a->actual;
    if (b == NULL || b->usado + alineado > b->capacidad) {
        /* Necesitamos un bloque nuevo. Su tamaño es al menos
           tamano_bloque y suficiente para esta petición. */
        size_t cap = a->tamano_bloque;
        if (alineado > cap) cap = alineado;
        BloqueArena *nuevo = crear_bloque(cap);
        if (nuevo == NULL) return NULL;

        if (b == NULL) {
            a->primero = nuevo;
        } else {
            b->siguiente = nuevo;
        }
        a->actual = nuevo;
        b = nuevo;
    }

    void *resultado = b->datos + b->usado;
    b->usado += alineado;
    a->total_alocado += alineado;
    return resultado;
}

void *arena_alocar_cero(Arena *a, size_t bytes) {
    void *p = arena_alocar(a, bytes);
    if (p != NULL) memset(p, 0, bytes);
    return p;
}

char *arena_duplicar_cadena(Arena *a, const char *texto) {
    if (texto == NULL) return NULL;
    size_t len = strlen(texto);
    char *copia = (char *)arena_alocar(a, len + 1);
    if (copia == NULL) return NULL;
    memcpy(copia, texto, len + 1);
    return copia;
}

void arena_destruir(Arena *a) {
    BloqueArena *b = a->primero;
    while (b != NULL) {
        BloqueArena *siguiente = b->siguiente;
        free(b);
        b = siguiente;
    }
    a->primero = NULL;
    a->actual = NULL;
    a->total_alocado = 0;
}
