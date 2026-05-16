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

Expr *expr_literal_f_cadena(Arena *a, ParteFCadena *partes, int n_partes,
                              int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_LITERAL_F_CADENA, linea, col);
    if (e) {
        e->como.f_cadena.partes = partes;
        e->como.f_cadena.n_partes = n_partes;
    }
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

Expr *expr_ternaria(Arena *a, Expr *si_si, Expr *cond, Expr *si_no,
                      int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_TERNARIA, linea, col);
    if (e) {
        e->como.ternaria.cond = cond;
        e->como.ternaria.si_si = si_si;
        e->como.ternaria.si_no = si_no;
    }
    return e;
}

Expr *expr_llamada(Arena *a, Expr *callee, Expr **args, int n_args, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_LLAMADA, linea, col);
    if (e) {
        e->como.llamada.callee = callee;
        e->como.llamada.args = args;
        e->como.llamada.n_args = n_args;
        e->como.llamada.args_spread = NULL;  /* v1.22: NULL = sin spreads */
        e->como.llamada.kwarg_keys = NULL;   /* v1.23: NULL = sin kwargs */
        e->como.llamada.kwarg_lens = NULL;
        e->como.llamada.args_doble_spread = NULL;  /* v1.25: NULL = sin **spread */
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

Expr *expr_lambda(Arena *a, Parametro *params, int n_params, Expr *cuerpo,
                  int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_LAMBDA, linea, col);
    if (e) {
        e->como.lambda.parametros = params;
        e->como.lambda.n_parametros = n_params;
        e->como.lambda.cuerpo = cuerpo;
    }
    return e;
}

static Expr *expr_secuencia_interno(Arena *a, TipoExpr tipo, Expr **elementos,
                                     int n, int linea, int col) {
    Expr *e = nuevo_expr(a, tipo, linea, col);
    if (e) {
        e->como.secuencia.elementos = elementos;
        e->como.secuencia.n_elementos = n;
    }
    return e;
}

Expr *expr_lista(Arena *a, Expr **elementos, int n, int linea, int col) {
    return expr_secuencia_interno(a, EXPR_LISTA, elementos, n, linea, col);
}

Expr *expr_conjunto(Arena *a, Expr **elementos, int n, int linea, int col) {
    return expr_secuencia_interno(a, EXPR_CONJUNTO, elementos, n, linea, col);
}

Expr *expr_tupla(Arena *a, Expr **elementos, int n, int linea, int col) {
    return expr_secuencia_interno(a, EXPR_TUPLA, elementos, n, linea, col);
}

Expr *expr_diccionario(Arena *a, Expr **claves, Expr **valores, int n,
                       int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_DICCIONARIO, linea, col);
    if (e) {
        e->como.diccionario.claves = claves;
        e->como.diccionario.valores = valores;
        e->como.diccionario.n_pares = n;
    }
    return e;
}

/* v1.30: comprehension. tipo_destino: 0=lista, 1=dict, 2=conjunto. */
Expr *expr_comprehension(Arena *a, int tipo_destino,
                          Expr *expr_elem, Expr *expr_valor,
                          const char *nombre_var, int longitud_var,
                          Expr *iterable, Expr *guarda,
                          int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_COMPREHENSION, linea, col);
    if (e) {
        e->como.comprehension.tipo_destino = tipo_destino;
        e->como.comprehension.expr_elem = expr_elem;
        e->como.comprehension.expr_valor = expr_valor;
        e->como.comprehension.nombre_var = nombre_var;
        e->como.comprehension.longitud_var = longitud_var;
        e->como.comprehension.iterable = iterable;
        e->como.comprehension.guarda = guarda;
    }
    return e;
}

Expr *expr_indice(Arena *a, Expr *objeto, Expr *indice, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_INDICE, linea, col);
    if (e) {
        e->como.indice.objeto = objeto;
        e->como.indice.indice = indice;
    }
    return e;
}

Expr *expr_rebanada(Arena *a, Expr *objeto, Expr *inicio, Expr *fin, Expr *paso,
                    int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_REBANADA, linea, col);
    if (e) {
        e->como.rebanada.objeto = objeto;
        e->como.rebanada.inicio = inicio;
        e->como.rebanada.fin = fin;
        e->como.rebanada.paso = paso;
    }
    return e;
}

