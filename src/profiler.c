#include "profiler.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <time.h>
#endif

void profiler_iniciar(Profiler *p) {
    memset(p, 0, sizeof(*p));
}

void profiler_destruir(Profiler *p) {
    for (int i = 0; i < p->n_entradas; i++) {
        free(p->entradas[i].nombre);
    }
    for (int i = 0; i < p->n_stack; i++) {
        free(p->stack[i].nombre);
    }
    memset(p, 0, sizeof(*p));
}

void profiler_activar(Profiler *p)   { p->activo = true; }

void profiler_desactivar(Profiler *p) {
    /* Flush: cierra cualquier frame que quede en el stack (el ultimo
       OP_RETORNAR del programa decrementa n_frames pero el dispatch
       loop ya no vuelve a iterar, asi que el sync no captura ese
       exit). Asi se contabiliza el top-level y cualquier frame
       pendiente por errores/yields. */
    while (p->n_stack > 0) {
        profiler_on_call_exit(p);
    }
    p->activo = false;
}

uint64_t profiler_tiempo_ns(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    /* now.QuadPart * 1e9 / freq.QuadPart, evitando overflow. */
    uint64_t ticks = (uint64_t)now.QuadPart;
    uint64_t hz = (uint64_t)freq.QuadPart;
    return (ticks / hz) * 1000000000ULL + ((ticks % hz) * 1000000000ULL) / hz;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
#endif
}

static ProfilerEntrada *buscar_o_crear(Profiler *p, const void *id, const char *nombre) {
    /* Búsqueda lineal: n_entradas suele estar en orden de decenas/centenas.
       Si crece, sustituir por hash table. */
    for (int i = 0; i < p->n_entradas; i++) {
        if (p->entradas[i].id == id) return &p->entradas[i];
    }
    if (p->n_entradas >= PROFILER_MAX_ENTRADAS) return NULL;
    ProfilerEntrada *e = &p->entradas[p->n_entradas++];
    e->id = id;
    e->nombre = nombre ? strdup(nombre) : strdup("<?>");
    e->llamadas = 0;
    e->total_ns = 0;
    e->self_ns = 0;
    return e;
}

void profiler_on_call_enter(Profiler *p, const void *id, const char *nombre) {
    if (!p->activo) return;
    if (p->n_stack >= PROFILER_STACK_MAX) { p->overflow++; return; }
    ProfilerStackEntry *s = &p->stack[p->n_stack++];
    s->id = id;
    s->nombre = nombre ? strdup(nombre) : strdup("<?>");
    s->t_inicio = profiler_tiempo_ns();
    s->t_hijos = 0;
}

void profiler_on_call_exit(Profiler *p) {
    if (!p->activo) return;
    if (p->n_stack == 0) return;
    uint64_t ahora = profiler_tiempo_ns();
    ProfilerStackEntry *s = &p->stack[--p->n_stack];
    uint64_t total = ahora - s->t_inicio;
    uint64_t self = total > s->t_hijos ? total - s->t_hijos : 0;

    ProfilerEntrada *e = buscar_o_crear(p, s->id, s->nombre);
    if (e) {
        e->llamadas++;
        e->total_ns += total;
        e->self_ns += self;
    }
    free(s->nombre);

    /* Propagar total al padre para su cálculo de self. */
    if (p->n_stack > 0) {
        p->stack[p->n_stack - 1].t_hijos += total;
    }
}

static int cmp_self_desc(const void *a, const void *b) {
    const ProfilerEntrada *ea = a;
    const ProfilerEntrada *eb = b;
    if (eb->self_ns > ea->self_ns) return 1;
    if (eb->self_ns < ea->self_ns) return -1;
    return 0;
}

static void formatear_ns(uint64_t ns, char *out, int cap) {
    if (ns >= 1000000000ULL) {
        snprintf(out, (size_t)cap, "%.3fs", (double)ns / 1e9);
    } else if (ns >= 1000000ULL) {
        snprintf(out, (size_t)cap, "%.3fms", (double)ns / 1e6);
    } else if (ns >= 1000ULL) {
        snprintf(out, (size_t)cap, "%.3fus", (double)ns / 1e3);
    } else {
        snprintf(out, (size_t)cap, "%lluns", (unsigned long long)ns);
    }
}

void profiler_dump(const Profiler *p, FILE *out, int top_n) {
    if (p->n_entradas == 0) {
        fprintf(out, "(profiler: no se registraron llamadas)\n");
        return;
    }

    /* Copiar para no mutar el original al ordenar. */
    ProfilerEntrada *copia = malloc(sizeof(ProfilerEntrada) * (size_t)p->n_entradas);
    if (!copia) return;
    memcpy(copia, p->entradas, sizeof(ProfilerEntrada) * (size_t)p->n_entradas);
    qsort(copia, (size_t)p->n_entradas, sizeof(ProfilerEntrada), cmp_self_desc);

    int filas = p->n_entradas;
    if (top_n > 0 && top_n < filas) filas = top_n;

    uint64_t total_global = 0;
    for (int i = 0; i < p->n_entradas; i++) total_global += copia[i].self_ns;

    fprintf(out, "\n");
    fprintf(out, "  llamadas       total        self    per-call  funcion\n");
    fprintf(out, "  --------     -------     -------    --------  -------\n");
    for (int i = 0; i < filas; i++) {
        char buf_total[32], buf_self[32], buf_per[32];
        formatear_ns(copia[i].total_ns, buf_total, sizeof(buf_total));
        formatear_ns(copia[i].self_ns, buf_self, sizeof(buf_self));
        uint64_t per = copia[i].llamadas ? copia[i].self_ns / copia[i].llamadas : 0;
        formatear_ns(per, buf_per, sizeof(buf_per));
        fprintf(out, "  %8llu  %10s  %10s  %10s  %s\n",
                (unsigned long long)copia[i].llamadas,
                buf_total, buf_self, buf_per, copia[i].nombre);
    }
    if (top_n > 0 && p->n_entradas > top_n) {
        fprintf(out, "  ... (%d funciones mas, no mostradas)\n",
                p->n_entradas - top_n);
    }
    if (p->overflow > 0) {
        fprintf(out, "  AVISO: %d llamadas perdidas (stack/tabla llena)\n", p->overflow);
    }

    char buf_total_g[32];
    formatear_ns(total_global, buf_total_g, sizeof(buf_total_g));
    fprintf(out, "\n  Total self acumulado: %s en %d funcion(es).\n",
            buf_total_g, p->n_entradas);

    free(copia);
}
