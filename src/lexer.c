#include "lexer.h"

#include <string.h>

/* ──────────────────────────────────────────────────────────────────
 * Utilidades internas
 * ────────────────────────────────────────────────────────────────── */

static bool en_fin(const Lexer *l) {
    return *l->actual == '\0';
}

static char avanzar(Lexer *l) {
    char c = *l->actual;
    l->actual++;
    return c;
}

static char mirar(const Lexer *l) {
    return *l->actual;
}

static bool coincidir(Lexer *l, char esperado) {
    if (en_fin(l)) return false;
    if (*l->actual != (unsigned char)esperado) return false;
    l->actual++;
    return true;
}

static int columna_actual(const Lexer *l) {
    return (int)(l->inicio_token - l->inicio_linea) + 1;
}

static Token crear_token(const Lexer *l, TipoToken tipo) {
    Token t;
    t.tipo = tipo;
    t.inicio = l->inicio_token;
    t.longitud = (int)(l->actual - l->inicio_token);
    t.linea = l->linea;
    t.columna = columna_actual(l);
    return t;
}

/*
 * Token de error léxico. `mensaje` debe ser una cadena estática
 * (literal o gestionada externamente), porque la guardamos por puntero
 * en `inicio` sin copiarla.
 */
static Token token_error(const Lexer *l, const char *mensaje) {
    Token t;
    t.tipo = TT_ERROR;
    t.inicio = mensaje;
    t.longitud = (int)strlen(mensaje);
    t.linea = l->linea;
    t.columna = columna_actual(l);
    return t;
}

/* ──────────────────────────────────────────────────────────────────
 * Whitespace y comentarios
 * ────────────────────────────────────────────────────────────────── */

/*
 * Salta espacios, tabuladores, retornos de carro, saltos de línea y
 * comentarios `# ...`. Los saltos de línea avanzan el contador de línea
 * y reinician el inicio de línea para el cómputo de columna.
 *
 * Decisión B1: la indentación NO es semántica, así que tabuladores y
 * espacios al principio de línea son simplemente whitespace.
 */
static void saltar_irrelevante(Lexer *l) {
    for (;;) {
        char c = mirar(l);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                avanzar(l);
                break;
            case '\n':
                avanzar(l);
                l->linea++;
                l->inicio_linea = l->actual;
                break;
            case '#':
                /* Comentario hasta fin de línea. El '\n' se trata en la
                   siguiente iteración para que se contabilice bien. */
                while (!en_fin(l) && mirar(l) != '\n') {
                    avanzar(l);
                }
                break;
            default:
                return;
        }
    }
}

/* ──────────────────────────────────────────────────────────────────
 * API pública
 * ────────────────────────────────────────────────────────────────── */

void lexer_iniciar(Lexer *l, const char *fuente, const char *archivo) {
    l->fuente = fuente;
    l->actual = fuente;
    l->inicio_token = fuente;
    l->linea = 1;
    l->inicio_linea = fuente;
    l->archivo = archivo;
}