Expr *expr_super(Arena *a, const char *nombre, int len, int linea, int col) {
    Expr *e = nuevo_expr(a, EXPR_SUPER, linea, col);
    if (e) {
        e->como.super.nombre = nombre;
        e->como.super.longitud = len;
    }
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

/*
 * Versión hacia FILE*. Delega al pretty-printer basado en buffer
 * (expr_a_buffer) para no duplicar la enumeración de variantes.
 * Para expresiones grandes el buffer se trunca; en uso normal
 * (debug, REPL) cabe siempre.
 */
void expr_imprimir(const Expr *e, FILE *out) {
    char buffer[16384];
    expr_a_cadena(e, buffer, sizeof(buffer));
    fputs(buffer, out);
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
        case EXPR_LITERAL_F_CADENA: {
            wb_escribir(eb, "(lit-fstr");
            for (int i = 0; i < e->como.f_cadena.n_partes; i++) {
                const ParteFCadena *p = &e->como.f_cadena.partes[i];
                wb_escribir(eb, " ");
                if (p->expr) {
                    wb_escribir(eb, "(expr ");
                    expr_a_buffer(p->expr, eb);
                    wb_escribir(eb, ")");
                } else {
                    wb_escribir(eb, "(lit \"");
                    if (p->longitud > 0) {
                        for (int k = 0; k < p->longitud; k++) {
                            char ch = p->literal[k];
                            char tmp[2] = { ch, '\0' };
                            wb_escribir(eb, tmp);
                        }
                    }
                    wb_escribir(eb, "\")");
                }
            }
            wb_escribir(eb, ")");
            break;
        }
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
        case EXPR_LAMBDA: {
            wb_escribir(eb, "(lambda");
            for (int i = 0; i < e->como.lambda.n_parametros; i++) {
                Parametro *par = &e->como.lambda.parametros[i];
                wb_escribir(eb, " (param ");
                wb_escribir_lexema(eb, par->nombre, par->longitud_nombre);
                if (par->anotacion_tipo) {
                    wb_escribir(eb, " (tipo ");
                    expr_a_buffer(par->anotacion_tipo, eb);
                    wb_escribir(eb, ")");
                }
                if (par->valor_defecto) {
                    wb_escribir(eb, " (defecto ");
                    expr_a_buffer(par->valor_defecto, eb);
                    wb_escribir(eb, ")");
                }
                wb_escribir(eb, ")");
            }
            wb_escribir(eb, " ");
            expr_a_buffer(e->como.lambda.cuerpo, eb);
            wb_escribir(eb, ")");
            break;
        }
        case EXPR_LISTA:
        case EXPR_CONJUNTO:
        case EXPR_TUPLA: {
            const char *etiqueta =
                e->tipo == EXPR_LISTA ? "lista" :
                e->tipo == EXPR_CONJUNTO ? "conjunto" : "tupla";
            wb_escribir(eb, "(%s", etiqueta);
            for (int i = 0; i < e->como.secuencia.n_elementos; i++) {
                wb_escribir(eb, " ");
                expr_a_buffer(e->como.secuencia.elementos[i], eb);
            }
            wb_escribir(eb, ")");
            break;
        }
        case EXPR_DICCIONARIO:
            wb_escribir(eb, "(dicc");
            for (int i = 0; i < e->como.diccionario.n_pares; i++) {
                wb_escribir(eb, " (par ");
                expr_a_buffer(e->como.diccionario.claves[i], eb);
                wb_escribir(eb, " ");
                expr_a_buffer(e->como.diccionario.valores[i], eb);
                wb_escribir(eb, ")");
            }
            wb_escribir(eb, ")");
            break;
        case EXPR_INDICE:
            wb_escribir(eb, "(indice ");
            expr_a_buffer(e->como.indice.objeto, eb);
            wb_escribir(eb, " ");
            expr_a_buffer(e->como.indice.indice, eb);
            wb_escribir(eb, ")");
            break;
        case EXPR_REBANADA:
            wb_escribir(eb, "(rebanada ");
            expr_a_buffer(e->como.rebanada.objeto, eb);
            wb_escribir(eb, " ");
            if (e->como.rebanada.inicio) {
                expr_a_buffer(e->como.rebanada.inicio, eb);
            } else {
                wb_escribir(eb, "nulo");
            }
            wb_escribir(eb, " ");
            if (e->como.rebanada.fin) {
                expr_a_buffer(e->como.rebanada.fin, eb);
            } else {
                wb_escribir(eb, "nulo");
            }
            if (e->como.rebanada.paso) {
                wb_escribir(eb, " ");
                expr_a_buffer(e->como.rebanada.paso, eb);
            }
            wb_escribir(eb, ")");
            break;
        case EXPR_SUPER:
            wb_escribir(eb, "(super \"");
            wb_escribir_lexema(eb, e->como.super.nombre, e->como.super.longitud);
            wb_escribir(eb, "\")");
            break;
        case EXPR_TERNARIA:
            wb_escribir(eb, "(ternaria ");
            expr_a_buffer(e->como.ternaria.cond, eb);
            wb_escribir(eb, " ");
            expr_a_buffer(e->como.ternaria.si_si, eb);
            wb_escribir(eb, " ");
            expr_a_buffer(e->como.ternaria.si_no, eb);
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

/* ══════════════════════════════════════════════════════════════════
 * Sentencias — constructores
 * ══════════════════════════════════════════════════════════════════ */

static Sent *nuevo_sent(Arena *a, TipoSent tipo, int linea, int col) {
    Sent *s = (Sent *)arena_alocar_cero(a, sizeof(Sent));
    if (s == NULL) return NULL;
    s->tipo = tipo;
    s->linea = linea;
    s->columna = col;
    return s;
}

Sent *sent_expr(Arena *a, Expr *e, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_EXPR, linea, col);
    if (s) s->como.expr.expr = e;
    return s;
}

Sent *sent_asignar(Arena *a, Expr *destino, Expr *valor, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_ASIGNAR, linea, col);
    if (s) { s->como.asignar.destino = destino; s->como.asignar.valor = valor; }
    return s;
}

