#ifndef CORNAMUSA_ERRORES_H
#define CORNAMUSA_ERRORES_H

#include <stdbool.h>
#include <stdio.h>

/*
 * Estructura de error siguiendo el estándar de MENSAJES.md.
 *
 * En esta versión (v0.2.0 / sesión 1 de Fase 2) implementamos solo el
 * formato mínimo: categoría + ubicación + mensaje + sugerencia opcional.
 * Los caret indicators (líneas 5-7 de la anatomía en MENSAJES.md §2)
 * llegan en sesión 5 cuando el lexer tenga acceso al texto de la línea.
 */
typedef struct {
    const char *categoria;     /* ej: "ErrorDeSintaxis"; cadena estática, no se libera */
    const char *archivo;       /* ruta o NULL para REPL; cadena no poseída */
    int linea;                 /* 1-indexed */
    int columna_inicio;        /* 1-indexed, en bytes desde inicio de línea */
    int columna_fin;           /* 1-indexed exclusivo, en bytes; 0 si no aplica */
    char *mensaje;             /* alocado con malloc o NULL; se libera en error_destruir */
    char *sugerencia;          /* alocado con malloc o NULL; se libera en error_destruir */
} Error;

/*
 * Inicializa un Error con la categoría dada (cadena estática) y campos
 * en cero/NULL. Tras esto se rellenan los campos directamente.
 */
void error_iniciar(Error *e, const char *categoria);

/*
 * Libera mensaje y sugerencia. Deja el resto intacto. Idempotente.
 */
void error_destruir(Error *e);

/*
 * Establece el mensaje principal duplicando la cadena (con malloc).
 * Reemplaza cualquier mensaje previo. Si texto es NULL, deja mensaje en NULL.
 */
void error_set_mensaje(Error *e, const char *texto);

/*
 * Establece la sugerencia duplicando la cadena. Reemplaza cualquier
 * sugerencia previa. Si texto es NULL, deja sugerencia en NULL.
 */
void error_set_sugerencia(Error *e, const char *texto);

/*
 * Imprime el error en `salida` siguiendo el formato de MENSAJES.md §2.
 *
 * Si `fuente` es no-NULL, el formato extiende con la línea de código
 * y caret indicators (`^^^^`) bajo el span del error:
 *
 *   ErrorDeSintaxis en programa.cor:5:14
 *       imprimir(saludaar(nombre))
 *                ^^^^^^^^
 *   'saludaar' no está definido.
 *   sugerencia: ¿quisiste decir 'saludar'?
 *
 * Si `fuente` es NULL produce el formato mínimo sin línea de código.
 *
 * `longitud_span` es el número de bytes a subrayar a partir de
 * `columna_inicio`; usar 1 si no hay span explícito.
 */
void error_imprimir(const Error *e, const char *fuente,
                    int longitud_span, FILE *salida);

/*
 * Forward declaration del Token del lexer para evitar dependencia
 * circular. La definición real está en lexer.h.
 */
struct Token;

/*
 * Atajo: imprime un Token TT_ERROR como ErrorDeSintaxis siguiendo el
 * formato de MENSAJES.md, usando `fuente` para extraer la línea de
 * código y el span del propio token para los carets.
 *
 * Marca el caller como propietario de `archivo` (no se libera).
 */
void error_imprimir_token(const struct Token *token,
                          const char *fuente,
                          const char *archivo,
                          FILE *salida);

#endif /* CORNAMUSA_ERRORES_H */