Token lexer_siguiente(Lexer *l) {
    saltar_irrelevante(l);

    l->inicio_token = l->actual;

    if (en_fin(l)) {
        return crear_token(l, TT_FIN_ARCHIVO);
    }

    char c = avanzar(l);

    switch (c) {
        /* Símbolos individuales */
        case '(': return crear_token(l, TT_PARENT_IZQ);
        case ')': return crear_token(l, TT_PARENT_DER);
        case '[': return crear_token(l, TT_CORCH_IZQ);
        case ']': return crear_token(l, TT_CORCH_DER);
        case '{': return crear_token(l, TT_LLAVE_IZQ);
        case '}': return crear_token(l, TT_LLAVE_DER);
        case ',': return crear_token(l, TT_COMA);
        case '.': return crear_token(l, TT_PUNTO);
        case ':': return crear_token(l, TT_DOS_PUNTOS);
        case ';': return crear_token(l, TT_PUNTO_COMA);
        case '@': return crear_token(l, TT_AT);
        case '~': return crear_token(l, TT_TILDE_BIT);

        /* Aritméticos con posible '=' compuesto */
        case '+':
            return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_MAS : TT_MAS);
        case '-':
            if (coincidir(l, '=')) return crear_token(l, TT_ASIGNAR_MENOS);
            if (coincidir(l, '>')) return crear_token(l, TT_FLECHA);
            return crear_token(l, TT_MENOS);
        case '*':
            if (coincidir(l, '*')) {
                return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_DOBLE_ASTER
                                                        : TT_DOBLE_ASTERISCO);
            }
            return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_ASTERISCO
                                                    : TT_ASTERISCO);
        case '/':
            if (coincidir(l, '/')) {
                return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_DOBLE_BARRA
                                                        : TT_DOBLE_BARRA);
            }
            return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_BARRA : TT_BARRA);
        case '%':
            return crear_token(l, coincidir(l, '=') ? TT_ASIGNAR_PORCENTAJE
                                                    : TT_PORCENTAJE);

        /* Asignación / igualdad */
        case '=':
            return crear_token(l, coincidir(l, '=') ? TT_IGUAL : TT_ASIGNAR);

        /* '!' solo válido como '!=' */
        case '!':
            if (coincidir(l, '=')) return crear_token(l, TT_DISTINTO);
            return token_error(l, "carácter '!' inesperado (¿quisiste decir '!='?)");

        /* Comparaciones y desplazamientos */
        case '<':
            if (coincidir(l, '=')) return crear_token(l, TT_MENOR_IGUAL);
            if (coincidir(l, '<')) return crear_token(l, TT_DESPL_IZQ);
            return crear_token(l, TT_MENOR);
        case '>':
            if (coincidir(l, '=')) return crear_token(l, TT_MAYOR_IGUAL);
            if (coincidir(l, '>')) return crear_token(l, TT_DESPL_DER);
            return crear_token(l, TT_MAYOR);

        /* Bitwise simples */
        case '&': return crear_token(l, TT_AMPERSAND);
        case '|': return crear_token(l, TT_BARRA_VERT);
        case '^': return crear_token(l, TT_CIRCUNFLEJO);

        default:
            /* Identificadores, números y cadenas llegan en sesiones 2-3.
               Por ahora cualquier otro carácter es un error léxico. */
            return token_error(l, "carácter no reconocido");
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Nombre simbólico de un tipo de token (para tests y depuración)
 * ────────────────────────────────────────────────────────────────── */

