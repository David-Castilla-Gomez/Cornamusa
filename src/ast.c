#include "ast.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────
 * Constructores
 * ────────────────────────────────────────────────────────────────── */

static Expr *nuevo_expr(Arena *a, TipoExpr tipo, int linea, int col) {
    Expr *e = (Expr *)arena_alocar_cero(a, sizeof(Expr));
    if (e == NULL) return NULL;
    e->tipo = tipo;
    e->linea = linea;
    e->columna = col;
    return e;
}

Expr *expr_literal_entero(Arena *a, const char *lexema, int len, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_LITERAL_ENTERO, linea, col);
    if (e) { e->como.literal.lexema = lexema; e->como.literal.longitud = len; }
    return e;
}

Expr *expr_literal_decimal(Arena *a, const char *lexema, int len, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_LITERAL_DECIMAL, linea, col);
    if (e) { e->como.literal.lexema = lexema; e->como.literal.longitud = len; }
    return e;
}

Expr *expr_literal_cadena(Arena *a, const char *lexema, int len, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_LITERAL_CADENA, linea, col);
    if (e) { e->como.literal.lexema = lexema; e->como.literal.longitud = len; }
    return e;
}

Expr *expr_literal_f_cadena(Arena *a, const char *lexema, int len, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_LITERAL_F_CADENA, linea, col);
    if (e) { e->como.literal.lexema = lexema; e->como.literal.longitud = len; }
    return e;
}

Expr *expr_literal_booleano(Arena *a, bool valor, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_LITERAL_BOOLEANO, linea, col);
    if (e) e->como.booleano.valor = valor;
    return e;
}

Expr *expr_literal_nulo(Arena *a, int linea, int col) {
    return nuevo_expr(a, EXPR_LITERAL_NULO, linea, col);
}

Expr *expr_ident(Arena *a, const char *nombre, int len, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_IDENT, linea, col);
    if (e) { e->como.ident.nombre = nombre; e->como.ident.longitud = len; }
    return e;
}

Expr *expr_binario(Arena *a, Expr *izq, TipoToken op, Expr *der, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_BINARIO, linea, col);
    if (e) {
        e->como.binario.izq = izq;
        e->como.binario.der = der;
        e->como.binario.op = op;
    }
    return e;
}

Expr *expr_unario(Arena *a, TipoToken op, Expr *operando, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_UNARIO, linea, col);
    if (e) { e->como.unario.op = op; e->como.unario.operando = operando; }
    return e;
}

Expr *expr_logica(Arena *a, Expr *izq, bool es_y, Expr *der, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_LOGICA, linea, col);
    if (e) {
        e->como.logica.izq = izq;
        e->como.logica.der = der;
        e->como.logica.es_y = es_y;
    }
    return e;
}

Expr *expr_llamada(Arena *a, Expr *callee, Expr **args, int n_args, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_LLAMADA, linea, col);
    if (e) {
        e->como.llamada.callee = callee;
        e->como.llamada.args = args;
        e->como.llamada.n_args = n_args;
    }
    return e;
}

Expr *expr_atributo(Arena *a, Expr *objeto, const char *nombre, int len, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_ATRIBUTO, linea, col);
    if (e) {
        e->como.atributo.objeto = objeto;
        e->como.atributo.nombre = nombre;
        e->como.atributo.longitud = len;
    }
    return e;
}

Expr *expr_grupo(Arena *a, Expr *interna, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_GRUPO, linea, col);
    if (e) e->como.grupo.interna = interna;
    return e;
}

/* ──────────────────────────────────────────────────────────────────
 * Pretty-printer (S-expression style)
 *
 * Tipos que emiten nombres simbólicos:
 *   (lit-int "42")
 *   (lit-dec "3.14")
 *   (lit-str "\"hola\"")
 *   (lit-fstr "f\"...\"")
 *   (lit-bool verdadero)
 *   (lit-nulo)
 *   (ident "nombre")
 *   (op "+" izq der)         para binarios
 *   (uop "-" operando)        para unarios
 *   (y izq der) / (o izq der) para lógicas
 *   (llamada callee arg1 arg2 ...)
 *   (atr objeto "nombre")
 *   (grupo interna)
 * ────────────────────────────────────────────────────────────────── */