Sent *sent_asignar_aug(Arena *a, Expr *destino, TipoToken op, Expr *valor,
                       int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_ASIGNAR_AUG, linea, col);
    if (s) {
        s->como.asignar_aug.destino = destino;
        s->como.asignar_aug.op = op;
        s->como.asignar_aug.valor = valor;
    }
    return s;
}

Sent *sent_pasar(Arena *a, int linea, int col) {
    return nuevo_sent(a, SENT_PASAR, linea, col);
}

Sent *sent_romper(Arena *a, int linea, int col) {
    return nuevo_sent(a, SENT_ROMPER, linea, col);
}

Sent *sent_continuar(Arena *a, int linea, int col) {
    return nuevo_sent(a, SENT_CONTINUAR, linea, col);
}

Sent *sent_retornar(Arena *a, Expr *valor, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_RETORNAR, linea, col);
    if (s) s->como.retornar.valor = valor;
    return s;
}

Sent *sent_producir(Arena *a, Expr *valor, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_PRODUCIR, linea, col);
    if (s) s->como.producir.valor = valor;
    return s;
}

Sent *sent_si(Arena *a, RamaSi *ramas, int n_ramas, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_SI, linea, col);
    if (s) { s->como.si.ramas = ramas; s->como.si.n_ramas = n_ramas; }
    return s;
}

Sent *sent_mientras(Arena *a, Expr *cond, Sent *cuerpo, Sent *sino,
                    int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_MIENTRAS, linea, col);
    if (s) {
        s->como.mientras.condicion = cond;
        s->como.mientras.cuerpo = cuerpo;
        s->como.mientras.sino = sino;
    }
    return s;
}

Sent *sent_para(Arena *a, Expr *objetivo, Expr *iterable, Sent *cuerpo,
                Sent *sino, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_PARA, linea, col);
    if (s) {
        s->como.para.objetivo = objetivo;
        s->como.para.iterable = iterable;
        s->como.para.cuerpo = cuerpo;
        s->como.para.sino = sino;
    }
    return s;
}

