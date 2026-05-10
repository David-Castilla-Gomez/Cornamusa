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

/*
 * Tipos de bloque que el parser puede tener abiertos. Se usa para
 * validar que `fin <etiqueta>` cierra el bloque correcto (decisión B1).
 */
typedef enum {
    BLOQUE_SI,
    BLOQUE_MIENTRAS,
    BLOQUE_PARA,
    BLOQUE_FUNCION,
    BLOQUE_CLASE,
    BLOQUE_INTENTAR,
    BLOQUE_CON,         /* v1.13: `con expr [como nombre]: ... fin con` */
    BLOQUE_COINCIDIR,   /* v1.15: `coincidir expr: ... fin coincidir` */
} TipoBloque;

typedef struct {
    TipoBloque tipo;
    int linea_apertura;
} BloqueAbierto;

typedef struct {
    Lexer *lexer;
    Arena *arena;

    Token actual;            /* token actual (ya leído) */
    Token previo;            /* último token consumido */

    bool tuvo_error;         /* se reportó al menos un error */
    bool en_panico;          /* en modo recuperación */

    const char *fuente;      /* para mensajes con caret indicators */
    const char *archivo;     /* para mensajes con ubicación */

    /* Stack de bloques abiertos para validar `fin <etiqueta>`. */
    BloqueAbierto pila_bloques[64];
    int profundidad_bloques;
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

/*
 * Parsea una sola sentencia. Devuelve el AST resultante o NULL si
 * hubo un error. El parser consume todos los tokens de la sentencia
 * incluyendo (cuando aplica) `fin <etiqueta>`.
 */
Sent *parser_parsear_sentencia(Parser *p);

/*
 * Parsea sentencias hasta TT_FIN_ARCHIVO. Devuelve un array alocado
 * en la arena del parser, con `*n_out` siendo la cuenta. Si hay
 * errores, `tuvo_error` se activa pero el parseo intenta seguir.
 */
Sent **parser_parsear_programa(Parser *p, int *n_out);

#endif /* CORNAMUSA_PARSER_H */
