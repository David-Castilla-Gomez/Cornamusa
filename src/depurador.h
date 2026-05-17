#ifndef CORNAMUSA_DEPURADOR_H
#define CORNAMUSA_DEPURADOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Debugger interactivo (v1.76). Reusa la arquitectura de profiler/cov:
 * hook único al inicio del dispatch loop, cuando inactivo es un branch.
 *
 * Punto de pausa: cambio de línea (no por opcode). Cuando se pausa,
 * imprime la línea actual con un caret y abre un prompt `(dep)`.
 *
 * Comandos:
 *   c | continuar       continuar hasta proximo breakpoint
 *   s | paso            step into (pausa en proxima linea, cualquier frame)
 *   n | siguiente       step over (pausa solo en mismo frame o superior)
 *   r | retornar        step out (continua hasta que termine frame actual)
 *   b N | break N       set breakpoint en linea N
 *   bd N | borrar N     borrar breakpoint en linea N
 *   bs | breaks         listar breakpoints
 *   l | lista [N]       mostrar codigo alrededor de la linea actual
 *   p NOMBRE | imprimir NOMBRE   muestra variable global por nombre
 *   pila | stack        backtrace de frames
 *   q | salir           aborta el programa
 *   ? | ayuda           help
 */

typedef enum {
    DEP_CORRIENDO,         /* sigue hasta proximo breakpoint */
    DEP_PASO,              /* pausa en proxima linea, cualquier frame */
    DEP_SIGUIENTE,         /* pausa en proxima linea de mismo frame o ancestral */
    DEP_RETORNAR,          /* pausa cuando volvamos a frame < frame_objetivo */
} ModoDepurador;

#define DEPURADOR_MAX_BREAKPOINTS 64

typedef struct Depurador {
    bool activo;
    ModoDepurador modo;
    int frame_objetivo;             /* para SIGUIENTE / RETORNAR */
    int breakpoints[DEPURADOR_MAX_BREAKPOINTS];
    int n_breakpoints;
    int ultima_linea;
    int ultimo_n_frames;

    /* Fuente para listing. Copia propia (heap), liberada por destruir. */
    char *fuente;
    int fuente_len;
    /* Indices de inicio de cada linea en `fuente`. lineas_offset[i] es
     * el offset del inicio de la linea i+1 (1-indexed). */
    int *lineas_offset;
    int n_lineas;

    /* Ruta solo para mostrar al usuario. No propietario. */
    const char *ruta;
} Depurador;

void depurador_iniciar(Depurador *d);
void depurador_destruir(Depurador *d);

/* Activa el debugger. Hace strdup de `fuente` y precomputa offsets de
 * lineas para listing rapido. `ruta` se almacena por referencia. */
void depurador_activar(Depurador *d, const char *fuente, const char *ruta);
void depurador_desactivar(Depurador *d);

#endif /* CORNAMUSA_DEPURADOR_H */
