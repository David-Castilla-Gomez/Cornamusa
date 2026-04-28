#include "compilador.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "valor.h"

/* Pone error en c->error preservando el primero. */
static void error_compilacion(Compilador *c, int linea, int columna,
                               const char *fmt, ...) {
    if (c->error.tuvo_error) return;
    c->error.tuvo_error = true;
    c->error.linea = linea;
    c->error.columna = columna;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(c->error.mensaje, sizeof(c->error.mensaje), fmt, ap);
    va_end(ap);
}

void compilador_iniciar(Compilador *c, Chunk *chunk) {
    c->chunk = chunk;
    c->error.tuvo_error = false;
    c->error.mensaje[0] = '\0';
    c->error.linea = 0;
    c->error.columna = 0;
}

/* ──────────────────────────────────────────────────────────────────
 * Mapeo TipoToken (operador) → OpCode binario.
 * Devuelve -1 si el operador no tiene representación directa
 * (los binarios no soportados se rechazan con error).
 * ────────────────────────────────────────────────────────────────── */

static int token_a_opcode_binario(TipoToken op) {
    switch (op) {
        case TT_MAS:             return OP_SUMAR;
        case TT_MENOS:           return OP_RESTAR;
        case TT_ASTERISCO:       return OP_MULTIPLICAR;
        case TT_BARRA:           return OP_DIVIDIR;
        case TT_DOBLE_BARRA:     return OP_DIVIDIR_ENTERO;
        case TT_PORCENTAJE:      return OP_MODULO;
        case TT_DOBLE_ASTERISCO: return OP_POTENCIA;
        case TT_IGUAL:           return OP_IGUAL;
        case TT_DISTINTO:        return OP_DISTINTO;
        case TT_MENOR:           return OP_MENOR;
        case TT_MENOR_IGUAL:     return OP_MENOR_IGUAL;
        case TT_MAYOR:           return OP_MAYOR;
        case TT_MAYOR_IGUAL:     return OP_MAYOR_IGUAL;
        default:                 return -1;
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Helpers para emitir Valores como constantes.
 *
 * Las cadenas literales del AST incluyen las comillas y secuencias
 * de escape. Las procesamos aquí igual que en el evaluador
 * tree-walking (`EXPR_LITERAL_CADENA`) para que ambos motores
 * produzcan el mismo Valor cadena.
 * ────────────────────────────────────────────────────────────────── */

static Valor cadena_desde_lexema(const char *lex, int len) {
    if (len < 2) return valor_cadena_referencia("", 0);
    const char *src = lex + 1;
    int srclen = len - 2;
    char *buf = (char *)malloc((size_t)srclen + 1);
    if (!buf) return valor_nulo();
    int j = 0;
    for (int i = 0; i < srclen; i++) {
        char ch = src[i];
        if (ch == '\\' && i + 1 < srclen) {
            char nx = src[++i];
            switch (nx) {
                case 'n': buf[j++] = '\n'; break;
                case 't': buf[j++] = '\t'; break;
                case 'r': buf[j++] = '\r'; break;
                case '0': buf[j++] = '\0'; break;
                case '\\': buf[j++] = '\\'; break;
                case '\'': buf[j++] = '\''; break;
                case '"': buf[j++] = '"'; break;
                default: buf[j++] = nx; break;
            }
        } else {
            buf[j++] = ch;
        }
    }
    buf[j] = '\0';
    Valor v;
    v.tipo = VAL_CADENA;
    v.dueno_cadena = true;
    v.como.cadena.texto = buf;
    v.como.cadena.longitud = j;
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Compilación de expresiones
 * ────────────────────────────────────────────────────────────────── */

bool compilador_compilar_expr(Compilador *c, const Expr *e) {
    if (c->error.tuvo_error) return false;

    switch (e->tipo) {
        case EXPR_LITERAL_NULO:
            chunk_emitir_byte(c->chunk, OP_NULO, e->linea);
            return true;

        case EXPR_LITERAL_BOOLEANO:
            chunk_emitir_byte(c->chunk,
                e->como.booleano.valor ? OP_VERDADERO : OP_FALSO,
                e->linea);
            return true;

        case EXPR_LITERAL_ENTERO: {
            Valor v = valor_entero_de_lexema(e->como.literal.lexema,
                                               e->como.literal.longitud);
            chunk_emitir_constante(c->chunk, v, e->linea);
            return true;
        }
        case EXPR_LITERAL_DECIMAL: {
            Valor v = valor_decimal_de_lexema(e->como.literal.lexema,
                                                e->como.literal.longitud);
            chunk_emitir_constante(c->chunk, v, e->linea);
            return true;
        }
        case EXPR_LITERAL_CADENA: {
            Valor v = cadena_desde_lexema(e->como.literal.lexema,
                                            e->como.literal.longitud);
            chunk_emitir_constante(c->chunk, v, e->linea);
            return true;
        }

        case EXPR_GRUPO:
            return compilador_compilar_expr(c, e->como.grupo.interna);

        case EXPR_BINARIO: {
            if (!compilador_compilar_expr(c, e->como.binario.izq)) return false;
            if (!compilador_compilar_expr(c, e->como.binario.der)) return false;
            int op = token_a_opcode_binario(e->como.binario.op);
            if (op < 0) {
                error_compilacion(c, e->linea, e->columna,
                    "operador binario no soportado en bytecode v0.6 sesion 2");
                return false;
            }
            chunk_emitir_byte(c->chunk, (uint8_t)op, e->linea);
            return true;
        }

        case EXPR_UNARIO: {
            if (!compilador_compilar_expr(c, e->como.unario.operando)) return false;
            switch (e->como.unario.op) {
                case TT_MENOS:
                    chunk_emitir_byte(c->chunk, OP_NEGAR, e->linea);
                    return true;
                case TT_NO:
                    chunk_emitir_byte(c->chunk, OP_NO, e->linea);
                    return true;
                case TT_MAS:
                    /* +x es identidad numérica; no emitimos nada — el
                       valor ya está en el tope. */
                    return true;
                default:
                    error_compilacion(c, e->linea, e->columna,
                        "operador unario no soportado en bytecode v0.6 sesion 2");
                    return false;
            }
        }

        /* Aplazadas a sesiones siguientes. */
        case EXPR_LITERAL_F_CADENA:
        case EXPR_IDENT:
        case EXPR_LOGICA:
        case EXPR_LLAMADA:
        case EXPR_ATRIBUTO:
        case EXPR_LAMBDA:
        case EXPR_LISTA:
        case EXPR_DICCIONARIO:
        case EXPR_CONJUNTO:
        case EXPR_TUPLA:
        case EXPR_INDICE:
        case EXPR_REBANADA:
            error_compilacion(c, e->linea, e->columna,
                "esta forma de expresion no esta implementada en bytecode v0.6 sesion 2");
            return false;
    }

    error_compilacion(c, e->linea, e->columna,
        "tipo de expresion desconocido");
    return false;
}

bool compilador_compilar_expr_top(Compilador *c, const Expr *e) {
    if (!compilador_compilar_expr(c, e)) return false;
    chunk_emitir_byte(c->chunk, OP_RETORNAR, e->linea);
    return true;
}
