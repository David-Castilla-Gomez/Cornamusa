#ifndef CORNAMUSA_PROFILER_H
#define CORNAMUSA_PROFILER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Profiler determinista (v1.71): mide tiempo por función registrando
 * cada entrada/salida de CallFrame. Cuando está inactivo, los hooks
 * son no-ops baratos (un branch + return). El usuario activa el
 * profiler con `cornamusa prof <script>`.
 *
 * Modelo:
 *   - `id` (puntero a FuncionBC o sentinel) identifica la función;
 *     diferentes closures que compartan plantilla agregan al mismo bucket.
 *   - `nombre` se duplica al primer hit (el FuncionBC podría liberarse
 *     antes del dump).
 *   - `total_ns`: tiempo desde el push hasta el pop (incluye hijos).
 *   - `self_ns`: tiempo del frame descontando los frames hijos. Es la
 *     métrica más útil para identificar hotspots.
 */

#define PROFILER_MAX_ENTRADAS 4096
#define PROFILER_STACK_MAX 256

typedef struct ProfilerEntrada {
    const void *id;
    char *nombre;
    uint64_t llamadas;
    uint64_t total_ns;
    uint64_t self_ns;
} ProfilerEntrada;

typedef struct ProfilerStackEntry {
    const void *id;
    char *nombre;        /* dueño (strdup); usado para registrar al exit */
    uint64_t t_inicio;
    uint64_t t_hijos;    /* tiempo acumulado de los hijos */
} ProfilerStackEntry;

typedef struct Profiler {
    bool activo;
    ProfilerEntrada entradas[PROFILER_MAX_ENTRADAS];
    int n_entradas;
    ProfilerStackEntry stack[PROFILER_STACK_MAX];
    int n_stack;
    int overflow;        /* llamadas perdidas por stack/tabla llena */
} Profiler;

void profiler_iniciar(Profiler *p);
void profiler_destruir(Profiler *p);
void profiler_activar(Profiler *p);
void profiler_desactivar(Profiler *p);

/* Reloj monotónico en nanosegundos. Resolución dependiente de plataforma. */
uint64_t profiler_tiempo_ns(void);

/* Hooks. Cuando p->activo es false, retornan inmediato. `nombre` no
   necesita ser estable — el profiler hace una copia. `id` debe ser
   estable para agregar correctamente (típicamente puntero a FuncionBC). */
void profiler_on_call_enter(Profiler *p, const void *id, const char *nombre);
void profiler_on_call_exit(Profiler *p);

/* Vuelca tabla ordenada por self_ns descendente. Top-N filas (0 = todas). */
void profiler_dump(const Profiler *p, FILE *out, int top_n);

#endif /* CORNAMUSA_PROFILER_H */
