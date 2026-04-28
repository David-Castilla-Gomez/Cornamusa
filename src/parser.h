#ifndef CORNAMUSA_PARSER_H
#define CORNAMUSA_PARSER_H

#include <stdbool.h>

#include "arena.h"
#include "ast.h"
#include "lexer.h"

/*
 * Parser de Cornamusa.
 *
 * Construye un AST a partir del flujo de tokens del lexer. Estilo
 * Pratt (precedence climbing) para expresiones (decisión Fase 3
 * sesión 1), recursive descent para sentencias (sesión 2+).
 *
 * En esta versión (sesión 1) solo se parsean expresiones aisladas
 * vía `parser_parsear_expr`. La función `parser_parsear_programa`
 * (todo un archivo .cor) llega en sesión 2.
 *
 * Recuperación de errores: panic mode con sincronización en límites
 * de sentencia (sesión 2+). En sesión 1 el primer error de expresión
 * detiene el parseo y devuelve NULL.
 */

typedef struct {
    Lexer *lexer;
    Arena *arena;

    Token actual;            /* token actual (ya leído) */
    Token previo;            /* último token consumido */

    bool tuvo_error;         /* se reportó al menos un error */
    bool en_panico;          /* en modo recuperación */

    const char *fuente;      /* para mensajes con caret indicators */
    const char *archivo;     /* para mensajes con ubicación */
} Parser;

/*
 * Inicializa el parser. El cliente proporciona el lexer (ya iniciado),
 * la arena (ya iniciada) y el buffer fuente para errores.
 *
 * Tras la llamada, parser->actual contiene el primer token.
 */
void parser_iniciar(Parser *p, Lexer *l, Arena *a,
                    const char *fuente, const char *archivo);

/*
 * Parsea una sola expresión hasta el siguiente token no consumido.
 * Devuelve el AST resultante o NULL si hubo un error léxico/sintáctico
 * (en cuyo caso se reportó por stderr).
 *
 * El cliente puede consultar parser->tuvo_error para verificar éxito.
 */
Expr *parser_parsear_expr(Parser *p);

#endif /* CORNAMUSA_PARSER_H */
