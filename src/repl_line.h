#ifndef CORNAMUSA_REPL_LINE_H
#define CORNAMUSA_REPL_LINE_H

#include <stdbool.h>

/*
 * v1.47: editor de línea con history para el REPL.
 *
 * Reemplaza `fgets` con edición interactiva:
 *   - Cursores izquierda/derecha, Home/End, Backspace, Delete.
 *   - Up/Down recorre el historial.
 *   - Ctrl-A / Ctrl-E como atajos a Home/End.
 *   - Ctrl-C cancela la línea actual sin salir.
 *   - Ctrl-D (Linux) / Ctrl-Z+Enter (Windows) con buffer vacío termina.
 *
 * Cross-platform: termios en POSIX, _getch+SetConsoleMode en Windows.
 * Si el stdin no es un terminal (script piped, archivo) cae a fgets.
 *
 * El historial es persistente en memoria durante la sesión. Persistencia
 * en archivo `.cornamusa_historial` (~/.cornamusa_historial en POSIX,
 * %USERPROFILE%\.cornamusa_historial en Windows) gestionada por
 * `repl_historial_cargar` / `repl_historial_guardar`.
 */

typedef struct ReplHistorial ReplHistorial;

/* Construye un historial vacío. Liberar con repl_historial_liberar. */
ReplHistorial *repl_historial_nuevo(void);
void repl_historial_liberar(ReplHistorial *h);

/* Agrega `linea` al final. No agrega si es vacía o idéntica a la
   última entrada. Toma copia. */
void repl_historial_agregar(ReplHistorial *h, const char *linea);

/* Carga/guarda el historial desde/hacia el archivo de usuario. */
void repl_historial_cargar(ReplHistorial *h);
void repl_historial_guardar(const ReplHistorial *h);

/*
 * Lee una línea con prompt, edición interactiva y navegación por
 * historial. La línea retornada NO incluye el `\n` final. El caller
 * libera con free().
 *
 * Devuelve NULL en EOF (Ctrl-D POSIX / Ctrl-Z Windows con buffer vacío).
 *
 * Si `historial` es NULL, no se ofrece navegación por historial.
 * En tal caso la función sigue ofreciendo edición de línea.
 */
char *repl_leer_linea(const char *prompt, ReplHistorial *historial);

#endif /* CORNAMUSA_REPL_LINE_H */
