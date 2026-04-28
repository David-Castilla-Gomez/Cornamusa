#ifndef CORNAMUSA_ARENA_H
#define CORNAMUSA_ARENA_H

#include <stddef.h>

/*
 * Arena allocator (también conocido como bump/region allocator).
 *
 * Asigna desde un buffer contiguo grande y libera todo de golpe al
 * destruir la arena. Patrón ideal para ASTs y otras estructuras
 * acíclicas con vida útil única: durante la fase de parseo
 * acumulamos nodos, y al terminar el AST entero (o la fase) se
 * libera con una sola llamada.
 *
 * Internamente usa una lista enlazada de bloques para crecer cuando
 * la capacidad inicial se queda corta.
 *
 * No soporta `free` individual de objetos. Si necesitas eso, usa
 * malloc/free directamente.
 */

typedef struct BloqueArena BloqueArena;

typedef struct {
    BloqueArena *primero;       /* head de la lista enlazada */
    BloqueArena *actual;        /* bloque actual del que se alocan bytes */
    size_t tamano_bloque;       /* tamaño por defecto de cada bloque */
    size_t total_alocado;       /* bytes vivos en la arena (estadística) */
} Arena;

/*
 * Inicializa la arena con un primer bloque de `tamano_bloque` bytes.
 * Bloques posteriores tendrán al menos ese tamaño (más si una
 * petición individual lo excede).
 *
 * Tamaño recomendado: 4096 (una página) o múltiplos.
 */
void arena_iniciar(Arena *a, size_t tamano_bloque);

/*
 * Aloca `bytes` bytes alineados a `void*` (típicamente 8 en 64-bit).
 * Devuelve un puntero al bloque o NULL si malloc falló (raro).
 *
 * Si la petición no cabe en el bloque actual, alocamos un nuevo
 * bloque grande suficiente. Bloques antiguos no se reciclan; siguen
 * conteniendo punteros válidos.
 */
void *arena_alocar(Arena *a, size_t bytes);

/*
 * Aloca y devuelve cero la memoria. Equivalente a calloc.
 */
void *arena_alocar_cero(Arena *a, size_t bytes);

/*
 * Duplica una cadena en la arena. Útil para copiar nombres del lexer
 * cuando queremos que el AST sea independiente del buffer fuente.
 * Para Cornamusa, sin embargo, preferimos guardar punteros al fuente
 * sin copiar — esta función es por si en algún momento se necesita.
 */
char *arena_duplicar_cadena(Arena *a, const char *texto);

/*
 * Libera todos los bloques. Tras esto la arena queda vacía y se
 * puede volver a iniciar con `arena_iniciar` si se desea.
 *
 * No vacía las pointers que el cliente tenía a memoria de la arena;
 * usarlos tras destruir es comportamiento indefinido.
 */
void arena_destruir(Arena *a);

#endif /* CORNAMUSA_ARENA_H */
