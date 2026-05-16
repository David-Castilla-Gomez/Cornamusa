#ifndef CORNAMUSA_DOCS_H
#define CORNAMUSA_DOCS_H

#include <stdbool.h>
#include <stddef.h>

#include "ast.h"

/*
 * Generador de documentacion Markdown a partir de fuente Cornamusa.
 *
 * Recibe el AST parseado + el texto fuente. Para cada SENT_FUNCION /
 * SENT_CLASE top-level extrae:
 *
 *   - Firma sintetizada desde el AST (nombre + parametros con
 *     defaults / `*args` / `**kwargs`).
 *   - Doc: bloque de comentarios `#` consecutivos inmediatamente
 *     anteriores a la declaracion (sin linea en blanco intermedia).
 *     Se descarta el `#` y un espacio opcional de cada linea.
 *
 * Para SENT_CLASE, recurre en su cuerpo emitiendo metodos como
 * subsecciones (un nivel mas de heading).
 *
 * El doc del modulo es el bloque de comentarios al inicio del
 * archivo (antes de la primera sentencia no-comentario).
 *
 * v1.51 (inicio): solo soporta comentarios `#` como fuente de docs.
 * Docstrings estilo Python (primer string literal del cuerpo) son
 * scope para v1.52.
 */

typedef struct {
    char *markdown;          /* malloc'd; vacio si no hay contenido. */
    size_t longitud;
    char *mensaje_error;     /* NULL en exito. */
} DocsResultado;

/*
 * Genera la documentacion en Markdown.
 *
 *   `fuente`         : texto fuente completo (necesario para extraer
 *                      comentarios — el AST no los preserva).
 *   `nombre_modulo`  : usado como titulo H1. Si NULL, no se emite H1.
 *   `sents`/`n`      : programa parseado.
 */
DocsResultado docs_generar(const char *fuente,
                            const char *nombre_modulo,
                            Sent **sents, int n);

void docs_resultado_destruir(DocsResultado *r);

#endif /* CORNAMUSA_DOCS_H */