const char *tipo_token_nombre(TipoToken t) {
    switch (t) {
        case TT_PARENT_IZQ:          return "TT_PARENT_IZQ";
        case TT_PARENT_DER:          return "TT_PARENT_DER";
        case TT_LLAVE_IZQ:           return "TT_LLAVE_IZQ";
        case TT_LLAVE_DER:           return "TT_LLAVE_DER";
        case TT_CORCH_IZQ:           return "TT_CORCH_IZQ";
        case TT_CORCH_DER:           return "TT_CORCH_DER";
        case TT_COMA:                return "TT_COMA";
        case TT_PUNTO:               return "TT_PUNTO";
        case TT_DOS_PUNTOS:          return "TT_DOS_PUNTOS";
        case TT_PUNTO_COMA:          return "TT_PUNTO_COMA";
        case TT_FLECHA:              return "TT_FLECHA";
        case TT_AT:                  return "TT_AT";

        case TT_MAS:                 return "TT_MAS";
        case TT_MENOS:               return "TT_MENOS";
        case TT_ASTERISCO:           return "TT_ASTERISCO";
        case TT_BARRA:               return "TT_BARRA";
        case TT_DOBLE_BARRA:         return "TT_DOBLE_BARRA";
        case TT_PORCENTAJE:          return "TT_PORCENTAJE";
        case TT_DOBLE_ASTERISCO:     return "TT_DOBLE_ASTERISCO";

        case TT_IGUAL:               return "TT_IGUAL";
        case TT_DISTINTO:            return "TT_DISTINTO";
        case TT_MENOR:               return "TT_MENOR";
        case TT_MENOR_IGUAL:         return "TT_MENOR_IGUAL";
        case TT_MAYOR:               return "TT_MAYOR";
        case TT_MAYOR_IGUAL:         return "TT_MAYOR_IGUAL";

        case TT_ASIGNAR:             return "TT_ASIGNAR";
        case TT_ASIGNAR_MAS:         return "TT_ASIGNAR_MAS";
        case TT_ASIGNAR_MENOS:       return "TT_ASIGNAR_MENOS";
        case TT_ASIGNAR_ASTERISCO:   return "TT_ASIGNAR_ASTERISCO";
        case TT_ASIGNAR_BARRA:       return "TT_ASIGNAR_BARRA";
        case TT_ASIGNAR_DOBLE_BARRA: return "TT_ASIGNAR_DOBLE_BARRA";
        case TT_ASIGNAR_PORCENTAJE:  return "TT_ASIGNAR_PORCENTAJE";
        case TT_ASIGNAR_DOBLE_ASTER: return "TT_ASIGNAR_DOBLE_ASTER";

        case TT_AMPERSAND:           return "TT_AMPERSAND";
        case TT_BARRA_VERT:          return "TT_BARRA_VERT";
        case TT_CIRCUNFLEJO:         return "TT_CIRCUNFLEJO";
        case TT_TILDE_BIT:           return "TT_TILDE_BIT";
        case TT_DESPL_IZQ:           return "TT_DESPL_IZQ";
        case TT_DESPL_DER:           return "TT_DESPL_DER";

        case TT_ENTERO:              return "TT_ENTERO";
        case TT_DECIMAL:             return "TT_DECIMAL";
        case TT_CADENA:              return "TT_CADENA";
        case TT_F_CADENA:            return "TT_F_CADENA";

        case TT_IDENT:               return "TT_IDENT";

        case TT_SI:                  return "TT_SI";
        case TT_SINO:                return "TT_SINO";
        case TT_MIENTRAS:            return "TT_MIENTRAS";
        case TT_PARA:                return "TT_PARA";
        case TT_EN:                  return "TT_EN";
        case TT_ROMPER:              return "TT_ROMPER";
        case TT_CONTINUAR:           return "TT_CONTINUAR";
        case TT_RETORNAR:            return "TT_RETORNAR";
        case TT_PASAR:               return "TT_PASAR";
        case TT_FIN:                 return "TT_FIN";

        case TT_FUNCION:             return "TT_FUNCION";
        case TT_LAMBDA:              return "TT_LAMBDA";
        case TT_CLASE:               return "TT_CLASE";
        case TT_EXTIENDE:            return "TT_EXTIENDE";
        case TT_SUPER:               return "TT_SUPER";
        case TT_IMPORTAR:            return "TT_IMPORTAR";
        case TT_DESDE:               return "TT_DESDE";
        case TT_COMO:                return "TT_COMO";
        case TT_GLOBAL:              return "TT_GLOBAL";
        case TT_NOLOCAL:             return "TT_NOLOCAL";

        case TT_INTENTAR:            return "TT_INTENTAR";
        case TT_ATRAPAR:             return "TT_ATRAPAR";
        case TT_FINALMENTE:          return "TT_FINALMENTE";
        case TT_LANZAR:              return "TT_LANZAR";

        case TT_Y:                   return "TT_Y";
        case TT_O:                   return "TT_O";
        case TT_NO:                  return "TT_NO";
        case TT_ES:                  return "TT_ES";

        case TT_VERDADERO:           return "TT_VERDADERO";
        case TT_FALSO:               return "TT_FALSO";
        case TT_NULO:                return "TT_NULO";

        case TT_PRODUCIR:            return "TT_PRODUCIR";
        case TT_ASINCRONO:           return "TT_ASINCRONO";
        case TT_ESPERAR:             return "TT_ESPERAR";
        case TT_CON:                 return "TT_CON";
        case TT_BORRAR:              return "TT_BORRAR";
        case TT_COINCIDIR:           return "TT_COINCIDIR";

        case TT_FIN_ARCHIVO:         return "TT_FIN_ARCHIVO";
        case TT_ERROR:               return "TT_ERROR";
    }
    return "TT_DESCONOCIDO";
}
