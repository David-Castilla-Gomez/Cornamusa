/*
 * Cornamusa v1.27 — Lanzar procesos externos.
 *
 * Implementación cross-platform: Windows (CreateProcess + pipes anónimos)
 * y POSIX (fork + execvp + pipes). Captura stdout, stderr y exit code.
 *
 * Esta es la primitiva pelada. El módulo `stdlib/proceso.cor` la
 * envuelve con `proceso.ejecutar(cmd, *args)` y `proceso.capturar(...)`.
 *
 * Limitaciones de v1.27:
 *   - Sin entrada interactiva (stdin).
 *   - Sin timeout (bloquea hasta que el proceso termine).
 *   - Sin variables de entorno extra (hereda del padre).
 *   - Sin cambio de directorio de trabajo.
 *   - Output capturado en heap con límite razonable (10 MB).
 */
#ifndef CORNAMUSA_PROCESO_H
#define CORNAMUSA_PROCESO_H

#include <stddef.h>

/*
 * Resultado de ejecutar un proceso. Los buffers `stdout_buf` y
 * `stderr_buf` son heap (free responsabilidad del caller). Si la
 * llamada falla antes de fork/CreateProcess, exit_codigo es -1 y
 * `mensaje_error` describe el error.
 */
typedef struct {
    char *stdout_buf;
    int   stdout_len;
    char *stderr_buf;
    int   stderr_len;
    int   exit_codigo;          /* -1 si fallo antes de ejecutar */
    char  mensaje_error[256];   /* "" si OK */
} ProcesoResultado;

/*
 * Ejecuta `programa` con `argv` (terminado en NULL — convención execvp).
 * Espera a que termine. Captura stdout y stderr en buffers nuevos.
 *
 * Retorna 0 si el proceso se lanzó (independientemente de su exit code).
 * Retorna -1 si falló la creación; mensaje_error en out describe el motivo.
 *
 * El caller debe `free()` stdout_buf y stderr_buf cuando termine de
 * usarlos.
 */
int proceso_ejecutar_c(const char *programa,
                        const char *const *argv,
                        ProcesoResultado *out);

#endif /* CORNAMUSA_PROCESO_H */
