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
 * En sesión 1 produce el formato mínimo (sin caret); en sesión 5 se
 * añadirán los indicadores ^^^ con la línea de fuente.
 */
void error_imprimir(const Error *e, FILE *salida);

#endif /* CORNAMUSA_ERRORES_H */
