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

/*
 * Emite el bytecode necesario para resolver un identificador como
 * variable global. Útil tanto para EXPR_IDENT (lectura) como para
 * SENT_ASIGNAR (escritura).
 *
 * Devuelve el índice de la constante (nombre clonado) en el chunk,
 * o -1 si excede 255 (limitación de v0.6 sesión 3 — `OP_OBTENER_GLOBAL`
 * usa operando byte). Para más globales habrá que añadir variantes
 * `*_LARGO`, igual que con `OP_CONST_LARGO`.
 */
static int agregar_nombre_global(Compilador *c, const char *texto, int len) {
    Valor name = valor_cadena_duplicar(texto, len);
    int idx = chunk_agregar_constante(c->chunk, name);
    return idx;
}

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

        case EXPR_IDENT: {
            int idx = agregar_nombre_global(c, e->como.ident.nombre,
                                              e->como.ident.longitud);
            if (idx < 0 || idx > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "demasiadas constantes para v0.6 (operando byte)");
                return false;
            }
            chunk_emitir_byte2(c->chunk, OP_OBTENER_GLOBAL, (uint8_t)idx, e->linea);
            return true;
        }

        case EXPR_LLAMADA: {
            /*
             * En v0.6 sesión 3 solo se compila el caso especial
             * `imprimir(args...)` (built-in). Otras llamadas necesitan
             * OP_LLAMAR + frames, que llegan en S5.
             */
            const Expr *callee = e->como.llamada.callee;
            bool es_imprimir =
                callee->tipo == EXPR_IDENT
                && callee->como.ident.longitud == 8
                && memcmp(callee->como.ident.nombre, "imprimir", 8) == 0;
            if (!es_imprimir) {
                error_compilacion(c, e->linea, e->columna,
                    "llamadas a funciones definidas por el usuario aun no estan en bytecode v0.6 sesion 3");
                return false;
            }
            if (e->como.llamada.n_args > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "imprimir() no puede tener mas de 255 argumentos");
                return false;
            }
            for (int i = 0; i < e->como.llamada.n_args; i++) {
                if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
            }
            chunk_emitir_byte2(c->chunk, OP_IMPRIMIR,
                               (uint8_t)e->como.llamada.n_args, e->linea);
            return true;
        }

        /* Aplazadas a sesiones siguientes. */
        case EXPR_LITERAL_F_CADENA:
        case EXPR_LOGICA:
        case EXPR_ATRIBUTO:
        case EXPR_LAMBDA:
        case EXPR_LISTA:
        case EXPR_DICCIONARIO:
        case EXPR_CONJUNTO:
        case EXPR_TUPLA:
        case EXPR_INDICE:
        case EXPR_REBANADA:
            error_compilacion(c, e->linea, e->columna,
                "esta forma de expresion no esta implementada en bytecode v0.6 sesion 3");
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

/* ──────────────────────────────────────────────────────────────────
 * Sentencias
 *
 * Sesión 3 soporta el subconjunto necesario para programas reales
 * lineales: asignación a global, sentencia-expresión (típicamente
 * llamadas a `imprimir`), `pasar`, y `bloque` para anidamiento.
 * Control de flujo (`si`, `mientras`, `para`) llega en S4. Funciones
 * y `retornar` en S5.
 * ────────────────────────────────────────────────────────────────── */

static bool compilar_asignar(Compilador *c, const Sent *s) {
    Expr *destino = s->como.asignar.destino;
    if (destino->tipo != EXPR_IDENT) {
        error_compilacion(c, s->linea, s->columna,
            "ErrorDeSintaxis: destino de asignacion no soportado en bytecode v0.6 sesion 3");
        return false;
    }
    if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;
    int idx = agregar_nombre_global(c, destino->como.ident.nombre,
                                      destino->como.ident.longitud);
    if (idx < 0 || idx > 255) {
        error_compilacion(c, s->linea, s->columna,
            "demasiadas constantes para v0.6 (operando byte)");
        return false;
    }
    chunk_emitir_byte2(c->chunk, OP_DEFINIR_GLOBAL, (uint8_t)idx, s->linea);
    return true;
}

bool compilador_compilar_sent(Compilador *c, const Sent *s) {
    if (c->error.tuvo_error) return false;

    switch (s->tipo) {
        case SENT_PASAR:
            return true;

        case SENT_EXPR:
            if (!compilador_compilar_expr(c, s->como.expr.expr)) return false;
            chunk_emitir_byte(c->chunk, OP_DESCARTAR, s->linea);
            return true;

        case SENT_ASIGNAR:
            return compilar_asignar(c, s);

        case SENT_BLOQUE: {
            int n = s->como.bloque.n_sentencias;
            for (int i = 0; i < n; i++) {
                if (!compilador_compilar_sent(c, s->como.bloque.sentencias[i])) {
                    return false;
                }
            }
            return true;
        }

        /* Sin soporte aún. */
        case SENT_ASIGNAR_AUG:
        case SENT_ROMPER:
        case SENT_CONTINUAR:
        case SENT_RETORNAR:
        case SENT_SI:
        case SENT_MIENTRAS:
        case SENT_PARA:
        case SENT_FUNCION:
        case SENT_CLASE:
        case SENT_INTENTAR:
        case SENT_LANZAR:
        case SENT_IMPORTAR:
        case SENT_DESDE_IMPORTAR:
        case SENT_GLOBAL:
        case SENT_NOLOCAL:
            error_compilacion(c, s->linea, s->columna,
                "esta sentencia aun no esta implementada en bytecode v0.6 sesion 3");
            return false;
    }
    error_compilacion(c, s->linea, s->columna,
        "tipo de sentencia desconocido");
    return false;
}

bool compilador_compilar_programa(Compilador *c, Sent **sents, int n) {
    for (int i = 0; i < n; i++) {
        if (!compilador_compilar_sent(c, sents[i])) return false;
    }
    /* OP_RETORNAR final con nulo: la VM termina y el cliente recibe
       nulo como "valor del programa" (las sentencias no producen
       valor; lo importante es el side-effect de las asignaciones a
       globales). */
    int linea_final = (n > 0) ? sents[n - 1]->linea : 1;
    chunk_emitir_byte(c->chunk, OP_NULO, linea_final);
    chunk_emitir_byte(c->chunk, OP_RETORNAR, linea_final);
    return true;
}
