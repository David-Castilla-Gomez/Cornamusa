#ifndef CORNAMUSA_FORMATEADOR_H
#define CORNAMUSA_FORMATEADOR_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Formateador de fuente Cornamusa (v1.48 - inicio de Fase 5 tooling).
 *
 * Reglas conservadoras, garantizadas idempotentes:
 *   - Reindentacion a 4 espacios derivada de profundidad de bloques.
 *     Bloques se abren con linea acabada en ':' y se cierran con
 *     'fin <etiqueta>'. Mid-block markers (sino, cuando, atrapar,
 *     finalmente) se dedentan a la profundidad del abridor.
 *   - Strip de trailing whitespace en cada linea.
 *   - Colapso de >=2 lineas en blanco a 1.
 *   - Garantiza exactamente un '\n' al final si el archivo no esta vacio.
 *   - Lineas dentro de () [] {} o triple-quoted strings se tratan como
 *     continuacion: leading whitespace se preserva tal cual. Solo se
 *     normaliza trailing whitespace y line endings.
 *
 * Lo que el formateador NO hace (queda para v1.49+):
 *   - Tocar espaciado de operadores.
 *   - Reformatear expresiones largas.
 *   - Anadir o quitar parentesis.
 *   - Insertar o eliminar comentarios.
 */

typedef struct {
    /* Fuente formateada. malloc'd; llamador libera con
     * formato_resultado_destruir. NULL si hubo error. */
    char *fuente;
    size_t longitud;
    /* true si la salida difiere del original. */
    bool cambiada;
    /* NULL en exito; mensaje malloc'd si hubo error. */
    char *mensaje_error;
} FormatoResultado;

FormatoResultado formateador_formatear(const char *fuente);

void formato_resultado_destruir(FormatoResultado *r);

#endif /* CORNAMUSA_FORMATEADOR_H */