/* Devuelve la cadena del operador para imprimir. */
static const char *nombre_op(TipoToken t) {
    switch (t) {
        case TT_MAS:              return "+";
        case TT_MENOS:            return "-";
        case TT_ASTERISCO:        return "*";
        case TT_BARRA:            return "/";
        case TT_DOBLE_BARRA:      return "//";
        case TT_PORCENTAJE:       return "%";
        case TT_DOBLE_ASTERISCO:  return "**";
        case TT_IGUAL:            return "==";
        case TT_DISTINTO:         return "!=";
        case TT_MENOR:            return "<";
        case TT_MENOR_IGUAL:      return "<=";
        case TT_MAYOR:            return ">";
        case TT_MAYOR_IGUAL:      return ">=";
        case TT_AMPERSAND:        return "&";
        case TT_BARRA_VERT:       return "|";
        case TT_CIRCUNFLEJO:      return "^";
        case TT_DESPL_IZQ:        return "<<";
        case TT_DESPL_DER:        return ">>";
        case TT_TILDE_BIT:        return "~";
        case TT_NO:               return "no";
        case TT_ES:               return "es";
        case TT_EN:               return "en";
        default:                  return "?";
    }
}

/* Imprime una cadena entrecomillando su contenido si tiene espacios
   o caracteres especiales. Para lexemas que ya incluyen comillas
   (cadenas), las dejamos tal cual. */
static void escribir_lexema(FILE *out, const char *texto, int len) {
    fprintf(out, "%.*s", len, texto);
}

void expr_imprimir(const Expr *e, FILE *out) {
    if (e == NULL) {
        fputs("(null)", out);
        return;
    }
    switch (e->tipo) {
        case EXPR_LITERAL_ENTERO:
            fputs("(lit-int ", out);
            escribir_lexema(out, e->como.literal.lexema, e->como.literal.longitud);
            fputc(')', out);
            break;
        case EXPR_LITERAL_DECIMAL:
            fputs("(lit-dec ", out);
            escribir_lexema(out, e->como.literal.lexema, e->como.literal.longitud);
            fputc(')', out);
            break;
        case EXPR_LITERAL_CADENA:
            fputs("(lit-str ", out);
            escribir_lexema(out, e->como.literal.lexema, e->como.literal.longitud);
            fputc(')', out);
            break;
        case EXPR_LITERAL_F_CADENA:
            fputs("(lit-fstr ", out);
            escribir_lexema(out, e->como.literal.lexema, e->como.literal.longitud);
            fputc(')', out);
            break;
        case EXPR_LITERAL_BOOLEANO:
            fputs(e->como.booleano.valor ? "(lit-bool verdadero)" : "(lit-bool falso)", out);
            break;
        case EXPR_LITERAL_NULO:
            fputs("(lit-nulo)", out);
            break;
        case EXPR_IDENT:
            fputs("(ident ", out);
            escribir_lexema(out, e->como.ident.nombre, e->como.ident.longitud);
            fputc(')', out);
            break;
        case EXPR_BINARIO:
            fprintf(out, "(op \"%s\" ", nombre_op(e->como.binario.op));
            expr_imprimir(e->como.binario.izq, out);
            fputc(' ', out);
            expr_imprimir(e->como.binario.der, out);
            fputc(')', out);
            break;
        case EXPR_UNARIO:
            fprintf(out, "(uop \"%s\" ", nombre_op(e->como.unario.op));
            expr_imprimir(e->como.unario.operando, out);
            fputc(')', out);
            break;
        case EXPR_LOGICA:
            fprintf(out, "(%s ", e->como.logica.es_y ? "y" : "o");
            expr_imprimir(e->como.logica.izq, out);
            fputc(' ', out);
            expr_imprimir(e->como.logica.der, out);
            fputc(')', out);
            break;
        case EXPR_LLAMADA: {
            fputs("(llamada ", out);
            expr_imprimir(e->como.llamada.callee, out);
            for (int i = 0; i < e->como.llamada.n_args; i++) {
                fputc(' ', out);
                expr_imprimir(e->como.llamada.args[i], out);
            }
            fputc(')', out);
            break;
        }
        case EXPR_ATRIBUTO:
            fputs("(atr ", out);
            expr_imprimir(e->como.atributo.objeto, out);
            fputs(" \"", out);
            escribir_lexema(out, e->como.atributo.nombre, e->como.atributo.longitud);
            fputs("\")", out);
            break;
        case EXPR_GRUPO:
            fputs("(grupo ", out);
            expr_imprimir(e->como.grupo.interna, out);
            fputc(')', out);
            break;
    }
}

/*
 * Implementación de expr_a_cadena: usamos `open_memstream` o, dado
 * que es POSIX-only, fallback a un fmemopen si está disponible. Para
 * portabilidad cross-Windows escribimos a un archivo temporal o,
 * más simple, formateamos a un buffer mediante una función auxiliar
 * que no usa FILE*.
 *
 * Para simplicidad, esta implementación usa una variante recursiva
 * que escribe directamente al buffer.
 */

