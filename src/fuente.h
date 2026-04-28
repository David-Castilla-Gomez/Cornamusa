#ifndef CORNAMUSA_FUENTE_H
#define CORNAMUSA_FUENTE_H

#include <stddef.h>

/*
 * Utilidades de carga y normalización de código fuente.
 *
 * Decisión B4: el lexer asume que la fuente está en forma normalizada
 * NFC. Las funciones de este módulo se encargan de:
 *   - Leer el archivo o aceptar un buffer en memoria.
 *   - Validar que es UTF-8 (o ASCII, que es subconjunto).
 *   - Normalizar a NFC.
 * Devuelven un buffer terminado en '\0' alocado con malloc; el cliente
 * lo libera con free().
 */

/*
 * Resultado de la operación. fuente es NULL si error != FUENTE_OK.
 * mensaje_error contiene cadena estática (no liberar) cuando hay error.
 */
typedef enum {
    FUENTE_OK = 0,
    FUENTE_ERROR_IO,            /* archivo no existe / no se puede leer */
    FUENTE_ERROR_UTF8,          /* el contenido no es UTF-8 válido */
    FUENTE_ERROR_MEMORIA,       /* malloc falló */
} FuenteResultado;

typedef struct {
    char *fuente;               /* alocado con malloc, terminada en \0 */
    size_t longitud;            /* en bytes, sin contar el \0 final */
    FuenteResultado codigo;     /* FUENTE_OK si fuente != NULL */
    const char *mensaje_error;  /* cadena estática, o NULL si OK */
} FuenteCargada;

/*
 * Lee `ruta` del disco, valida UTF-8 y normaliza a NFC.
 * Devuelve un FuenteCargada con fuente alocado en heap si OK.
 */
FuenteCargada fuente_cargar_archivo(const char *ruta);

/*
 * Toma un buffer en memoria (cadena UTF-8 terminada en \0) y devuelve
 * una versión normalizada NFC en una nueva alocación.
 * No modifica el input.
 */
FuenteCargada fuente_normalizar(const char *texto);

/*
 * Libera la fuente cargada. Idempotente: tras llamar, fuente == NULL.
 */
void fuente_destruir(FuenteCargada *fc);

#endif /* CORNAMUSA_FUENTE_H */
