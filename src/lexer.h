#ifndef CORNAMUSA_LEXER_H
#define CORNAMUSA_LEXER_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Lexer de Cornamusa.
 *
 * Convierte una cadena UTF-8 (decisión B4) en una secuencia de tokens.
 * El cliente llama lexer_iniciar() una vez y luego lexer_siguiente()
 * repetidamente hasta recibir TT_FIN_ARCHIVO.
 *
 * Implementación distribuida en sesiones (Fase 2):
 *   - Sesión 1 (v0.2.0-alfa): símbolos, operadores, comentarios, whitespace.
 *   - Sesión 2: literales numéricos y cadenas básicas.
 *   - Sesión 3: identificadores Unicode + NFC + tabla de keywords.
 *   - Sesión 4: f-strings y triple-quoted strings.
 *   - Sesión 5: mensajes de error pulidos siguiendo MENSAJES.md.
 *
 * Esta versión (sesión 1) solo emite tokens del primer grupo.
 */

/*
 * Tipos de token. La enumeración es estable desde sesión 1 aunque solo
 * algunos tipos se emitan todavía. Los tipos no implementados en sesión 1
 * están marcados con (sN) indicando la sesión que los activará.
 */
typedef enum {
    /* ─── Símbolos individuales ─── */
    TT_PARENT_IZQ,         /* (  */
    TT_PARENT_DER,         /* )  */
    TT_LLAVE_IZQ,          /* {  */
    TT_LLAVE_DER,          /* }  */
    TT_CORCH_IZQ,          /* [  */
    TT_CORCH_DER,          /* ]  */
    TT_COMA,               /* ,  */
    TT_PUNTO,              /* .  */
    TT_DOS_PUNTOS,         /* :  */
    TT_PUNTO_COMA,         /* ;  */
    TT_FLECHA,             /* -> */
    TT_AT,                 /* @  */

    /* ─── Operadores aritméticos ─── */
    TT_MAS,                /* +  */
    TT_MENOS,              /* -  */
    TT_ASTERISCO,          /* *  */
    TT_BARRA,              /* /  */
    TT_DOBLE_BARRA,        /* // */
    TT_PORCENTAJE,         /* %  */
    TT_DOBLE_ASTERISCO,    /* ** */

    /* ─── Operadores de comparación ─── */
    TT_IGUAL,              /* == */
    TT_DISTINTO,           /* != */
    TT_MENOR,              /* <  */
    TT_MENOR_IGUAL,        /* <= */
    TT_MAYOR,              /* >  */
    TT_MAYOR_IGUAL,        /* >= */

    /* ─── Asignación simple y compuesta ─── */
    TT_ASIGNAR,            /* =  */
    TT_ASIGNAR_MAS,        /* += */
    TT_ASIGNAR_MENOS,      /* -= */
    TT_ASIGNAR_ASTERISCO,  /* *= */
    TT_ASIGNAR_BARRA,      /* /= */
    TT_ASIGNAR_DOBLE_BARRA,/* //= */
    TT_ASIGNAR_PORCENTAJE, /* %= */
    TT_ASIGNAR_DOBLE_ASTER,/* **= */

    /* ─── Operadores bitwise ─── */
    TT_AMPERSAND,          /* &  */
    TT_BARRA_VERT,         /* |  */
    TT_CIRCUNFLEJO,        /* ^  */
    TT_TILDE_BIT,          /* ~  (negación bit a bit, no acento) */
    TT_DESPL_IZQ,          /* << */
    TT_DESPL_DER,          /* >> */

    /* ─── Literales (s2-s4) ─── */
    TT_ENTERO,             /* 42, 1_000_000, 0xff   (s2) */
    TT_DECIMAL,            /* 3.14, 1.5e10           (s2) */
    TT_CADENA,             /* "hola", 'mundo'        (s2) */
    TT_F_CADENA,           /* f"hola {nombre}"       (s4) */

    /* ─── Identificador (s3) ─── */
    TT_IDENT,              /* niño, calcular_total */

    /* ─── Palabras clave (s3) ─── */
    /* Control de flujo */
    TT_SI, TT_SINO, TT_MIENTRAS, TT_PARA, TT_EN,
    TT_ROMPER, TT_CONTINUAR, TT_RETORNAR, TT_PASAR,
    TT_FIN,                /* `fin <etiqueta>` se reconoce como dos tokens (decisión B1) */

    /* Funciones, clases, módulos */
    TT_FUNCION, TT_LAMBDA, TT_CLASE, TT_EXTIENDE, TT_SUPER,
    TT_IMPORTAR, TT_DESDE, TT_COMO, TT_GLOBAL, TT_NOLOCAL,

    /* Excepciones */
    TT_INTENTAR, TT_ATRAPAR, TT_FINALMENTE, TT_LANZAR,

    /* Operadores lógicos / comparativos como palabras */
    TT_Y, TT_O, TT_NO, TT_ES,

    /* Literales por palabra */
    TT_VERDADERO, TT_FALSO, TT_NULO,

    /* Reservadas para futuro: el lexer las reconoce, el parser las
       rechaza con mensaje específico hasta su versión de implementación */
    TT_PRODUCIR, TT_ASINCRONO, TT_ESPERAR, TT_CON, TT_BORRAR, TT_COINCIDIR,

    /* ─── Especiales ─── */
    TT_FIN_ARCHIVO,        /* marca de fin de fuente */
    TT_ERROR,              /* error léxico; el lexema apunta al mensaje */
} TipoToken;