Sent *sent_bloque(Arena *a, Sent **sentencias, int n, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_BLOQUE, linea, col);
    if (s) {
        s->como.bloque.sentencias = sentencias;
        s->como.bloque.n_sentencias = n;
    }
    return s;
}

Sent *sent_funcion(Arena *a, const char *nombre, int len_nombre,
                   Parametro *params, int n_params,
                   Expr *anot_retorno, Sent *cuerpo,
                   int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_FUNCION, linea, col);
    if (s) {
        s->como.funcion.nombre = nombre;
        s->como.funcion.longitud_nombre = len_nombre;
        s->como.funcion.parametros = params;
        s->como.funcion.n_parametros = n_params;
        s->como.funcion.anotacion_retorno = anot_retorno;
        s->como.funcion.cuerpo = cuerpo;
    }
    return s;
}

Sent *sent_clase(Arena *a, const char *nombre, int len_nombre,
                 Expr **supers, int n_supers, Sent *cuerpo,
                 int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_CLASE, linea, col);
    if (s) {
        s->como.clase.nombre = nombre;
        s->como.clase.longitud_nombre = len_nombre;
        s->como.clase.superclases = supers;
        s->como.clase.n_superclases = n_supers;
        s->como.clase.cuerpo = cuerpo;
    }
    return s;
}

Sent *sent_intentar(Arena *a, Sent *cuerpo,
                    ClausulaAtrapar *atrapadores, int n_atrapadores,
                    Sent *sino, Sent *finalmente,
                    int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_INTENTAR, linea, col);
    if (s) {
        s->como.intentar.cuerpo = cuerpo;
        s->como.intentar.atrapadores = atrapadores;
        s->como.intentar.n_atrapadores = n_atrapadores;
        s->como.intentar.sino = sino;
        s->como.intentar.finalmente = finalmente;
    }
    return s;
}

Sent *sent_lanzar(Arena *a, Expr *valor, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_LANZAR, linea, col);
    if (s) s->como.lanzar.valor = valor;
    return s;
}

Sent *sent_importar(Arena *a, Nombre *segmentos, int n_segmentos,
                    Nombre alias, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_IMPORTAR, linea, col);
    if (s) {
        s->como.importar.segmentos = segmentos;
        s->como.importar.n_segmentos = n_segmentos;
        s->como.importar.alias = alias;
    }
    return s;
}

Sent *sent_desde_importar(Arena *a, Nombre *segmentos_modulo, int n_seg,
                           ItemImportado *items, int n_items,
                           bool importa_todo, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_DESDE_IMPORTAR, linea, col);
    if (s) {
        s->como.desde_importar.segmentos_modulo = segmentos_modulo;
        s->como.desde_importar.n_segmentos_modulo = n_seg;
        s->como.desde_importar.items = items;
        s->como.desde_importar.n_items = n_items;
        s->como.desde_importar.importa_todo = importa_todo;
    }
    return s;
}

Sent *sent_global(Arena *a, Nombre *nombres, int n_nombres, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_GLOBAL, linea, col);
    if (s) {
        s->como.global_o_nolocal.nombres = nombres;
        s->como.global_o_nolocal.n_nombres = n_nombres;
    }
    return s;
}

Sent *sent_nolocal(Arena *a, Nombre *nombres, int n_nombres, int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_NOLOCAL, linea, col);
    if (s) {
        s->como.global_o_nolocal.nombres = nombres;
        s->como.global_o_nolocal.n_nombres = n_nombres;
    }
    return s;
}

Sent *sent_coincidir(Arena *a, Expr *sujeto,
                     ClausulaCuando *clausulas, int n_clausulas,
                     int linea, int col) {
    Sent *s = nuevo_sent(a, SENT_COINCIDIR, linea, col);
    if (s) {
        s->como.coincidir.sujeto = sujeto;
        s->como.coincidir.clausulas = clausulas;
        s->como.coincidir.n_clausulas = n_clausulas;
    }
    return s;
}

/* ───── Patrones (v1.15) ───── */

static Patron *nuevo_patron(Arena *a, TipoPatron tipo, int linea, int col) {
    Patron *p = (Patron *)arena_alocar_cero(a, sizeof(Patron));
    if (!p) return NULL;
    p->tipo = tipo;
    p->linea = linea;
    p->columna = col;
    return p;
}