typedef struct {
    char *buffer;
    int capacidad;
    int usado;
} EscrituraBuffer;

static void wb_escribir(EscrituraBuffer *eb, const char *fmt, ...) {
    if (eb->usado >= eb->capacidad - 1) return;
    va_list args;
    va_start(args, fmt);
    int restante = eb->capacidad - eb->usado;
    int n = vsnprintf(eb->buffer + eb->usado, (size_t)restante, fmt, args);
    va_end(args);
    if (n > 0) {
        eb->usado += (n < restante) ? n : restante - 1;
    }
}

static void wb_escribir_lexema(EscrituraBuffer *eb, const char *texto, int len) {
    wb_escribir(eb, "%.*s", len, texto);
}

static void expr_a_buffer(const Expr *e, EscrituraBuffer *eb) {
    if (e == NULL) { wb_escribir(eb, "(null)"); return; }
    switch (e->tipo) {
        case EXPR_LITERAL_ENTERO:
            wb_escribir(eb, "(lit-int ");
            wb_escribir_lexema(eb, e->como.literal.lexema, e->como.literal.longitud);
            wb_escribir(eb, ")");
            break;
        case EXPR_LITERAL_DECIMAL:
            wb_escribir(eb, "(lit-dec ");
            wb_escribir_lexema(eb, e->como.literal.lexema, e->como.literal.longitud);
            wb_escribir(eb, ")");
            break;
        case EXPR_LITERAL_CADENA:
            wb_escribir(eb, "(lit-str ");
            wb_escribir_lexema(eb, e->como.literal.lexema, e->como.literal.longitud);
            wb_escribir(eb, ")");
            break;
        case EXPR_LITERAL_F_CADENA:
            wb_escribir(eb, "(lit-fstr ");
            wb_escribir_lexema(eb, e->como.literal.lexema, e->como.literal.longitud);
            wb_escribir(eb, ")");
            break;
        case EXPR_LITERAL_BOOLEANO:
            wb_escribir(eb, e->como.booleano.valor ? "(lit-bool verdadero)" : "(lit-bool falso)");
            break;
        case EXPR_LITERAL_NULO:
            wb_escribir(eb, "(lit-nulo)");
            break;
        case EXPR_IDENT:
            wb_escribir(eb, "(ident ");
            wb_escribir_lexema(eb, e->como.ident.nombre, e->como.ident.longitud);
            wb_escribir(eb, ")");
            break;
        case EXPR_BINARIO:
            wb_escribir(eb, "(op \"%s\" ", nombre_op(e->como.binario.op));
            expr_a_buffer(e->como.binario.izq, eb);
            wb_escribir(eb, " ");
            expr_a_buffer(e->como.binario.der, eb);
            wb_escribir(eb, ")");
            break;
        case EXPR_UNARIO:
            wb_escribir(eb, "(uop \"%s\" ", nombre_op(e->como.unario.op));
            expr_a_buffer(e->como.unario.operando, eb);
            wb_escribir(eb, ")");
            break;
        case EXPR_LOGICA:
            wb_escribir(eb, "(%s ", e->como.logica.es_y ? "y" : "o");
            expr_a_buffer(e->como.logica.izq, eb);
            wb_escribir(eb, " ");
            expr_a_buffer(e->como.logica.der, eb);
            wb_escribir(eb, ")");
            break;
        case EXPR_LLAMADA:
            wb_escribir(eb, "(llamada ");
            expr_a_buffer(e->como.llamada.callee, eb);
            for (int i = 0; i < e->como.llamada.n_args; i++) {
                wb_escribir(eb, " ");
                expr_a_buffer(e->como.llamada.args[i], eb);
            }
            wb_escribir(eb, ")");
            break;
        case EXPR_ATRIBUTO:
            wb_escribir(eb, "(atr ");
            expr_a_buffer(e->como.atributo.objeto, eb);
            wb_escribir(eb, " \"");
            wb_escribir_lexema(eb, e->como.atributo.nombre, e->como.atributo.longitud);
            wb_escribir(eb, "\")");
            break;
        case EXPR_GRUPO:
            wb_escribir(eb, "(grupo ");
            expr_a_buffer(e->como.grupo.interna, eb);
            wb_escribir(eb, ")");
            break;
    }
}

int expr_a_cadena(const Expr *e, char *buffer, int capacidad) {
    if (capacidad <= 0) return 0;
    EscrituraBuffer eb = { buffer, capacidad, 0 };
    expr_a_buffer(e, &eb);
    if (eb.usado >= eb.capacidad) eb.usado = eb.capacidad - 1;
    buffer[eb.usado] = '\0';
    return eb.usado;
}