/*
 * Token producido por el lexer.
 *
 * `inicio` apunta al primer byte del lexema dentro del buffer fuente
 * (no se copia). `longitud` es en bytes. Para tokens TT_ERROR, `inicio`
 * apunta al mensaje de error (cadena estática gestionada por el lexer).
 *
 * `linea` es 1-indexed. `columna` es 1-indexed por bytes desde el
 * inicio de la línea (la conversión a columnas por código de carácter
 * para mensajes de error se hará en sesión 5).
 */
typedef struct {
    TipoToken tipo;
    const char *inicio;
    int longitud;
    int linea;
    int columna;
} Token;

/*
 * Estado del lexer. El cliente lo crea (típicamente en pila), llama
 * lexer_iniciar(), y luego invoca lexer_siguiente() en bucle.
 */
typedef struct {
    const char *fuente;        /* inicio del buffer (no posee, debe vivir más que el lexer) */
    const char *actual;        /* posición del scanner */
    const char *inicio_token;  /* inicio del lexema en curso */
    int linea;                 /* línea actual (1-indexed) */
    const char *inicio_linea;  /* inicio de la línea actual (para columna) */
    const char *archivo;       /* ruta del archivo (para errores) o NULL */
} Lexer;

/*
 * Inicializa el lexer apuntando al inicio de `fuente` (debe estar
 * terminada en `\0`). `archivo` es la ruta para mensajes de error o
 * NULL si la fuente viene del REPL.
 */
void lexer_iniciar(Lexer *l, const char *fuente, const char *archivo);

/*
 * Devuelve el siguiente token. Tras alcanzar el fin de archivo, sigue
 * devolviendo TT_FIN_ARCHIVO indefinidamente (idempotente).
 *
 * Los tokens TT_ERROR llevan el mensaje en `inicio` (no es lexema real).
 * El cliente decide si tratar errores como fatales o continuar tokenizando
 * para reportar varios errores de una pasada.
 */
Token lexer_siguiente(Lexer *l);

/*
 * Devuelve el nombre simbólico del tipo de token (ej. "TT_MAS").
 * Útil para tests y depuración. Cadena estática, no se libera.
 */
const char *tipo_token_nombre(TipoToken t);

#endif /* CORNAMUSA_LEXER_H */