Patron *patron_wildcard(Arena *a, int linea, int col) {
    return nuevo_patron(a, PATRON_WILDCARD, linea, col);
}

Patron *patron_literal(Arena *a, Expr *lit, int linea, int col) {
    Patron *p = nuevo_patron(a, PATRON_LITERAL, linea, col);
    if (p) p->como.literal = lit;
    return p;
}

Patron *patron_bind(Arena *a, const char *nombre, int len, int linea, int col) {
    Patron *p = nuevo_patron(a, PATRON_BIND, linea, col);
    if (p) {
        p->como.bind.nombre = nombre;
        p->como.bind.longitud = len;
    }
    return p;
}

Patron *patron_tupla(Arena *a, Patron **elementos, int n, int linea, int col) {
    Patron *p = nuevo_patron(a, PATRON_TUPLA, linea, col);
    if (p) {
        p->como.estructural.elementos = elementos;
        p->como.estructural.n = n;
    }
    return p;
}

Patron *patron_lista(Arena *a, Patron **elementos, int n, int linea, int col) {
    Patron *p = nuevo_patron(a, PATRON_LISTA, linea, col);
    if (p) {
        p->como.estructural.elementos = elementos;
        p->como.estructural.n = n;
    }
    return p;
}

Patron *patron_or(Arena *a, Patron **alternativas, int n, int linea, int col) {
    Patron *p = nuevo_patron(a, PATRON_OR, linea, col);
    if (p) {
        p->como.estructural.elementos = alternativas;
        p->como.estructural.n = n;
    }
    return p;
}

Patron *patron_star_bind(Arena *a, const char *nombre, int len, int linea, int col) {
    Patron *p = nuevo_patron(a, PATRON_STAR_BIND, linea, col);
    if (p) {
        p->como.bind.nombre = nombre;
        p->como.bind.longitud = len;
    }
    return p;
}

Patron *patron_tipo(Arena *a, const char *nombre, int len, int linea, int col) {
    Patron *p = nuevo_patron(a, PATRON_TIPO, linea, col);
    if (p) {
        /* Reusamos el union .bind para el nombre de la clase. */
        p->como.bind.nombre = nombre;
        p->como.bind.longitud = len;
    }
    return p;
}

/* ══════════════════════════════════════════════════════════════════
 * Sentencias — pretty-printer
 *
 * Formato S-expression coherente con el de expresiones:
 *   (sent-expr <expr>)
 *   (asignar <destino> <valor>)
 *   (asignar-aug "+=" <destino> <valor>)
 *   (pasar) (romper) (continuar)
 *   (retornar [<expr>])
 *   (si (rama <cond> <bloque>) (rama <cond> <bloque>) ... (rama nulo <bloque>))
 *   (mientras <cond> <cuerpo> [<sino>])
 *   (para <objetivo> <iterable> <cuerpo> [<sino>])
 *   (bloque <s1> <s2> ...)
 * ══════════════════════════════════════════════════════════════════ */

static const char *nombre_op_aug(TipoToken t) {
    switch (t) {
        case TT_ASIGNAR_MAS:         return "+=";
        case TT_ASIGNAR_MENOS:       return "-=";
        case TT_ASIGNAR_ASTERISCO:   return "*=";
        case TT_ASIGNAR_BARRA:       return "/=";
        case TT_ASIGNAR_DOBLE_BARRA: return "//=";
        case TT_ASIGNAR_PORCENTAJE:  return "%=";
        case TT_ASIGNAR_DOBLE_ASTER: return "**=";
        default: return "?=";
    }
}

static void sent_a_buffer(const Sent *s, EscrituraBuffer *eb);

