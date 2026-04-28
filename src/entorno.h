#ifndef CORNAMUSA_ENTORNO_H
#define CORNAMUSA_ENTORNO_H

#include <stdbool.h>
#include <stddef.h>

#include "valor.h"

/*
 * Entorno (scope chain): tabla hash de nombres → Valor con
 * encadenamiento al entorno padre.
 *
 * Cada bloque de código (cuerpo de función, módulo, REPL) tiene un
 * Entorno. Para resolver una variable se busca primero en el actual,
 * y si no se encuentra se sube al padre, y así hasta llegar al global
 * o fallar.
 *
 * En Fase 4 sin closures (decisión B2), los entornos forman una pila
 * lineal: módulo global → función llamada. En Fase 6 con bytecode VM
 * se reemplazará por un layout de slots numerados, pero para el
 * tree-walking este modelo simple basta.
 *
 * Implementación:
 *   - Tabla hash con probing lineal y factor de carga 0.75 (estilo
 *     clox cap. 20).
 *   - Hash FNV-1a sobre el texto del nombre.
 *   - El entorno es DUEÑO de los Valores almacenados — los destruye
 *     al destruir el entorno o al sobrescribir una variable.
 *
 * NO HAY GC en Fase 4: la liberación de Valores es eager. Cuando una
 * función retorna, su entorno se destruye y todos los Valores locales
 * se liberan (mp_int, cadenas con dueño, etc.).
 */

typedef struct EntradaEntorno EntradaEntorno;

struct EntradaEntorno {
    const char *clave;       /* puntero a buffer fuente; NO se libera */
    int longitud_clave;
    Valor valor;
    bool ocupada;            /* false para slots vacíos en el array */
};

typedef struct Entorno Entorno;

struct Entorno {
    EntradaEntorno *entradas;
    size_t capacidad;
    size_t cuenta;           /* entradas ocupadas */
    Entorno *padre;          /* NULL para el entorno global */
};

/*
 * Inicializa un entorno vacío con capacidad inicial pequeña. `padre`
 * es el entorno enclosing (puede ser NULL para el global).
 *
 * Tras la llamada el cliente puede hacer `entorno_definir`,
 * `entorno_obtener`, `entorno_asignar`. Liberar con `entorno_destruir`.
 */
void entorno_iniciar(Entorno *e, Entorno *padre);

/*
 * Libera todos los Valores del entorno y la tabla hash. NO libera
 * el entorno padre. Idempotente.
 */
void entorno_destruir(Entorno *e);

/*
 * Define una variable nueva en este entorno. Si la clave ya existía,
 * libera el Valor anterior y guarda el nuevo. Toma posesión del Valor
 * proporcionado (lo destruirá cuando corresponda).
 *
 * `clave` no se copia — debe vivir mientras el entorno la use (típi-
 * camente apunta al buffer fuente).
 *
 * Devuelve true si OK; false si OOM.
 */
bool entorno_definir(Entorno *e, const char *clave, int longitud, Valor v);

/*
 * Busca la variable en este entorno y, si no la encuentra, en los
 * padres. Devuelve true y rellena `*out` con un CLON del Valor (ya
 * que el entorno mantiene la propiedad). Devuelve false si la
 * variable no existe en ninguna parte de la cadena.
 *
 * El cliente es responsable de destruir el clon cuando lo deje de usar.
 */
bool entorno_obtener(Entorno *e, const char *clave, int longitud, Valor *out);

/*
 * Asigna a una variable existente. Busca en este entorno y los padres,
 * y modifica donde la encuentre. Devuelve true si la variable existía;
 * false si no (en cuyo caso el cliente debe reportar ErrorDeNombre).
 *
 * Toma posesión del Valor.
 */
bool entorno_asignar(Entorno *e, const char *clave, int longitud, Valor v);

/*
 * Comprueba si la variable existe (sin clonar el valor). Útil para
 * `x en frecuencias` y validaciones rápidas.
 */
bool entorno_existe(Entorno *e, const char *clave, int longitud);

#endif /* CORNAMUSA_ENTORNO_H */
