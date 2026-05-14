/*
 * Cornamusa v1.29 — Cliente HTTP/1.1 plano (sin TLS).
 *
 * Subset acotado: solo método GET, URLs `http://host[:puerto]/path`,
 * sin redirecciones, sin keep-alive, sin chunked encoding (asume
 * Content-Length explícito). Cubre el caso 80%: APIs internas, healthchecks,
 * scraping simple en LAN.
 *
 * Para HTTPS, scraping moderno o sesiones autenticadas, usar el fetch
 * toolkit externo (separate dependency).
 *
 * Implementación cross-platform: WinSock2 en Windows, BSD sockets en POSIX.
 * Resolución DNS via `getaddrinfo`.
 */
#ifndef CORNAMUSA_RED_H
#define CORNAMUSA_RED_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Resultado de un HTTP request. Los buffers `cuerpo` y `cabeceras_raw`
 * son heap (free responsabilidad del caller).
 */
typedef struct {
    int  codigo;             /* HTTP status code (200, 404, etc.). 0 si fallo */
    char *cuerpo;            /* body, heap, libre con free() */
    int  cuerpo_len;
    char *cabeceras_raw;     /* todas las cabeceras como cadena multi-linea */
    int  cabeceras_len;
    char mensaje_error[256]; /* "" si OK */
} RedHttpResultado;

/*
 * GET sobre `url`. Si la URL no es http:// válida, retorna -1 y
 * llena mensaje_error.
 *
 * `cabeceras_extra` es opcional (NULL si no hay). Si se pasa, debe ser
 * una cadena con cabeceras en formato "Nombre: valor\r\nNombre2: valor2\r\n".
 *
 * Retorna 0 si la conexión + parsing tuvieron éxito (independientemente
 * del status code). Retorna -1 si error de red/DNS/parsing.
 *
 * El caller debe `free()` cuerpo y cabeceras_raw.
 */
int red_http_obtener_c(const char *url,
                         const char *cabeceras_extra,
                         int timeout_seg,
                         RedHttpResultado *out);

#endif /* CORNAMUSA_RED_H */