static void sent_a_buffer(const Sent *s, EscrituraBuffer *eb) {
    if (s == NULL) { wb_escribir(eb, "(null)"); return; }
    switch (s->tipo) {
        case SENT_EXPR:
            wb_escribir(eb, "(sent-expr ");
            expr_a_buffer(s->como.expr.expr, eb);
            wb_escribir(eb, ")");
            break;
        case SENT_ASIGNAR:
            wb_escribir(eb, "(asignar ");
            expr_a_buffer(s->como.asignar.destino, eb);
            wb_escribir(eb, " ");
            expr_a_buffer(s->como.asignar.valor, eb);
            wb_escribir(eb, ")");
            break;
        case SENT_ASIGNAR_AUG:
            wb_escribir(eb, "(asignar-aug \"%s\" ", nombre_op_aug(s->como.asignar_aug.op));
            expr_a_buffer(s->como.asignar_aug.destino, eb);
            wb_escribir(eb, " ");
            expr_a_buffer(s->como.asignar_aug.valor, eb);
            wb_escribir(eb, ")");
            break;
        case SENT_PASAR:     wb_escribir(eb, "(pasar)"); break;
        case SENT_ROMPER:    wb_escribir(eb, "(romper)"); break;
        case SENT_CONTINUAR: wb_escribir(eb, "(continuar)"); break;
        case SENT_RETORNAR:
            if (s->como.retornar.valor) {
                wb_escribir(eb, "(retornar ");
                expr_a_buffer(s->como.retornar.valor, eb);
                wb_escribir(eb, ")");
            } else {
                wb_escribir(eb, "(retornar)");
            }
            break;
        case SENT_SI:
            wb_escribir(eb, "(si");
            for (int i = 0; i < s->como.si.n_ramas; i++) {
                RamaSi *r = &s->como.si.ramas[i];
                wb_escribir(eb, " (rama ");
                if (r->condicion) {
                    expr_a_buffer(r->condicion, eb);
                } else {
                    wb_escribir(eb, "nulo");
                }
                wb_escribir(eb, " ");
                sent_a_buffer(r->cuerpo, eb);
                wb_escribir(eb, ")");
            }
            wb_escribir(eb, ")");
            break;
        case SENT_MIENTRAS:
            wb_escribir(eb, "(mientras ");
            expr_a_buffer(s->como.mientras.condicion, eb);
            wb_escribir(eb, " ");
            sent_a_buffer(s->como.mientras.cuerpo, eb);
            if (s->como.mientras.sino) {
                wb_escribir(eb, " ");
                sent_a_buffer(s->como.mientras.sino, eb);
            }
            wb_escribir(eb, ")");
            break;
        case SENT_PARA:
            wb_escribir(eb, "(para ");
            expr_a_buffer(s->como.para.objetivo, eb);
            wb_escribir(eb, " ");
            expr_a_buffer(s->como.para.iterable, eb);
            wb_escribir(eb, " ");
            sent_a_buffer(s->como.para.cuerpo, eb);
            if (s->como.para.sino) {
                wb_escribir(eb, " ");
                sent_a_buffer(s->como.para.sino, eb);
            }
            wb_escribir(eb, ")");
            break;
        case SENT_BLOQUE:
            wb_escribir(eb, "(bloque");
            for (int i = 0; i < s->como.bloque.n_sentencias; i++) {
                wb_escribir(eb, " ");
                sent_a_buffer(s->como.bloque.sentencias[i], eb);
            }
            wb_escribir(eb, ")");
            break;
        case SENT_FUNCION:
            wb_escribir(eb, "(funcion ");
            wb_escribir_lexema(eb, s->como.funcion.nombre,
                               s->como.funcion.longitud_nombre);
            for (int i = 0; i < s->como.funcion.n_parametros; i++) {
                Parametro *par = &s->como.funcion.parametros[i];
                wb_escribir(eb, " (param ");
                wb_escribir_lexema(eb, par->nombre, par->longitud_nombre);
                if (par->anotacion_tipo) {
                    wb_escribir(eb, " (tipo ");
                    expr_a_buffer(par->anotacion_tipo, eb);
                    wb_escribir(eb, ")");
                }
                if (par->valor_defecto) {
                    wb_escribir(eb, " (defecto ");
                    expr_a_buffer(par->valor_defecto, eb);
                    wb_escribir(eb, ")");
                }
                wb_escribir(eb, ")");
            }
            if (s->como.funcion.anotacion_retorno) {
                wb_escribir(eb, " (retorno ");
                expr_a_buffer(s->como.funcion.anotacion_retorno, eb);
                wb_escribir(eb, ")");
            }
            wb_escribir(eb, " ");
            sent_a_buffer(s->como.funcion.cuerpo, eb);
            wb_escribir(eb, ")");
            break;
        case SENT_CLASE:
            wb_escribir(eb, "(clase ");
            wb_escribir_lexema(eb, s->como.clase.nombre,
                               s->como.clase.longitud_nombre);
            if (s->como.clase.n_superclases > 0) {
                wb_escribir(eb, " (extiende");
                for (int i = 0; i < s->como.clase.n_superclases; i++) {
                    wb_escribir(eb, " ");
                    expr_a_buffer(s->como.clase.superclases[i], eb);
                }
                wb_escribir(eb, ")");
            }
            wb_escribir(eb, " ");
            sent_a_buffer(s->como.clase.cuerpo, eb);
            wb_escribir(eb, ")");
            break;

        case SENT_INTENTAR:
            wb_escribir(eb, "(intentar ");
            sent_a_buffer(s->como.intentar.cuerpo, eb);
            for (int i = 0; i < s->como.intentar.n_atrapadores; i++) {
                ClausulaAtrapar *ca = &s->como.intentar.atrapadores[i];
                wb_escribir(eb, " (atrapar ");
                if (ca->tipo) {
                    expr_a_buffer(ca->tipo, eb);
                } else {
                    wb_escribir(eb, "nulo");
                }
                if (ca->alias.texto) {
                    wb_escribir(eb, " (alias ");
                    wb_escribir_lexema(eb, ca->alias.texto, ca->alias.longitud);
                    wb_escribir(eb, ")");
                }
                wb_escribir(eb, " ");
                sent_a_buffer(ca->cuerpo, eb);
                wb_escribir(eb, ")");
            }
            if (s->como.intentar.sino) {
                wb_escribir(eb, " (sino ");
                sent_a_buffer(s->como.intentar.sino, eb);
                wb_escribir(eb, ")");
            }
            if (s->como.intentar.finalmente) {
                wb_escribir(eb, " (finalmente ");
                sent_a_buffer(s->como.intentar.finalmente, eb);
                wb_escribir(eb, ")");
            }
            wb_escribir(eb, ")");
            break;

        case SENT_LANZAR:
            if (s->como.lanzar.valor) {
                wb_escribir(eb, "(lanzar ");
                expr_a_buffer(s->como.lanzar.valor, eb);
                wb_escribir(eb, ")");
            } else {
                wb_escribir(eb, "(lanzar)");
            }
            break;

        case SENT_IMPORTAR:
            wb_escribir(eb, "(importar ");
            for (int i = 0; i < s->como.importar.n_segmentos; i++) {
                if (i > 0) wb_escribir(eb, ".");
                wb_escribir_lexema(eb, s->como.importar.segmentos[i].texto,
                                   s->como.importar.segmentos[i].longitud);
            }
            if (s->como.importar.alias.texto) {
                wb_escribir(eb, " (alias ");
                wb_escribir_lexema(eb, s->como.importar.alias.texto,
                                   s->como.importar.alias.longitud);
                wb_escribir(eb, ")");
            }
            wb_escribir(eb, ")");
            break;

        case SENT_DESDE_IMPORTAR:
            wb_escribir(eb, "(desde ");
            for (int i = 0; i < s->como.desde_importar.n_segmentos_modulo; i++) {
                if (i > 0) wb_escribir(eb, ".");
                wb_escribir_lexema(eb,
                    s->como.desde_importar.segmentos_modulo[i].texto,
                    s->como.desde_importar.segmentos_modulo[i].longitud);
            }
            wb_escribir(eb, " importar");
            if (s->como.desde_importar.importa_todo) {
                wb_escribir(eb, " *");
            } else {
                for (int i = 0; i < s->como.desde_importar.n_items; i++) {
                    ItemImportado *it = &s->como.desde_importar.items[i];
                    wb_escribir(eb, " (item ");
                    wb_escribir_lexema(eb, it->nombre.texto, it->nombre.longitud);
                    if (it->alias.texto) {
                        wb_escribir(eb, " (alias ");
                        wb_escribir_lexema(eb, it->alias.texto, it->alias.longitud);
                        wb_escribir(eb, ")");
                    }
                    wb_escribir(eb, ")");
                }
            }
            wb_escribir(eb, ")");
            break;

        case SENT_GLOBAL:
        case SENT_NOLOCAL: {
            const char *etiqueta = (s->tipo == SENT_GLOBAL) ? "global" : "nolocal";
            wb_escribir(eb, "(%s", etiqueta);
            for (int i = 0; i < s->como.global_o_nolocal.n_nombres; i++) {
                wb_escribir(eb, " ");
                wb_escribir_lexema(eb, s->como.global_o_nolocal.nombres[i].texto,
                                   s->como.global_o_nolocal.nombres[i].longitud);
            }
            wb_escribir(eb, ")");
            break;
        }

        case SENT_COINCIDIR:
            wb_escribir(eb, "(coincidir ");
            expr_a_buffer(s->como.coincidir.sujeto, eb);
            for (int i = 0; i < s->como.coincidir.n_clausulas; i++) {
                ClausulaCuando *cw = &s->como.coincidir.clausulas[i];
                wb_escribir(eb, " (cuando ");
                {
                    /* Pretty-print del patrón. Para v1.16 estructurales,
                       solo nombre y conteo (no recurrimos por simplicidad). */
                    const Patron *pp = cw->patron;
                    switch (pp->tipo) {
                        case PATRON_WILDCARD:
                            wb_escribir(eb, "_");
                            break;
                        case PATRON_LITERAL:
                            expr_a_buffer(pp->como.literal, eb);
                            break;
                        case PATRON_BIND:
                            wb_escribir(eb, "(bind ");
                            wb_escribir_lexema(eb, pp->como.bind.nombre,
                                                pp->como.bind.longitud);
                            wb_escribir(eb, ")");
                            break;
                        case PATRON_TUPLA:
                            wb_escribir(eb, "(tupla %d)", pp->como.estructural.n);
                            break;
                        case PATRON_LISTA:
                            wb_escribir(eb, "(lista %d)", pp->como.estructural.n);
                            break;
                        case PATRON_OR:
                            wb_escribir(eb, "(or %d)", pp->como.estructural.n);
                            break;
                        case PATRON_STAR_BIND:
                            wb_escribir(eb, "(*bind ");
                            wb_escribir_lexema(eb, pp->como.bind.nombre,
                                                pp->como.bind.longitud);
                            wb_escribir(eb, ")");
                            break;
                        case PATRON_TIPO:
                            wb_escribir(eb, "(tipo ");
                            wb_escribir_lexema(eb, pp->como.bind.nombre,
                                                pp->como.bind.longitud);
                            wb_escribir(eb, ")");
                            break;
                    }
                    if (cw->bind_completo_texto != NULL) {
                        wb_escribir(eb, " (como ");
                        wb_escribir_lexema(eb, cw->bind_completo_texto,
                                            cw->bind_completo_longitud);
                        wb_escribir(eb, ")");
                    }
                }
                if (cw->guarda) {
                    wb_escribir(eb, " (si ");
                    expr_a_buffer(cw->guarda, eb);
                    wb_escribir(eb, ")");
                }
                wb_escribir(eb, " ");
                sent_a_buffer(cw->cuerpo, eb);
                wb_escribir(eb, ")");
            }
            wb_escribir(eb, ")");
            break;
    }
}

void sent_imprimir(const Sent *s, FILE *out) {
    /* Implementación simple via buffer pequeño con re-impresión.
       Para volumen real usaríamos buffer streaming, pero aquí es OK. */
    char buffer[8192];
    sent_a_cadena(s, buffer, sizeof(buffer));
    fputs(buffer, out);
}

int sent_a_cadena(const Sent *s, char *buffer, int capacidad) {
    if (capacidad <= 0) return 0;
    EscrituraBuffer eb = { buffer, capacidad, 0 };
    sent_a_buffer(s, &eb);
    if (eb.usado >= eb.capacidad) eb.usado = eb.capacidad - 1;
    buffer[eb.usado] = '\0';
    return eb.usado;
}
