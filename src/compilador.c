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

/* Inicializa un scope (raíz o de función anidada). Para scopes de
   función, el slot 0 se reserva para el callee (convención compartida
   con la VM en OP_LLAMAR). */
static void scope_iniciar(ScopeCompilador *s, Chunk *chunk, bool es_funcion,
                           ScopeCompilador *padre) {
    s->chunk = chunk;
    s->es_funcion = es_funcion;
    s->n_locales = 0;
    s->n_upvalues = 0;
    s->n_bucles = 0;
    /* v1.40 fix: `n_nolocales` quedaba SIN inicializar — basura de
       stack. Con -O0/-O2 solía caer en 0 por suerte, pero -O3+LTO lo
       destapó: el contador arrancaba en un valor alto y disparaba
       "demasiadas declaraciones nolocal" (o corrompía el array index
       → segfault). UB latente desde que se añadió `nolocal` (v1.4). */
    s->n_nolocales = 0;
    s->n_globales = 0;
    s->funcion = NULL;
    s->padre = padre;
    if (es_funcion) {
        s->locales[0].nombre = "";
        s->locales[0].longitud_nombre = 0;
        s->locales[0].capturado = false;
        s->n_locales = 1;
    }
}

void compilador_iniciar(Compilador *c, Chunk *chunk) {
    scope_iniciar(&c->raiz, chunk, false, NULL);
    c->actual = &c->raiz;
    c->error.tuvo_error = false;
    c->error.mensaje[0] = '\0';
    c->error.linea = 0;
    c->error.columna = 0;
    c->n_atrapadores_activos = 0;
}

/* Busca un local por nombre en el scope actual. Devuelve el slot
   (>=0) si existe, -1 si no. */
static int buscar_local(const ScopeCompilador *s, const char *nombre, int len) {
    for (int i = s->n_locales - 1; i >= 0; i--) {
        if (s->locales[i].longitud_nombre == len
            && memcmp(s->locales[i].nombre, nombre, (size_t)len) == 0) {
            return i;
        }
    }
    return -1;
}

/* v1.57: true si `nombre` fue declarado `global` en el scope actual
 * (vía `SENT_GLOBAL`). Las asignaciones y lecturas posteriores deben
 * dirigirse al scope de modulo en lugar de local/upvalue. */
static bool es_global_declarado(const ScopeCompilador *s,
                                  const char *nombre, int len) {
    for (int i = 0; i < s->n_globales; i++) {
        if (s->globales[i].longitud_nombre == len
            && memcmp(s->globales[i].nombre, nombre, (size_t)len) == 0) {
            return true;
        }
    }
    return false;
}

/* Añade un local en el scope actual y devuelve su slot. Reporta error
   si excede COMPILADOR_LOCALES_MAX. */
static int agregar_local(Compilador *c, const char *nombre, int len, int linea) {
    ScopeCompilador *s = c->actual;
    if (s->n_locales >= COMPILADOR_LOCALES_MAX) {
        c->error.tuvo_error = true;
        c->error.linea = linea;
        snprintf(c->error.mensaje, sizeof(c->error.mensaje),
            "demasiadas variables locales en una funcion (>%d)",
            COMPILADOR_LOCALES_MAX);
        return -1;
    }
    int slot = s->n_locales;
    s->locales[slot].nombre = nombre;
    s->locales[slot].longitud_nombre = len;
    s->locales[slot].capturado = false;
    s->n_locales++;
    return slot;
}

/*
 * Añade un upvalue al scope dado y devuelve su índice. Si ya existe
 * uno equivalente (mismo es_local + mismo índice), devuelve el
 * existente para evitar duplicados.
 */
static int agregar_upvalue(Compilador *c, ScopeCompilador *s,
                            bool es_local, uint8_t indice, int linea) {
    for (int i = 0; i < s->n_upvalues; i++) {
        if (s->upvalues[i].es_local == es_local
            && s->upvalues[i].indice == indice) {
            return i;
        }
    }
    if (s->n_upvalues >= COMPILADOR_UPVALUES_MAX) {
        c->error.tuvo_error = true;
        c->error.linea = linea;
        snprintf(c->error.mensaje, sizeof(c->error.mensaje),
            "demasiados upvalues en una funcion (>%d)",
            COMPILADOR_UPVALUES_MAX);
        return -1;
    }
    int idx = s->n_upvalues;
    s->upvalues[idx].es_local = es_local;
    s->upvalues[idx].indice = indice;
    s->n_upvalues++;
    /* Reflejar en la FuncionBC para que OP_CLOSURE en runtime tenga
       acceso al conteo. La metadata real se emite inline en el
       chunk del scope padre. */
    if (s->funcion) {
        s->funcion->info_upvalues[idx].es_local = es_local;
        s->funcion->info_upvalues[idx].indice = indice;
        s->funcion->n_upvalues = s->n_upvalues;
    }
    return idx;
}

/*
 * Resuelve un identificador en una cadena de scopes: busca como
 * upvalue en el scope dado (recursivamente subiendo a padres). Si
 * encuentra una local en algún ancestro, marca esa local como
 * `capturado` y registra un upvalue en cada scope intermedio.
 *
 * Devuelve el índice del upvalue en `s` si lo encuentra; -1 si no
 * existe en ningún ancestro.
 */
static int resolver_upvalue(Compilador *c, ScopeCompilador *s,
                             const char *nombre, int len, int linea) {
    if (s->padre == NULL) return -1;

    /* Buscar como local en el padre directo. */
    for (int i = s->padre->n_locales - 1; i >= 0; i--) {
        if (s->padre->locales[i].longitud_nombre == len
            && memcmp(s->padre->locales[i].nombre, nombre, (size_t)len) == 0) {
            s->padre->locales[i].capturado = true;
            return agregar_upvalue(c, s, true, (uint8_t)i, linea);
        }
    }

    /* Si no, buscar en upvalue del padre (recursión). */
    int idx_padre = resolver_upvalue(c, s->padre, nombre, len, linea);
    if (idx_padre >= 0) {
        return agregar_upvalue(c, s, false, (uint8_t)idx_padre, linea);
    }
    return -1;
}

/* ──────────────────────────────────────────────────────────────────
 * Helpers de saltos
 *
 * `emitir_salto`: emite un opcode con un operando u16 placeholder
 * (0xffff). Devuelve el offset al primer byte del placeholder, que se
 * pasará a `parchear_salto` cuando conozcamos el destino.
 *
 * `parchear_salto`: rellena el placeholder con la distancia desde el
 * byte siguiente al placeholder hasta la posición actual del chunk
 * (cuántos bytes saltar hacia adelante).
 *
 * `emitir_bucle`: emite OP_BUCLE con offset hacia atrás calculado
 * desde el byte siguiente al operando hasta `inicio`.
 * ────────────────────────────────────────────────────────────────── */

static int emitir_salto(Compilador *c, OpCode op, int linea) {
    chunk_emitir_byte(c->actual->chunk, (uint8_t)op, linea);
    chunk_emitir_byte(c->actual->chunk, 0xff, linea);
    chunk_emitir_byte(c->actual->chunk, 0xff, linea);
    return c->actual->chunk->cuenta - 2;   /* offset del primer byte del placeholder */
}

static void parchear_salto(Compilador *c, int offset_placeholder, int linea) {
    int salto = c->actual->chunk->cuenta - offset_placeholder - 2;
    if (salto > UINT16_MAX) {
        c->error.tuvo_error = true;
        c->error.linea = linea;
        snprintf(c->error.mensaje, sizeof(c->error.mensaje),
            "salto demasiado grande para u16 (>%u bytes)", UINT16_MAX);
        return;
    }
    c->actual->chunk->codigo[offset_placeholder]     = (uint8_t)((salto >> 8) & 0xff);
    c->actual->chunk->codigo[offset_placeholder + 1] = (uint8_t)(salto & 0xff);
}

static void emitir_bucle(Compilador *c, int inicio, int linea) {
    chunk_emitir_byte(c->actual->chunk, OP_BUCLE, linea);
    int offset = c->actual->chunk->cuenta - inicio + 2;   /* +2 por el operando */
    if (offset > UINT16_MAX) {
        c->error.tuvo_error = true;
        c->error.linea = linea;
        snprintf(c->error.mensaje, sizeof(c->error.mensaje),
            "bucle demasiado grande para u16 (>%u bytes)", UINT16_MAX);
        return;
    }
    chunk_emitir_byte(c->actual->chunk, (uint8_t)((offset >> 8) & 0xff), linea);
    chunk_emitir_byte(c->actual->chunk, (uint8_t)(offset & 0xff), linea);
}

/* ──────────────────────────────────────────────────────────────────
 * Bucle stack (para romper/continuar)
 * ────────────────────────────────────────────────────────────────── */

static BucleAbierto *bucle_actual(Compilador *c) {
    if (c->actual->n_bucles == 0) return NULL;
    return &c->actual->bucles[c->actual->n_bucles - 1];
}

static bool empujar_bucle(Compilador *c, int inicio_continuar, int linea) {
    if (c->actual->n_bucles >= COMPILADOR_BUCLES_MAX) {
        c->error.tuvo_error = true;
        c->error.linea = linea;
        snprintf(c->error.mensaje, sizeof(c->error.mensaje),
            "anidamiento de bucles excede %d niveles", COMPILADOR_BUCLES_MAX);
        return false;
    }
    BucleAbierto *b = &c->actual->bucles[c->actual->n_bucles++];
    b->inicio_continuar = inicio_continuar;
    b->parches_romper = NULL;
    b->n_parches = 0;
    b->cap_parches = 0;
    return true;
}

static bool registrar_parche_romper(Compilador *c, int offset, int linea) {
    BucleAbierto *b = bucle_actual(c);
    if (!b) {
        c->error.tuvo_error = true;
        c->error.linea = linea;
        snprintf(c->error.mensaje, sizeof(c->error.mensaje),
            "'romper' fuera de un bucle");
        return false;
    }
    if (b->n_parches == b->cap_parches) {
        int nueva = b->cap_parches < 4 ? 4 : b->cap_parches * 2;
        int *p = (int *)realloc(b->parches_romper, sizeof(int) * (size_t)nueva);
        if (!p) {
            c->error.tuvo_error = true;
            c->error.linea = linea;
            snprintf(c->error.mensaje, sizeof(c->error.mensaje),
                "memoria insuficiente registrando 'romper'");
            return false;
        }
        b->parches_romper = p;
        b->cap_parches = nueva;
    }
    b->parches_romper[b->n_parches++] = offset;
    return true;
}

static void cerrar_bucle(Compilador *c, int linea) {
    BucleAbierto *b = bucle_actual(c);
    if (!b) return;
    /* Parchear cada `romper` al offset actual (final del bucle). */
    for (int i = 0; i < b->n_parches; i++) {
        parchear_salto(c, b->parches_romper[i], linea);
    }
    free(b->parches_romper);
    c->actual->n_bucles--;
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
        case TT_ES:              return OP_ES;
        case TT_EN:              return OP_EN;
        case TT_AMPERSAND:       return OP_BIT_Y;     /* v1.170 */
        case TT_BARRA_VERT:      return OP_BIT_O;     /* v1.170 */
        case TT_CIRCUNFLEJO:     return OP_BIT_XOR;   /* v1.170 */
        case TT_DESPL_IZQ:       return OP_DESPL_IZQ; /* v1.170 */
        case TT_DESPL_DER:       return OP_DESPL_DER; /* v1.170 */
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

/*
 * `cadena_desde_lexema` toma el lexema completo con comillas y
 * delega en `valor_cadena_desde_escapes` (declarado en valor.h)
 * que maneja la decodificación de escapes. El mismo helper se
 * comparte con el evaluador y con las partes literales de
 * `EXPR_LITERAL_F_CADENA` para garantizar comportamiento
 * idéntico entre ambos motores.
 */
static Valor cadena_desde_lexema(const char *lex, int len) {
    if (len < 2) return valor_cadena_referencia("", 0);
    /* v1.14: cadenas triples (`"""..."""` o `'''...'''`) — detectar
       6+ caracteres con prefijo de tres delimitadores iguales. */
    if (len >= 6 && (lex[0] == '"' || lex[0] == '\'')
                  && lex[1] == lex[0] && lex[2] == lex[0]) {
        return valor_cadena_desde_escapes(lex + 3, len - 6);
    }
    return valor_cadena_desde_escapes(lex + 1, len - 2);
}

/* ──────────────────────────────────────────────────────────────────
 * Constant folding (v0.11.3)
 *
 * Si una expresión binaria/unaria tiene operandos constantes (literales
 * o sub-expresiones constantes recursivas), la computamos en
 * compile-time y emitimos OP_CONST en lugar del bytecode aritmético.
 *
 * Cubre patrones comunes en código real:
 *   `1 + 2`, `2 ** 10`, `60 * 60 * 24` (constantes nombradas), etc.
 *
 * NO foldeamos si la operación produciría error (división por cero,
 * tipos incompatibles): dejamos que el runtime reporte el error con
 * la línea correcta, no en compile-time.
 * ────────────────────────────────────────────────────────────────── */

/*
 * Devuelve true y rellena *out con un Valor recién construido si `e`
 * es una expresión constante evaluable en compile-time. Toma posesión
 * del Valor — el llamador debe destruirlo.
 *
 * Soporta: literales (NULO, BOOLEANO, ENTERO, DECIMAL, CADENA), GRUPO
 * recursivo, UNARIO recursivo, BINARIO recursivo. NO soporta f-strings,
 * identificadores ni llamadas (esos no son constantes en compile-time).
 */
static bool evaluar_constante(const Expr *e, Valor *out) {
    if (!e) return false;
    switch (e->tipo) {
        case EXPR_LITERAL_NULO:
            *out = valor_nulo();
            return true;
        case EXPR_LITERAL_BOOLEANO:
            *out = valor_booleano(e->como.booleano.valor);
            return true;
        case EXPR_LITERAL_ENTERO:
            *out = valor_entero_de_lexema(e->como.literal.lexema,
                                            e->como.literal.longitud);
            return out->tipo != VAL_NULO;  /* false si lexema malformado */
        case EXPR_LITERAL_DECIMAL:
            *out = valor_decimal_de_lexema(e->como.literal.lexema,
                                             e->como.literal.longitud);
            return out->tipo != VAL_NULO;
        case EXPR_LITERAL_CADENA:
            *out = cadena_desde_lexema(e->como.literal.lexema,
                                         e->como.literal.longitud);
            return out->tipo != VAL_NULO;
        case EXPR_GRUPO:
            return evaluar_constante(e->como.grupo.interna, out);
        case EXPR_UNARIO: {
            Valor inner;
            if (!evaluar_constante(e->como.unario.operando, &inner)) return false;
            EvalError err = {0};
            *out = evaluador_aplicar_unario(&err,
                (int)e->como.unario.op, inner, e->linea, e->columna);
            if (err.tuvo_error) {
                /* `inner` fue consumido por evaluador_aplicar_unario.
                   Y `*out` puede contener basura — descartarlo. */
                valor_destruir(out);
                *out = valor_nulo();
                return false;
            }
            return true;
        }
        case EXPR_BINARIO: {
            Valor a, b;
            if (!evaluar_constante(e->como.binario.izq, &a)) return false;
            if (!evaluar_constante(e->como.binario.der, &b)) {
                valor_destruir(&a);
                return false;
            }
            EvalError err = {0};
            *out = evaluador_aplicar_binario(&err,
                (int)e->como.binario.op, a, b, e->linea, e->columna);
            if (err.tuvo_error) {
                valor_destruir(out);
                *out = valor_nulo();
                return false;
            }
            return true;
        }
        default:
            return false;
    }
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
    int idx = chunk_agregar_constante(c->actual->chunk, name);
    return idx;
}

/* v1.138: helpers de destructuring con patrones anidados para
 * comprehensions y genex. Definicion mas abajo; forward para usar
 * desde EXPR_COMPREHENSION en compilador_compilar_expr. */
static bool validar_patron_compr(Compilador *c, const Expr *patron,
                                  int linea, int col);
static int contar_slots_patron(const Expr *patron);
static bool prereservar_slots_patron_compr(Compilador *c, const Expr *patron,
                                              int linea);
static bool emitir_destruct_patron_compr(Compilador *c, int slot_item,
                                            const Expr *patron, int *cursor,
                                            int linea);

bool compilador_compilar_expr(Compilador *c, const Expr *e) {
    if (c->error.tuvo_error) return false;

    switch (e->tipo) {
        case EXPR_LITERAL_NULO:
            chunk_emitir_byte(c->actual->chunk, OP_NULO, e->linea);
            return true;

        case EXPR_LITERAL_BOOLEANO:
            chunk_emitir_byte(c->actual->chunk,
                e->como.booleano.valor ? OP_VERDADERO : OP_FALSO,
                e->linea);
            return true;

        case EXPR_LITERAL_ENTERO: {
            Valor v = valor_entero_de_lexema(e->como.literal.lexema,
                                               e->como.literal.longitud);
            chunk_emitir_constante(c->actual->chunk, v, e->linea);
            return true;
        }
        case EXPR_LITERAL_DECIMAL: {
            Valor v = valor_decimal_de_lexema(e->como.literal.lexema,
                                                e->como.literal.longitud);
            chunk_emitir_constante(c->actual->chunk, v, e->linea);
            return true;
        }
        case EXPR_LITERAL_CADENA: {
            Valor v = cadena_desde_lexema(e->como.literal.lexema,
                                            e->como.literal.longitud);
            chunk_emitir_constante(c->actual->chunk, v, e->linea);
            return true;
        }

        case EXPR_GRUPO:
            return compilador_compilar_expr(c, e->como.grupo.interna);

        case EXPR_BINARIO: {
            /* v0.11.3: constant folding. Si ambos lados se reducen a
               constantes, computamos el resultado en compile-time. */
            Valor folded;
            if (evaluar_constante(e, &folded)) {
                chunk_emitir_constante(c->actual->chunk, folded, e->linea);
                return true;
            }
            if (!compilador_compilar_expr(c, e->como.binario.izq)) return false;
            if (!compilador_compilar_expr(c, e->como.binario.der)) return false;
            int op = token_a_opcode_binario(e->como.binario.op);
            if (op < 0) {
                error_compilacion(c, e->linea, e->columna,
                    "operador binario no soportado en bytecode v0.6 sesion 2");
                return false;
            }
            chunk_emitir_byte(c->actual->chunk, (uint8_t)op, e->linea);
            return true;
        }

        case EXPR_UNARIO: {
            /* v0.11.3: constant folding para -3, no falso, +5, etc. */
            Valor folded;
            if (evaluar_constante(e, &folded)) {
                chunk_emitir_constante(c->actual->chunk, folded, e->linea);
                return true;
            }
            if (!compilador_compilar_expr(c, e->como.unario.operando)) return false;
            switch (e->como.unario.op) {
                case TT_MENOS:
                    chunk_emitir_byte(c->actual->chunk, OP_NEGAR, e->linea);
                    return true;
                case TT_NO:
                    chunk_emitir_byte(c->actual->chunk, OP_NO, e->linea);
                    return true;
                case TT_MAS:
                    /* v1.169: emitir OP_POSITIVO para que la VM despache
                       `__positivo__` en instancias. Para valores numericos
                       el opcode es un no-op trivial (overhead < 1ns). */
                    chunk_emitir_byte(c->actual->chunk, OP_POSITIVO, e->linea);
                    return true;
                case TT_TILDE_BIT:  /* v1.167 */
                    chunk_emitir_byte(c->actual->chunk, OP_TILDE_BIT, e->linea);
                    return true;
                default:
                    error_compilacion(c, e->linea, e->columna,
                        "operador unario no soportado en bytecode v0.6 sesion 2");
                    return false;
            }
        }

        case EXPR_IDENT: {
            /* v1.57: si fue declarada `global` en este scope, saltar
               directo a OP_OBTENER_GLOBAL ignorando locales/upvalues. */
            bool forzar_global = c->actual->es_funcion
                && es_global_declarado(c->actual,
                                        e->como.ident.nombre,
                                        e->como.ident.longitud);

            /* Prioridad: local del scope actual → upvalue (búsqueda
               recursiva en scopes padres) → global. */
            if (!forzar_global) {
                int slot = buscar_local(c->actual, e->como.ident.nombre,
                                           e->como.ident.longitud);
                if (slot >= 0) {
                    chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                                        (uint8_t)slot, e->linea);
                    return true;
                }
                int upv = resolver_upvalue(c, c->actual,
                                              e->como.ident.nombre,
                                              e->como.ident.longitud, e->linea);
                if (upv >= 0) {
                    chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_UPVALUE,
                                        (uint8_t)upv, e->linea);
                    return true;
                }
            }
            int idx = agregar_nombre_global(c, e->como.ident.nombre,
                                              e->como.ident.longitud);
            if (idx < 0 || idx > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "demasiadas constantes para v0.6 (operando byte)");
                return false;
            }
            /* OP_OBTENER_GLOBAL ahora ocupa 6 bytes (v0.10 / F10):
               opcode + name_idx + 4 bytes de cache (zero-init).
               El runtime los rellena en el primer hit y promueve el
               opcode a OP_OBTENER_GLOBAL_CACHE para hits subsecuentes. */
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_GLOBAL, (uint8_t)idx, e->linea);
            chunk_emitir_byte2(c->actual->chunk, 0, 0, e->linea);
            chunk_emitir_byte2(c->actual->chunk, 0, 0, e->linea);
            return true;
        }

        case EXPR_LLAMADA: {
            const Expr *callee = e->como.llamada.callee;
            int n_args = e->como.llamada.n_args;
            if (n_args > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "una llamada no puede tener mas de 255 argumentos");
                return false;
            }
            /* Caso especial `super.metodo(args)`:
             *   - empuja `yo` (slot 1 del frame del método).
             *   - empuja args.
             *   - emite OP_SUPER_INVOCAR [name_idx] [n_args].
             * Solo válido dentro de un método (scope de función). */
            if (callee->tipo == EXPR_SUPER) {
                if (!c->actual->es_funcion) {
                    error_compilacion(c, e->linea, e->columna,
                        "'super' solo puede usarse dentro de un metodo");
                    return false;
                }
                /* OP_OBTENER_LOCAL 1 → empuja `yo` (primer parametro). */
                chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                                    1, callee->linea);
                for (int i = 0; i < n_args; i++) {
                    if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
                }
                int idx = chunk_agregar_constante(c->actual->chunk,
                    valor_cadena_duplicar(callee->como.super.nombre,
                                            callee->como.super.longitud));
                if (idx < 0 || idx > 255) {
                    error_compilacion(c, e->linea, e->columna,
                        "demasiadas constantes para v0.7 (operando byte)");
                    return false;
                }
                chunk_emitir_byte(c->actual->chunk, OP_SUPER_INVOCAR, e->linea);
                chunk_emitir_byte(c->actual->chunk, (uint8_t)idx, e->linea);
                chunk_emitir_byte(c->actual->chunk, (uint8_t)n_args, e->linea);
                return true;
            }
            /* Caso especial built-in `imprimir(...)`: emitimos OP_IMPRIMIR
               directamente para evitar tener que registrar `imprimir`
               como global y poder llamarlo desde el top-level. */
            bool es_imprimir =
                callee->tipo == EXPR_IDENT
                && callee->como.ident.longitud == 8
                && memcmp(callee->como.ident.nombre, "imprimir", 8) == 0;
            if (es_imprimir
                && buscar_local(c->actual, "imprimir", 8) < 0) {
                for (int i = 0; i < n_args; i++) {
                    if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
                    /* v1.2: coerce cada arg a cadena via OP_FORMATO_F.
                     * Esto invoca `__cadena__` si el arg es una
                     * instancia con dunder definido; en otro caso usa
                     * `valor_a_cadena_alocada`. OP_ASEGURAR_CADENA
                     * valida que el resultado sea cadena. */
                    chunk_emitir_byte(c->actual->chunk, OP_FORMATO_F, e->linea);
                    chunk_emitir_byte(c->actual->chunk, OP_ASEGURAR_CADENA, e->linea);
                }
                chunk_emitir_byte2(c->actual->chunk, OP_IMPRIMIR,
                                   (uint8_t)n_args, e->linea);
                return true;
            }
            /* v1.3: atajo `longitud(arg)` → OP_LONGITUD. Despacha a
             * `__longitud__` si el arg es instancia con dunder, o a la
             * lógica de la nativa para tipos primitivos. */
            bool es_longitud =
                callee->tipo == EXPR_IDENT
                && callee->como.ident.longitud == 8
                && memcmp(callee->como.ident.nombre, "longitud", 8) == 0
                && n_args == 1
                && buscar_local(c->actual, "longitud", 8) < 0;
            if (es_longitud) {
                if (!compilador_compilar_expr(c, e->como.llamada.args[0])) return false;
                chunk_emitir_byte(c->actual->chunk, OP_LONGITUD, e->linea);
                return true;
            }
            /* v1.3: atajo `cadena(arg)` → OP_FORMATO_F + OP_ASEGURAR_CADENA.
             * Hace que `cadena(obj)` invoque `__cadena__` consistentemente
             * con f-strings e `imprimir`. La nativa `cadena` queda como
             * fallback indirecto (`f = cadena; f(x)` no pasa por el
             * atajo). */
            bool es_cadena =
                callee->tipo == EXPR_IDENT
                && callee->como.ident.longitud == 6
                && memcmp(callee->como.ident.nombre, "cadena", 6) == 0
                && n_args == 1
                && buscar_local(c->actual, "cadena", 6) < 0;
            if (es_cadena) {
                if (!compilador_compilar_expr(c, e->como.llamada.args[0])) return false;
                chunk_emitir_byte(c->actual->chunk, OP_FORMATO_F, e->linea);
                chunk_emitir_byte(c->actual->chunk, OP_ASEGURAR_CADENA, e->linea);
                return true;
            }
            /* v1.41: atajo `repr(arg)` → OP_REPR + OP_ASEGURAR_CADENA.
             * Hace que `repr(obj)` invoque `__repr__` cuando la clase lo
             * define. La nativa `repr` queda como fallback indirecto
             * (vía `f = repr; f(x)`) y conserva el comportamiento sin
             * dunder. */
            bool es_repr =
                callee->tipo == EXPR_IDENT
                && callee->como.ident.longitud == 4
                && memcmp(callee->como.ident.nombre, "repr", 4) == 0
                && n_args == 1
                && buscar_local(c->actual, "repr", 4) < 0;
            if (es_repr) {
                if (!compilador_compilar_expr(c, e->como.llamada.args[0])) return false;
                chunk_emitir_byte(c->actual->chunk, OP_REPR, e->linea);
                chunk_emitir_byte(c->actual->chunk, OP_ASEGURAR_CADENA, e->linea);
                return true;
            }
            /* v1.22: si la llamada tiene algún `*expr` (spread arg),
               construimos una lista runtime con todos los args
               expandidos y usamos OP_LLAMAR_SPREAD. */
            bool tiene_spread = (e->como.llamada.args_spread != NULL);
            bool tiene_kwargs = (e->como.llamada.kwarg_keys != NULL);
            bool tiene_dspread = (e->como.llamada.args_doble_spread != NULL);
            /* v1.46: si combinamos `*args` con kwargs/`**dict`, emitimos
               OP_LLAMAR_SPREAD_KW_DICT que construye una lista (args
               posicionales) y un dict (kwargs) y los pasa juntos. */
            if (tiene_spread && (tiene_kwargs || tiene_dspread)) {
                if (!compilador_compilar_expr(c, callee)) return false;
                /* Construir lista de posicionales (incluye `*spread`). */
                chunk_emitir_byte2(c->actual->chunk, OP_BUILD_LISTA, 0, e->linea);
                for (int i = 0; i < n_args; i++) {
                    bool es_kw = (e->como.llamada.kwarg_keys
                                  && e->como.llamada.kwarg_keys[i] != NULL);
                    bool es_dsp = (e->como.llamada.args_doble_spread
                                   && e->como.llamada.args_doble_spread[i]);
                    bool es_sp = (e->como.llamada.args_spread
                                  && e->como.llamada.args_spread[i]);
                    if (es_kw || es_dsp) continue;
                    if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
                    if (es_sp) {
                        chunk_emitir_byte(c->actual->chunk, OP_LISTA_EXTENDER, e->linea);
                    } else {
                        chunk_emitir_byte(c->actual->chunk, OP_LISTA_AGREGAR, e->linea);
                    }
                }
                /* Construir dict de kwargs (incluye `**dspread`). */
                chunk_emitir_byte2(c->actual->chunk, OP_BUILD_DICC, 0, e->linea);
                for (int i = 0; i < n_args; i++) {
                    bool es_kw = (e->como.llamada.kwarg_keys
                                  && e->como.llamada.kwarg_keys[i] != NULL);
                    bool es_dsp = (e->como.llamada.args_doble_spread
                                   && e->como.llamada.args_doble_spread[i]);
                    if (!es_kw && !es_dsp) continue;
                    if (es_kw) {
                        const char *k = e->como.llamada.kwarg_keys[i];
                        int klen = e->como.llamada.kwarg_lens[i];
                        Valor v_clave = valor_cadena_duplicar(k, klen);
                        int idx_const = chunk_agregar_constante(c->actual->chunk, v_clave);
                        if (idx_const < 0 || idx_const > 255) {
                            error_compilacion(c, e->linea, e->columna,
                                "demasiadas constantes");
                            return false;
                        }
                        chunk_emitir_byte2(c->actual->chunk, OP_CONST,
                                            (uint8_t)idx_const, e->linea);
                        if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
                        chunk_emitir_byte(c->actual->chunk, OP_DICC_AGREGAR_PAR, e->linea);
                    } else {  /* **dspread */
                        if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
                        chunk_emitir_byte(c->actual->chunk, OP_DICC_EXTENDER, e->linea);
                    }
                }
                chunk_emitir_byte(c->actual->chunk, OP_LLAMAR_SPREAD_KW_DICT, e->linea);
                return true;
            }
            if (tiene_spread) {
                if (!compilador_compilar_expr(c, callee)) return false;
                /* Lista vacía sobre la que iremos acumulando args. */
                chunk_emitir_byte2(c->actual->chunk, OP_BUILD_LISTA, 0, e->linea);
                for (int i = 0; i < n_args; i++) {
                    if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
                    if (e->como.llamada.args_spread[i]) {
                        chunk_emitir_byte(c->actual->chunk, OP_LISTA_EXTENDER, e->linea);
                    } else {
                        chunk_emitir_byte(c->actual->chunk, OP_LISTA_AGREGAR, e->linea);
                    }
                }
                chunk_emitir_byte(c->actual->chunk, OP_LLAMAR_SPREAD, e->linea);
                return true;
            }
            /* v1.25: llamada con `**dict` spread (con o sin kwargs
               explícitos). Construye un dict runtime con todos los
               kwargs y usa OP_LLAMAR_KW_DICT. */
            if (tiene_dspread) {
                int n_pos = 0;
                for (int i = 0; i < n_args; i++) {
                    bool es_kw = (e->como.llamada.kwarg_keys
                                  && e->como.llamada.kwarg_keys[i] != NULL);
                    bool es_dsp = e->como.llamada.args_doble_spread[i];
                    if (!es_kw && !es_dsp) n_pos++;
                }
                if (n_pos > 255) {
                    error_compilacion(c, e->linea, e->columna,
                        "demasiados posicionales (max 255)");
                    return false;
                }
                if (!compilador_compilar_expr(c, callee)) return false;
                /* Posicionales primero. */
                for (int i = 0; i < n_args; i++) {
                    bool es_kw = (e->como.llamada.kwarg_keys
                                  && e->como.llamada.kwarg_keys[i] != NULL);
                    bool es_dsp = e->como.llamada.args_doble_spread[i];
                    if (es_kw || es_dsp) continue;
                    if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
                }
                /* Dict vacío que recibirá los kwargs. */
                chunk_emitir_byte2(c->actual->chunk, OP_BUILD_DICC, 0, e->linea);
                /* En orden de aparición, kwargs y dspreads alimentan el dict. */
                for (int i = 0; i < n_args; i++) {
                    bool es_kw = (e->como.llamada.kwarg_keys
                                  && e->como.llamada.kwarg_keys[i] != NULL);
                    bool es_dsp = e->como.llamada.args_doble_spread[i];
                    if (!es_kw && !es_dsp) continue;
                    if (es_kw) {
                        const char *k = e->como.llamada.kwarg_keys[i];
                        int klen = e->como.llamada.kwarg_lens[i];
                        Valor v_clave = valor_cadena_duplicar(k, klen);
                        int idx_const = chunk_agregar_constante(c->actual->chunk, v_clave);
                        if (idx_const < 0 || idx_const > 255) {
                            error_compilacion(c, e->linea, e->columna,
                                "demasiadas constantes");
                            return false;
                        }
                        chunk_emitir_byte2(c->actual->chunk, OP_CONST,
                                            (uint8_t)idx_const, e->linea);
                        if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
                        chunk_emitir_byte(c->actual->chunk, OP_DICC_AGREGAR_PAR, e->linea);
                    } else { /* es_dsp */
                        if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
                        chunk_emitir_byte(c->actual->chunk, OP_DICC_EXTENDER, e->linea);
                    }
                }
                chunk_emitir_byte2(c->actual->chunk, OP_LLAMAR_KW_DICT,
                                    (uint8_t)n_pos, e->linea);
                return true;
            }
            /* v1.23: llamada con keyword arguments. Empuja callee,
               posicionales en orden, luego pares (clave, valor) para
               cada kwarg. */
            if (tiene_kwargs) {
                int n_pos = 0;
                for (int i = 0; i < n_args; i++) {
                    if (e->como.llamada.kwarg_keys[i] == NULL) n_pos++;
                }
                int n_kw = n_args - n_pos;
                if (n_pos > 255 || n_kw > 255) {
                    error_compilacion(c, e->linea, e->columna,
                        "demasiados argumentos (max 255 posicionales o 255 keyword)");
                    return false;
                }
                if (!compilador_compilar_expr(c, callee)) return false;
                /* Posicionales primero (parser garantiza que están antes). */
                for (int i = 0; i < n_args; i++) {
                    if (e->como.llamada.kwarg_keys[i] != NULL) continue;
                    if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
                }
                /* Pares (clave_cadena, valor) para cada kwarg. */
                for (int i = 0; i < n_args; i++) {
                    const char *k = e->como.llamada.kwarg_keys[i];
                    if (k == NULL) continue;
                    int klen = e->como.llamada.kwarg_lens[i];
                    Valor v_clave = valor_cadena_duplicar(k, klen);
                    int idx_const = chunk_agregar_constante(c->actual->chunk, v_clave);
                    if (idx_const < 0 || idx_const > 255) {
                        error_compilacion(c, e->linea, e->columna,
                            "demasiadas constantes");
                        return false;
                    }
                    chunk_emitir_byte2(c->actual->chunk, OP_CONST,
                                        (uint8_t)idx_const, e->linea);
                    if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
                }
                chunk_emitir_byte(c->actual->chunk, OP_LLAMAR_KW, e->linea);
                chunk_emitir_byte(c->actual->chunk, (uint8_t)n_pos, e->linea);
                chunk_emitir_byte(c->actual->chunk, (uint8_t)n_kw, e->linea);
                return true;
            }
            /* Caso general: empuja callee, después args, y emite
               OP_LLAMAR [n]. La VM se encarga del frame y de validar
               aridad. */
            if (!compilador_compilar_expr(c, callee)) return false;
            for (int i = 0; i < n_args; i++) {
                if (!compilador_compilar_expr(c, e->como.llamada.args[i])) return false;
            }
            chunk_emitir_byte2(c->actual->chunk, OP_LLAMAR,
                               (uint8_t)n_args, e->linea);
            return true;
        }

        case EXPR_WALRUS: {
            /* v1.113: `nombre := valor`. Asigna `valor` a `nombre` y
             * deja el valor en stack como resultado de la expresion.
             *
             * Plan:
             *   1. Compilar valor -> stack [v]
             *   2. OP_DUP -> stack [v, v]
             *   3. Asignar TOS al destino (pop) -> stack [v]
             *
             * Para nuevo local en funcion: agregar_local registra el
             * slot en la posicion del primer v (sin OP_NULO), luego
             * OP_DUP empuja la copia que sera el resultado. Esto FALLA
             * dentro de bucles para variables creadas por primera vez
             * con walrus (el slot se fija en iter 1; en iter 2+ apunta
             * a posicion incorrecta — mismo bug que v0.11.5). Para
             * walrus dentro de loops, la variable debe pre-existir
             * (`n = 0` antes del loop). */
            if (!compilador_compilar_expr(c, e->como.walrus.valor)) return false;
            const char *nombre = e->como.walrus.nombre;
            int len = e->como.walrus.longitud;

            if (c->actual->es_funcion) {
                int slot = buscar_local(c->actual, nombre, len);
                if (slot >= 0) {
                    chunk_emitir_byte(c->actual->chunk, OP_DUP, e->linea);
                    chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                        (uint8_t)slot, e->linea);
                    return true;
                }
                int upv = resolver_upvalue(c, c->actual, nombre, len, e->linea);
                if (upv >= 0) {
                    chunk_emitir_byte(c->actual->chunk, OP_DUP, e->linea);
                    chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_UPVALUE,
                                        (uint8_t)upv, e->linea);
                    return true;
                }
                /* Nuevo local. El valor v esta en TOS; agregar_local lo
                 * registra como slot. Luego OP_DUP empuja la copia que
                 * es el resultado de la expresion. */
                if (agregar_local(c, nombre, len, e->linea) < 0) return false;
                chunk_emitir_byte(c->actual->chunk, OP_DUP, e->linea);
                return true;
            }
            /* Top-level: variable global. */
            chunk_emitir_byte(c->actual->chunk, OP_DUP, e->linea);
            int gidx = agregar_nombre_global(c, nombre, len);
            if (gidx < 0 || gidx > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "demasiadas constantes para walrus (operando byte)");
                return false;
            }
            chunk_emitir_byte2(c->actual->chunk, OP_DEFINIR_GLOBAL,
                                (uint8_t)gidx, e->linea);
            return true;
        }

        case EXPR_TERNARIA: {
            /* v1.44: `si_si si cond sino si_no`. Desugar a salto
               condicional. OP_SALTAR_SI_FALSO hace peek de cond, así
               que la rama tomada hace OP_DESCARTAR para soltar cond
               y empuja su propio valor. Stack neto: -1 (cond) + 1 (rama). */
            if (!compilador_compilar_expr(c, e->como.ternaria.cond)) return false;
            int salto_falso = emitir_salto(c, OP_SALTAR_SI_FALSO, e->linea);
            /* Rama verdadera: descartar cond y evaluar si_si. */
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, e->linea);
            if (!compilador_compilar_expr(c, e->como.ternaria.si_si)) return false;
            int salto_fin = emitir_salto(c, OP_SALTAR, e->linea);
            /* Rama falsa. */
            parchear_salto(c, salto_falso, e->linea);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, e->linea);
            if (!compilador_compilar_expr(c, e->como.ternaria.si_no)) return false;
            parchear_salto(c, salto_fin, e->linea);
            return true;
        }

        case EXPR_LOGICA: {
            /*
             * Cortocircuito al estilo clox cap. 23.
             *   `a y b`: si `a` es falso, deja `a` y salta sobre `b`.
             *            Si verdad, descarta `a` y evalúa `b`.
             *   `a o b`: si `a` es verdadero, deja `a` y salta sobre `b`.
             *            Si falso, descarta `a` y evalúa `b`.
             *
             * En ambos casos `OP_SALTAR_SI_FALSO` hace PEEK (no pop),
             * así que el valor ya queda en stack si decide la rama.
             */
            if (!compilador_compilar_expr(c, e->como.logica.izq)) return false;
            if (e->como.logica.es_y) {
                int salto_falso = emitir_salto(c, OP_SALTAR_SI_FALSO, e->linea);
                chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, e->linea);
                if (!compilador_compilar_expr(c, e->como.logica.der)) return false;
                parchear_salto(c, salto_falso, e->linea);
            } else {
                /* `a o b`: si a falso → siguiente; si a verdad → salta a fin. */
                int salto_falso = emitir_salto(c, OP_SALTAR_SI_FALSO, e->linea);
                int salto_fin   = emitir_salto(c, OP_SALTAR, e->linea);
                parchear_salto(c, salto_falso, e->linea);
                chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, e->linea);
                if (!compilador_compilar_expr(c, e->como.logica.der)) return false;
                parchear_salto(c, salto_fin, e->linea);
            }
            return true;
        }

        case EXPR_LISTA: {
            int n = e->como.secuencia.n_elementos;
            /* v1.171: si hay algun elemento spread `*xs`, construir
             * incrementalmente con OP_LISTA_AGREGAR / OP_LISTA_EXTENDER. */
            bool hay_spread = false;
            for (int i = 0; i < n; i++) {
                Expr *el = e->como.secuencia.elementos[i];
                if (el->tipo == EXPR_UNARIO
                    && el->como.unario.op == TT_ASTERISCO) {
                    hay_spread = true;
                    break;
                }
            }
            if (hay_spread) {
                chunk_emitir_byte2(c->actual->chunk, OP_BUILD_LISTA,
                                    0, e->linea);
                for (int i = 0; i < n; i++) {
                    Expr *el = e->como.secuencia.elementos[i];
                    if (el->tipo == EXPR_UNARIO
                        && el->como.unario.op == TT_ASTERISCO) {
                        if (!compilador_compilar_expr(c, el->como.unario.operando))
                            return false;
                        chunk_emitir_byte(c->actual->chunk,
                                            OP_LISTA_EXTENDER, e->linea);
                    } else {
                        if (!compilador_compilar_expr(c, el)) return false;
                        chunk_emitir_byte(c->actual->chunk,
                                            OP_LISTA_AGREGAR, e->linea);
                    }
                }
                return true;
            }
            if (n > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "literal de lista con mas de 255 elementos no soportado en v0.6");
                return false;
            }
            for (int i = 0; i < n; i++) {
                if (!compilador_compilar_expr(c, e->como.secuencia.elementos[i])) return false;
            }
            chunk_emitir_byte2(c->actual->chunk, OP_BUILD_LISTA,
                                (uint8_t)n, e->linea);
            return true;
        }
        case EXPR_TUPLA: {
            int n = e->como.secuencia.n_elementos;
            if (n > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "literal de tupla con mas de 255 elementos no soportado en v0.6");
                return false;
            }
            for (int i = 0; i < n; i++) {
                if (!compilador_compilar_expr(c, e->como.secuencia.elementos[i])) return false;
            }
            chunk_emitir_byte2(c->actual->chunk, OP_BUILD_TUPLA,
                                (uint8_t)n, e->linea);
            return true;
        }
        case EXPR_DICCIONARIO: {
            int n = e->como.diccionario.n_pares;
            if (n > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "literal de diccionario con mas de 255 pares no soportado en v0.6");
                return false;
            }
            for (int i = 0; i < n; i++) {
                if (!compilador_compilar_expr(c, e->como.diccionario.claves[i])) return false;
                if (!compilador_compilar_expr(c, e->como.diccionario.valores[i])) return false;
            }
            chunk_emitir_byte2(c->actual->chunk, OP_BUILD_DICC,
                                (uint8_t)n, e->linea);
            return true;
        }
        case EXPR_CONJUNTO: {
            int n = e->como.secuencia.n_elementos;
            if (n > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "literal de conjunto con mas de 255 elementos no soportado en v0.6");
                return false;
            }
            for (int i = 0; i < n; i++) {
                if (!compilador_compilar_expr(c, e->como.secuencia.elementos[i])) return false;
            }
            chunk_emitir_byte2(c->actual->chunk, OP_BUILD_CONJUNTO,
                                (uint8_t)n, e->linea);
            return true;
        }
        case EXPR_COMPREHENSION: {
            /* v1.30/v1.32: desugar a un bucle con acumulador. Usa
               `agregar_local` SIEMPRE — funciona tanto en función como
               en top-level (igual que `compilar_para` con `$iter`).
                acumulador = []  (o {} / set)
                $comp_iter = iter(iterable)
                $comp_var = nulo
                bucle:
                    OP_ITER_SIGUIENTE → si fin, salir
                    $comp_var = valor del iter
                    si guarda: eval; OP_SALTAR_SI_FALSO continuar
                    eval expr_elem (+ expr_valor para dict)
                    OP_{LISTA,CONJUNTO}_AGREGAR / OP_DICC_AGREGAR_PAR
                fin bucle
               Al salir, deja el acumulador como TOS y libera los 3
               slots temporales (n_locales -= 3). */
            int tipo_destino = e->como.comprehension.tipo_destino;

            /* v1.34: generator expression `(expr para v en iter si g)`.
               Se compila como una FuncionBC sintética generadora con un
               parámetro (el iterable), y se invoca inmediatamente con
               el iterable real. El cuerpo:
                   funcion $genex($it):
                       para v en $it:
                           si guarda: producir expr fin si
                       fin para
                   fin funcion
               `expr`/`guarda` resuelven variables externas como
               upvalues — gracias al scope hijo. Lazy de verdad: cada
               `iter_siguiente` reanuda el frame del generador. */
            if (tipo_destino == 3) {
                /* v1.136: genex con multiples `para` y/o destructuring.
                 * El primer iterable se pasa como parametro $gx_param; los
                 * extras se evaluan dentro del scope del generador (pueden
                 * referenciar variables anteriores como locales/upvalues).
                 *
                 * Mismo patron que list/dict/set: pre-reservar slots
                 * (iter, var, destinos del patron) ANTES del primer
                 * inicio_loop para evitar crecimiento del stack.
                 * El cuerpo, en lugar de agregar a un acumulador, hace
                 * OP_PRODUCIR. */
                FuncionBC *fn = funcion_bc_nueva("$genex", 6, 1);
                if (!fn) {
                    return error_compilacion(c, e->linea, e->columna,
                        "memoria insuficiente"), false;
                }
                ScopeCompilador scope_gx;
                scope_iniciar(&scope_gx, &fn->chunk, true, c->actual);
                scope_gx.funcion = fn;
                scope_gx.locales[0].nombre = "$genex";
                scope_gx.locales[0].longitud_nombre = 6;
                scope_gx.locales[1].nombre = "$gx_param";
                scope_gx.locales[1].longitud_nombre = 9;
                scope_gx.locales[1].capturado = false;
                scope_gx.n_locales = 2;

                ScopeCompilador *prev = c->actual;
                c->actual = &scope_gx;

                bool gx_ok = true;
                int gx_n_extras = e->como.comprehension.n_extras;
                struct ClausulaComp *gx_extras =
                    e->como.comprehension.clausulas_extra;
                int gx_n_clausulas = 1 + gx_n_extras;

                if (gx_n_clausulas > 16) {
                    c->actual = prev;
                    funcion_bc_liberar(fn);
                    error_compilacion(c, e->linea, e->columna,
                        "demasiadas clausulas en comprehension (max 16)");
                    return false;
                }

                Expr *gx_patrones[16];
                int gx_star_idx[16];
                int gx_slots_iter[16];
                int gx_slots_var[16];
                int gx_inicios[16];
                int gx_offsets_ph[16];
                int gx_saltos_cont[16];
                for (int i = 0; i < 16; i++) {
                    gx_patrones[i] = NULL;
                    gx_star_idx[i] = -1;
                    gx_saltos_cont[i] = -1;
                }
                gx_patrones[0] = e->como.comprehension.patron;
                for (int i = 1; i < gx_n_clausulas; i++) {
                    gx_patrones[i] = gx_extras[i - 1].patron;
                }
                /* v1.138: validar patrones recursivamente (acepta
                 * tuplas anidadas; max un STAR por nivel). */
                for (int i = 0; i < gx_n_clausulas; i++) {
                    if (gx_patrones[i] == NULL) continue;
                    if (!validar_patron_compr(c, gx_patrones[i],
                                                e->linea, e->columna)) {
                        c->actual = prev;
                        funcion_bc_liberar(fn);
                        return false;
                    }
                }

                /* Primera clausula: iterar sobre $gx_param. */
                chunk_emitir_byte2(&fn->chunk, OP_OBTENER_LOCAL, 1, e->linea);
                chunk_emitir_byte(&fn->chunk, OP_ITER_INICIAR, e->linea);
                gx_slots_iter[0] = agregar_local(c, "$gx_iter", 8, e->linea);
                if (gx_slots_iter[0] < 0) gx_ok = false;

                /* Pre-reservar slots de var/patron de TODAS las clausulas
                 * antes del primer inicio_loop. */
                for (int i = 0; i < gx_n_clausulas && gx_ok; i++) {
                    if (i > 0) {
                        chunk_emitir_byte(&fn->chunk, OP_NULO, e->linea);
                        gx_slots_iter[i] = agregar_local(c, "$gx_iter", 8, e->linea);
                        if (gx_slots_iter[i] < 0) { gx_ok = false; break; }
                    }
                    chunk_emitir_byte(&fn->chunk, OP_NULO, e->linea);
                    const char *vn; int vl;
                    if (gx_patrones[i] != NULL) {
                        vn = "$gx_item";
                        vl = 8;
                    } else if (i == 0) {
                        vn = e->como.comprehension.nombre_var;
                        vl = e->como.comprehension.longitud_var;
                    } else {
                        vn = gx_extras[i - 1].nombre_var;
                        vl = gx_extras[i - 1].longitud_var;
                    }
                    gx_slots_var[i] = agregar_local(c, vn, vl, e->linea);
                    if (gx_slots_var[i] < 0) { gx_ok = false; break; }
                    /* v1.138: pre-reservar slots del patron (hojas
                     * + sub-tuplas) recursivamente. */
                    if (gx_patrones[i] != NULL) {
                        if (!prereservar_slots_patron_compr(c, gx_patrones[i],
                                                              e->linea)) {
                            gx_ok = false; break;
                        }
                    }
                }

                /* Por cada clausula: inicio_loop, SIGUIENTE, ASIGNAR,
                 * destructuring opcional, guarda opcional. */
                for (int i = 0; i < gx_n_clausulas && gx_ok; i++) {
                    if (i > 0) {
                        if (!compilador_compilar_expr(c, gx_extras[i - 1].iterable)) {
                            gx_ok = false; break;
                        }
                        chunk_emitir_byte(&fn->chunk, OP_ITER_INICIAR, e->linea);
                        chunk_emitir_byte2(&fn->chunk, OP_ASIGNAR_LOCAL,
                                            (uint8_t)gx_slots_iter[i], e->linea);
                    }
                    gx_inicios[i] = fn->chunk.cuenta;
                    chunk_emitir_byte2(&fn->chunk, OP_ITER_SIGUIENTE,
                                        (uint8_t)gx_slots_iter[i], e->linea);
                    chunk_emitir_byte(&fn->chunk, 0xff, e->linea);
                    chunk_emitir_byte(&fn->chunk, 0xff, e->linea);
                    gx_offsets_ph[i] = fn->chunk.cuenta - 2;
                    chunk_emitir_byte2(&fn->chunk, OP_ASIGNAR_LOCAL,
                                        (uint8_t)gx_slots_var[i], e->linea);

                    /* v1.135/v1.138: destructuring inline si hay
                     * patron, ahora con soporte anidado via helper
                     * recursivo. */
                    if (gx_patrones[i] != NULL) {
                        int cursor = gx_slots_var[i] + 1;
                        if (!emitir_destruct_patron_compr(c, gx_slots_var[i],
                                                          gx_patrones[i],
                                                          &cursor, e->linea)) {
                            gx_ok = false; break;
                        }
                    }

                    Expr *guarda_i = (i == 0)
                        ? e->como.comprehension.guarda
                        : gx_extras[i - 1].guarda;
                    if (guarda_i) {
                        if (!compilador_compilar_expr(c, guarda_i)) {
                            gx_ok = false; break;
                        }
                        gx_saltos_cont[i] = emitir_salto(c, OP_SALTAR_SI_FALSO, e->linea);
                        chunk_emitir_byte(&fn->chunk, OP_DESCARTAR, e->linea);
                    }
                }

                /* Cuerpo: producir el elemento. */
                if (gx_ok) {
                    if (!compilador_compilar_expr(c, e->como.comprehension.expr_elem)) {
                        gx_ok = false;
                    }
                }
                if (gx_ok) {
                    chunk_emitir_byte(&fn->chunk, OP_PRODUCIR, e->linea);
                    /* Cerrar clausulas en orden inverso. */
                    for (int i = gx_n_clausulas - 1; i >= 0; i--) {
                        emitir_bucle(c, gx_inicios[i], e->linea);
                        if (gx_saltos_cont[i] >= 0) {
                            parchear_salto(c, gx_saltos_cont[i], e->linea);
                            chunk_emitir_byte(&fn->chunk, OP_DESCARTAR, e->linea);
                            emitir_bucle(c, gx_inicios[i], e->linea);
                        }
                        int offset_fin = fn->chunk.cuenta - gx_offsets_ph[i] - 2;
                        fn->chunk.codigo[gx_offsets_ph[i]] =
                            (uint8_t)((offset_fin >> 8) & 0xff);
                        fn->chunk.codigo[gx_offsets_ph[i] + 1] =
                            (uint8_t)(offset_fin & 0xff);
                    }
                    chunk_emitir_byte(&fn->chunk, OP_NULO, e->linea);
                    chunk_emitir_byte(&fn->chunk, OP_RETORNAR, e->linea);
                }

                c->actual = prev;
                if (!gx_ok) {
                    funcion_bc_liberar(fn);
                    return false;
                }
                fn->es_generador = true;
                /* Promover a constante + OP_CLOSURE + upvalues. */
                Valor v_pl = valor_plantilla(fn);
                int fn_idx = chunk_agregar_constante(c->actual->chunk, v_pl);
                if (fn_idx < 0 || fn_idx > 255) {
                    error_compilacion(c, e->linea, e->columna,
                        "demasiadas constantes");
                    return false;
                }
                chunk_emitir_byte2(c->actual->chunk, OP_CLOSURE,
                                    (uint8_t)fn_idx, e->linea);
                for (int i = 0; i < scope_gx.n_upvalues; i++) {
                    chunk_emitir_byte(c->actual->chunk,
                        scope_gx.upvalues[i].es_local ? 1 : 0, e->linea);
                    chunk_emitir_byte(c->actual->chunk,
                        scope_gx.upvalues[i].indice, e->linea);
                }
                /* Compilar el iterable de la primera clausula en el scope
                 * padre y llamar el genex con 1 arg. */
                if (!compilador_compilar_expr(c, e->como.comprehension.iterable)) return false;
                chunk_emitir_byte2(c->actual->chunk, OP_LLAMAR, 1, e->linea);
                return true;
            }

            /* 1. Crear acumulador, dejarlo en TOS, reservar su slot. */
            switch (tipo_destino) {
                case 0:
                    chunk_emitir_byte2(c->actual->chunk, OP_BUILD_LISTA, 0, e->linea);
                    break;
                case 1:
                    chunk_emitir_byte2(c->actual->chunk, OP_BUILD_DICC, 0, e->linea);
                    break;
                case 2:
                    chunk_emitir_byte2(c->actual->chunk, OP_BUILD_CONJUNTO, 0, e->linea);
                    break;
                default:
                    error_compilacion(c, e->linea, e->columna,
                        "tipo de comprehension desconocido");
                    return false;
            }
            int slot_acc = agregar_local(c, "$comp_acc", 9, e->linea);
            if (slot_acc < 0) return false;

            /* 2. Eval iterable + OP_ITER_INICIAR, reservar su slot. */
            if (!compilador_compilar_expr(c, e->como.comprehension.iterable)) return false;
            chunk_emitir_byte(c->actual->chunk, OP_ITER_INICIAR, e->linea);
            int slot_iter = agregar_local(c, "$comp_iter", 10, e->linea);
            if (slot_iter < 0) return false;

            /* v1.132: soportar multiples clausulas anidadas. La primera
             * vive en los campos legacy; las extras en `clausulas_extra`.
             * Estrategia: para cada clausula i en orden:
             *   - Si i == 0: el iter de la primera ya esta en slot_iter
             *     (compilado arriba antes de este bloque) y el var aun
             *     no se ha reservado.
             *   - Si i > 0: compilar iterable_i, OP_ITER_INICIAR,
             *     agregar slot_iter_i.
             *   - En todos: OP_NULO + agregar slot_var_i, inicio_loop_i,
             *     OP_ITER_SIGUIENTE slot_iter_i offset_ph_i,
             *     OP_ASIGNAR_LOCAL slot_var_i, eval guarda con
             *     OP_SALTAR_SI_FALSO salto_continuar_i si la hay.
             *
             * Tras la ultima clausula se hace el agregado al acumulador.
             * Luego, en orden inverso, se cierra cada clausula:
             * OP_BUCLE al inicio_loop_i, aterrizaje del continuar (si
             * habia guarda) con OP_DESCARTAR + OP_BUCLE, y patcheo del
             * offset_fin del OP_ITER_SIGUIENTE. Por cada clausula se
             * eliminan los 2 slots locales ($iter_i, var_i) al final.
             */
            int n_extras = e->como.comprehension.n_extras;
            struct ClausulaComp *extras = e->como.comprehension.clausulas_extra;
            int n_clausulas = 1 + n_extras;

            /* Limite practico: el bytecode v0.6 indexa locales con u8. */
            if (n_clausulas > 16) {
                error_compilacion(c, e->linea, e->columna,
                    "demasiadas clausulas en comprehension (max 16)");
                return false;
            }

            int slots_iter[16];
            int slots_var[16];
            int inicios_loop[16];
            int offsets_ph[16];
            int saltos_continuar[16];

            slots_iter[0] = slot_iter;
            /* v1.135: si la clausula i tiene patron destructuring,
             * patrones_i[i] guarda el EXPR_TUPLA con los destinos.
             * slots_var[i] sera un slot anonimo $comp_item; los slots
             * de los destinos del patron se pre-reservan tambien fuera
             * del loop, justo despues del slot anonimo. star_idx_i[i]
             * marca la posicion del star (-1 si no hay). */
            Expr *patrones[16];
            int star_idx_clausula[16];
            for (int i = 0; i < 16; i++) {
                patrones[i] = NULL;
                star_idx_clausula[i] = -1;
            }
            patrones[0] = e->como.comprehension.patron;
            for (int i = 1; i < n_clausulas; i++) {
                patrones[i] = extras[i - 1].patron;
            }
            /* v1.138: validar recursivamente cada patron (acepta
             * tuplas anidadas; max un STAR por nivel). */
            for (int i = 0; i < n_clausulas; i++) {
                if (patrones[i] == NULL) continue;
                if (!validar_patron_compr(c, patrones[i], e->linea, e->columna))
                    return false;
            }

            /* Pre-empujar OP_NULO + agregar_local para TODOS los slots
             * (var de clausula 0, y iter+var de las extras) ANTES del
             * primer inicio_loop. Asi sus OP_NULO no estan dentro de
             * ningun bucle, evitando que el stack crezca por iteracion.
             * Las cláusulas extra escriben sus iters con OP_ASIGNAR_LOCAL
             * dentro del cuerpo del loop padre.
             * v1.135: si la clausula tiene patron, ademas pre-reservar
             * slots para cada destino IDENT/STAR_BIND. */
            for (int i = 0; i < n_clausulas; i++) {
                saltos_continuar[i] = -1;
                if (i > 0) {
                    chunk_emitir_byte(c->actual->chunk, OP_NULO, e->linea);
                    slots_iter[i] = agregar_local(c, "$comp_iter", 10, e->linea);
                    if (slots_iter[i] < 0) return false;
                }
                chunk_emitir_byte(c->actual->chunk, OP_NULO, e->linea);
                const char *vn;
                int vl;
                if (patrones[i] != NULL) {
                    vn = "$comp_item";
                    vl = 10;
                } else if (i == 0) {
                    vn = e->como.comprehension.nombre_var;
                    vl = e->como.comprehension.longitud_var;
                } else {
                    vn = extras[i - 1].nombre_var;
                    vl = extras[i - 1].longitud_var;
                }
                slots_var[i] = agregar_local(c, vn, vl, e->linea);
                if (slots_var[i] < 0) return false;

                /* v1.138: si hay patron, pre-reservar slots de TODOS
                 * los destinos hoja Y sub-tuplas anonimas (DFS). */
                if (patrones[i] != NULL) {
                    if (!prereservar_slots_patron_compr(c, patrones[i],
                                                          e->linea))
                        return false;
                }
            }

            /* Emitir los inicios_loop y los SIGUIENTE/ASIGNAR de cada
             * clausula. Para clausulas i > 0, antes del inicio_loop[i]
             * eval iterable_i + ITER_INICIAR + OP_ASIGNAR_LOCAL al slot
             * pre-reservado — asi se ejecuta UNA vez por entrada al
             * loop padre, no en cada iter. */
            for (int i = 0; i < n_clausulas; i++) {
                if (i > 0) {
                    if (!compilador_compilar_expr(c, extras[i - 1].iterable))
                        return false;
                    chunk_emitir_byte(c->actual->chunk, OP_ITER_INICIAR, e->linea);
                    chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                        (uint8_t)slots_iter[i], e->linea);
                }
                inicios_loop[i] = c->actual->chunk->cuenta;
                chunk_emitir_byte2(c->actual->chunk, OP_ITER_SIGUIENTE,
                                    (uint8_t)slots_iter[i], e->linea);
                chunk_emitir_byte(c->actual->chunk, 0xff, e->linea);
                chunk_emitir_byte(c->actual->chunk, 0xff, e->linea);
                offsets_ph[i] = c->actual->chunk->cuenta - 2;

                chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                    (uint8_t)slots_var[i], e->linea);

                /* v1.135/v1.138: si la clausula tiene patron, hacer
                 * el destructuring inline. Helper recursivo soporta
                 * IDENT, *IDENT y tuplas anidadas. El item esta en
                 * el slot anonimo slots_var[i]; los slots de los
                 * destinos (incluyendo sub-tuplas anonimas) ya
                 * fueron pre-reservados con DFS. */
                if (patrones[i] != NULL) {
                    int cursor = slots_var[i] + 1;
                    if (!emitir_destruct_patron_compr(c, slots_var[i],
                                                       patrones[i], &cursor,
                                                       e->linea)) return false;
                }

                Expr *guarda_i = (i == 0)
                    ? e->como.comprehension.guarda
                    : extras[i - 1].guarda;
                if (guarda_i) {
                    if (!compilador_compilar_expr(c, guarda_i)) return false;
                    saltos_continuar[i] = emitir_salto(c, OP_SALTAR_SI_FALSO, e->linea);
                    chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, e->linea);
                }
            }

            /* Cuerpo: agregar al acumulador. */
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                                (uint8_t)slot_acc, e->linea);
            if (tipo_destino == 1) {
                if (!compilador_compilar_expr(c, e->como.comprehension.expr_elem)) return false;
                if (!compilador_compilar_expr(c, e->como.comprehension.expr_valor)) return false;
                chunk_emitir_byte(c->actual->chunk, OP_DICC_AGREGAR_PAR, e->linea);
            } else {
                if (!compilador_compilar_expr(c, e->como.comprehension.expr_elem)) return false;
                if (tipo_destino == 0) {
                    chunk_emitir_byte(c->actual->chunk, OP_LISTA_AGREGAR, e->linea);
                } else {
                    chunk_emitir_byte(c->actual->chunk, OP_CONJUNTO_AGREGAR, e->linea);
                }
            }
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, e->linea);

            /* Cerrar clausulas en orden inverso. */
            for (int i = n_clausulas - 1; i >= 0; i--) {
                emitir_bucle(c, inicios_loop[i], e->linea);
                if (saltos_continuar[i] >= 0) {
                    parchear_salto(c, saltos_continuar[i], e->linea);
                    chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, e->linea);
                    emitir_bucle(c, inicios_loop[i], e->linea);
                }
                int offset_fin = c->actual->chunk->cuenta - offsets_ph[i] - 2;
                c->actual->chunk->codigo[offsets_ph[i]] =
                    (uint8_t)((offset_fin >> 8) & 0xff);
                c->actual->chunk->codigo[offsets_ph[i] + 1] =
                    (uint8_t)(offset_fin & 0xff);
            }

            /* Limpieza: dejar el acumulador como TOS y liberar slots
             * temporales ($comp_acc, y por cada clausula su $comp_iter
             * y su var). 1 + 2 * n_clausulas slots base.
             * v1.135/v1.138: cada clausula con patron anade
             * contar_slots_patron(...) slots extra (DFS: hojas +
             * sub-tuplas anonimas). */
            int extras_patron = 0;
            for (int i = 0; i < n_clausulas; i++) {
                if (patrones[i] != NULL) {
                    extras_patron += contar_slots_patron(patrones[i]);
                }
            }
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                                (uint8_t)slot_acc, e->linea);
            chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                (uint8_t)slot_acc, e->linea);
            int n_desc = 2 * n_clausulas + extras_patron;
            for (int i = 0; i < n_desc; i++) {
                chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, e->linea);
            }
            c->actual->n_locales -= (1 + n_desc);
            return true;
        }
        case EXPR_INDICE: {
            if (!compilador_compilar_expr(c, e->como.indice.objeto)) return false;
            if (!compilador_compilar_expr(c, e->como.indice.indice)) return false;
            chunk_emitir_byte(c->actual->chunk, OP_INDICE, e->linea);
            return true;
        }

        case EXPR_REBANADA: {
            /*
             * `obj[a:b:c]`: cualquier campo opcional. Para los faltantes
             * emitimos `OP_NULO` como sentinela (la VM lo interpreta
             * como "default según el signo del paso"). El orden de
             * push: objeto, inicio, fin, paso (el OP_REBANADA los
             * saca en orden inverso).
             */
            if (!compilador_compilar_expr(c, e->como.rebanada.objeto)) return false;
            if (e->como.rebanada.inicio) {
                if (!compilador_compilar_expr(c, e->como.rebanada.inicio)) return false;
            } else {
                chunk_emitir_byte(c->actual->chunk, OP_NULO, e->linea);
            }
            if (e->como.rebanada.fin) {
                if (!compilador_compilar_expr(c, e->como.rebanada.fin)) return false;
            } else {
                chunk_emitir_byte(c->actual->chunk, OP_NULO, e->linea);
            }
            if (e->como.rebanada.paso) {
                if (!compilador_compilar_expr(c, e->como.rebanada.paso)) return false;
            } else {
                chunk_emitir_byte(c->actual->chunk, OP_NULO, e->linea);
            }
            chunk_emitir_byte(c->actual->chunk, OP_REBANADA, e->linea);
            return true;
        }

        case EXPR_LAMBDA: {
            int n_params = e->como.lambda.n_parametros;
            Parametro *params = e->como.lambda.parametros;
            /* v1.17: contar defaults y validar que estén en cola.
               v1.22: detectar `*resto`. v1.24: detectar `**kw`. */
            int n_defaults_lam = 0;
            bool vio_default = false;
            bool tiene_estrella_lam = false;
            bool tiene_doble_estrella_lam = false;
            int idx_doble_lam = -1;
            int idx_estrella_lam = -1;
            for (int i = 0; i < n_params; i++) {
                if (params[i].es_doble_estrella) {
                    tiene_doble_estrella_lam = true;
                    idx_doble_lam = i;
                } else if (params[i].es_estrella) {
                    tiene_estrella_lam = true;
                    idx_estrella_lam = i;
                } else if (params[i].valor_defecto != NULL) {
                    vio_default = true;
                    n_defaults_lam++;
                } else if (vio_default) {
                    error_compilacion(c, e->linea, e->columna,
                        "parametro sin valor por defecto despues de uno con default");
                    return false;
                }
            }
            if (tiene_doble_estrella_lam && idx_doble_lam != n_params - 1) {
                error_compilacion(c, e->linea, e->columna,
                    "'**kw' debe ser el ultimo parametro");
                return false;
            }
            if (tiene_estrella_lam) {
                int esperado = tiene_doble_estrella_lam ? n_params - 2 : n_params - 1;
                if (idx_estrella_lam != esperado) {
                    error_compilacion(c, e->linea, e->columna,
                        "'*resto' debe ir justo antes de '**kw' o ser el ultimo");
                    return false;
                }
            }
            if ((tiene_estrella_lam || tiene_doble_estrella_lam) && n_defaults_lam > 0) {
                error_compilacion(c, e->linea, e->columna,
                    "variádicos no se combinan con defaults en v1.24");
                return false;
            }

            FuncionBC *fn = funcion_bc_nueva("lambda", 6, n_params);
            if (!fn) return error_compilacion(c, e->linea, e->columna,
                "memoria insuficiente"), false;

            ScopeCompilador scope_lam;
            scope_iniciar(&scope_lam, &fn->chunk, true, c->actual);
            scope_lam.funcion = fn;
            scope_lam.locales[0].nombre = "lambda";
            scope_lam.locales[0].longitud_nombre = 6;
            for (int i = 0; i < n_params; i++) {
                scope_lam.locales[scope_lam.n_locales].nombre = params[i].nombre;
                scope_lam.locales[scope_lam.n_locales].longitud_nombre =
                    params[i].longitud_nombre;
                scope_lam.locales[scope_lam.n_locales].capturado = false;
                scope_lam.n_locales++;
            }

            ScopeCompilador *prev = c->actual;
            c->actual = &scope_lam;
            bool ok = compilador_compilar_expr(c, e->como.lambda.cuerpo);
            chunk_emitir_byte(&fn->chunk, OP_RETORNAR, e->linea);
            c->actual = prev;

            if (!ok) {
                funcion_bc_liberar(fn);
                return false;
            }

            /* v1.17: registrar n_defaults antes de promover a constante. */
            fn->n_defaults = n_defaults_lam;
            fn->tiene_estrella = tiene_estrella_lam;
            fn->tiene_doble_estrella = tiene_doble_estrella_lam;
            /* v1.23: duplicar nombres de params (lambda también soporta kwargs). */
            if (n_params > 0) {
                fn->nombres_params = (char **)malloc(sizeof(char *) * (size_t)n_params);
                fn->long_nombres_params = (int *)malloc(sizeof(int) * (size_t)n_params);
                if (!fn->nombres_params || !fn->long_nombres_params) {
                    error_compilacion(c, e->linea, e->columna, "memoria insuficiente");
                    funcion_bc_liberar(fn);
                    return false;
                }
                for (int i = 0; i < n_params; i++) {
                    int ln = params[i].longitud_nombre;
                    char *copia = (char *)malloc((size_t)ln + 1);
                    if (!copia) {
                        error_compilacion(c, e->linea, e->columna, "memoria insuficiente");
                        funcion_bc_liberar(fn);
                        return false;
                    }
                    if (ln > 0) memcpy(copia, params[i].nombre, (size_t)ln);
                    copia[ln] = '\0';
                    fn->nombres_params[i] = copia;
                    fn->long_nombres_params[i] = ln;
                }
            }

            Valor v_plantilla = valor_plantilla(fn);
            int fn_idx = chunk_agregar_constante(c->actual->chunk, v_plantilla);
            if (fn_idx < 0 || fn_idx > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "demasiadas constantes para v0.6 (operando byte)");
                return false;
            }
            /* v1.17: emitir expresiones de default antes de OP_CLOSURE. */
            for (int i = n_params - n_defaults_lam; i < n_params; i++) {
                if (!compilador_compilar_expr(c, params[i].valor_defecto)) return false;
            }
            chunk_emitir_byte2(c->actual->chunk, OP_CLOSURE,
                                (uint8_t)fn_idx, e->linea);
            for (int i = 0; i < scope_lam.n_upvalues; i++) {
                chunk_emitir_byte(c->actual->chunk,
                    scope_lam.upvalues[i].es_local ? 1 : 0, e->linea);
                chunk_emitir_byte(c->actual->chunk,
                    scope_lam.upvalues[i].indice, e->linea);
            }
            return true;
        }

        case EXPR_SUPER:
            error_compilacion(c, e->linea, e->columna,
                "'super' debe ir seguido de una llamada a metodo (super.metodo(...))");
            return false;

        case EXPR_ATRIBUTO: {
            if (!compilador_compilar_expr(c, e->como.atributo.objeto)) return false;
            int idx = chunk_agregar_constante(c->actual->chunk,
                valor_cadena_duplicar(e->como.atributo.nombre,
                                        e->como.atributo.longitud));
            if (idx < 0 || idx > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "demasiadas constantes para v0.7 (operando byte)");
                return false;
            }
            /* OP_OBTENER_ATRIBUTO ocupa 6 bytes desde v0.10 (F10):
               opcode + name_idx + 4 bytes de cache (clase_hash u16 +
               slot_idx u16). Quickening lo promueve a
               OP_OBTENER_ATRIBUTO_INSTANCIA tras el primer acierto en
               atributo de instancia. */
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_ATRIBUTO,
                                (uint8_t)idx, e->linea);
            chunk_emitir_byte2(c->actual->chunk, 0, 0, e->linea);
            chunk_emitir_byte2(c->actual->chunk, 0, 0, e->linea);
            return true;
        }

        case EXPR_LITERAL_F_CADENA: {
            /* Compila a una cadena de partes concatenadas con OP_SUMAR.
             * Cada parte literal se emite como OP_CONST con la cadena
             * (escapes ya decodificados); cada parte expresión se
             * compila y se pasa por OP_FORMATO_F (str-coerce). */
            int n = e->como.f_cadena.n_partes;
            const ParteFCadena *partes = e->como.f_cadena.partes;
            if (n == 0) {
                /* f"" → cadena vacía. */
                Valor v = valor_cadena_duplicar("", 0);
                chunk_emitir_constante(c->actual->chunk, v, e->linea);
                return true;
            }
            for (int i = 0; i < n; i++) {
                const ParteFCadena *p = &partes[i];
                if (p->expr) {
                    /* v1.112: para `f"{expr=}"` empujar primero el
                     * literal "expr=" + posibles espacios, luego el
                     * valor formateado, y OP_SUMAR para unirlos en
                     * un solo string. El resultado cuenta como UNA
                     * parte que se concatena al acumulado por el
                     * OP_SUMAR exterior al final del iter. */
                    bool tiene_debug = (p->debug_texto && p->debug_longitud > 0);
                    if (tiene_debug) {
                        Valor dbg = valor_cadena_duplicar(
                            p->debug_texto, p->debug_longitud);
                        if (dbg.tipo == VAL_NULO) {
                            error_compilacion(c, e->linea, e->columna,
                                "memoria insuficiente al compilar debug f-cadena");
                            return false;
                        }
                        chunk_emitir_constante(c->actual->chunk, dbg, e->linea);
                    }
                    if (!compilador_compilar_expr(c, p->expr)) return false;
                    if (p->spec && p->spec_longitud > 0) {
                        /* v1.45: con format spec. Almacenamos el spec
                           como constante cadena y emitimos
                           OP_FORMATO_F_SPEC [u8 spec_idx]. El runtime
                           ya garantiza VAL_CADENA tras procesar el
                           spec, así que no necesitamos OP_ASEGURAR_CADENA. */
                        Valor spec_v = valor_cadena_duplicar(p->spec,
                            p->spec_longitud);
                        if (spec_v.tipo == VAL_NULO) {
                            error_compilacion(c, e->linea, e->columna,
                                "memoria insuficiente al compilar f-cadena con spec");
                            return false;
                        }
                        int spec_idx = chunk_agregar_constante(
                            c->actual->chunk, spec_v);
                        if (spec_idx < 0 || spec_idx > 255) {
                            error_compilacion(c, e->linea, e->columna,
                                "demasiadas constantes para v0.6 (operando byte)");
                            return false;
                        }
                        chunk_emitir_byte2(c->actual->chunk,
                            OP_FORMATO_F_SPEC, (uint8_t)spec_idx, e->linea);
                    } else {
                        chunk_emitir_byte(c->actual->chunk, OP_FORMATO_F, e->linea);
                        /* v1.2: validar que el resultado de OP_FORMATO_F
                         * (posiblemente venido de `__cadena__`) sea cadena. */
                        chunk_emitir_byte(c->actual->chunk, OP_ASEGURAR_CADENA, e->linea);
                    }
                    if (tiene_debug) {
                        /* Fusionar "expr=" + valor en un solo string
                         * en stack. Despues el OP_SUMAR exterior lo
                         * concatena al acumulado. */
                        chunk_emitir_byte(c->actual->chunk, OP_SUMAR, e->linea);
                    }
                } else {
                    Valor v = valor_cadena_desde_escapes(p->literal, p->longitud);
                    if (v.tipo == VAL_NULO) {
                        error_compilacion(c, e->linea, e->columna,
                            "memoria insuficiente al compilar f-cadena");
                        return false;
                    }
                    chunk_emitir_constante(c->actual->chunk, v, e->linea);
                }
                /* Concatenar con la cadena acumulada (excepto en la
                 * primera parte, que es el seed). */
                if (i > 0) {
                    chunk_emitir_byte(c->actual->chunk, OP_SUMAR, e->linea);
                }
            }
            return true;
        }
    }

    error_compilacion(c, e->linea, e->columna,
        "tipo de expresion desconocido");
    return false;
}

bool compilador_compilar_expr_top(Compilador *c, const Expr *e) {
    if (!compilador_compilar_expr(c, e)) return false;
    chunk_emitir_byte(c->actual->chunk, OP_RETORNAR, e->linea);
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

static bool compilar_funcion(Compilador *c, const Sent *s);
static bool compilar_para(Compilador *c, const Sent *s);
static bool compilar_intentar(Compilador *c, const Sent *s);
static bool compilar_clase(Compilador *c, const Sent *s);

/* v1.21: destructuring helper. Asume valor a destructurar está en TOS.
   Emit bytecode que:
     1. Mueve TOS a un slot anónimo local.
     2. Verifica longitud == n elementos del patrón LHS.
     3. Por cada elemento i, extrae V[i] y lo asigna al elemento i del LHS.
        - Si LHS[i] es identificador: emit OP_ASIGNAR_LOCAL/UPVALUE/DEFINIR_GLOBAL.
        - Si LHS[i] es EXPR_TUPLA/EXPR_LISTA: recursión.
        - Otro tipo: error de compilación.
     4. Si la longitud no coincide: lanza ErrorDeValor con mensaje.

   El iterable puede ser tupla, lista, cadena, rango... cualquier cosa que
   soporte `longitud()` y `[i]`. No requiere tipo coincidente con el LHS.
*/
static bool emitir_destructuring(Compilador *c, const Expr *patron, int linea);

/*
 * v1.138: helpers para destructuring inline en comprehensions y
 * genex, ahora con soporte de patrones anidados (EXPR_TUPLA dentro
 * de EXPR_TUPLA).
 *
 * validar_patron_compr: recorre recursivamente comprobando que cada
 * elemento sea IDENT, STAR_BIND o TUPLA, y que haya como mucho un
 * STAR por nivel. NO emite bytes.
 *
 * prereservar_slots_patron_compr: por cada destino IDENT/STAR_BIND
 * emite OP_NULO + agregar_local con nombre real; por cada sub-TUPLA
 * emite OP_NULO + agregar_local "$compsub" y recurre. El orden DFS
 * coincide con emitir_destruct_patron_compr.
 *
 * emitir_destruct_patron_compr: verifica aridad y emite las
 * extracciones (OP_INDICE / OP_REBANADA + OP_ASIGNAR_LOCAL al slot
 * correspondiente). Para sub-TUPLA, llama recursivamente sobre el
 * slot que acaba de asignar. En aridad mala lanza ErrorDeValor
 * atrapable. `cursor` apunta al slot del primer destino del nivel y
 * avanza monotonicamente con cada slot consumido.
 *
 * Todos asumen c->actual fijado al scope correcto (function o
 * top-level para list/dict/set; scope_gx para genex).
 */
static bool validar_patron_compr(Compilador *c, const Expr *patron,
                                  int linea, int col) {
    int n_dst = patron->como.secuencia.n_elementos;
    Expr **dst = patron->como.secuencia.elementos;
    int star_count = 0;
    for (int j = 0; j < n_dst; j++) {
        switch (dst[j]->tipo) {
            case EXPR_IDENT:
                break;
            case EXPR_STAR_BIND:
                if (++star_count > 1) {
                    error_compilacion(c, linea, col,
                        "ErrorDeSintaxis: solo se permite un '*' por nivel del patron");
                    return false;
                }
                break;
            case EXPR_TUPLA:
                if (!validar_patron_compr(c, dst[j], linea, col)) return false;
                break;
            default:
                error_compilacion(c, linea, col,
                    "ErrorDeSintaxis: destino de comprehension debe ser "
                    "IDENT, '*IDENT' o tupla anidada");
                return false;
        }
    }
    return true;
}

static int contar_slots_patron(const Expr *patron) {
    int n_dst = patron->como.secuencia.n_elementos;
    Expr **dst = patron->como.secuencia.elementos;
    int total = n_dst;
    for (int j = 0; j < n_dst; j++) {
        if (dst[j]->tipo == EXPR_TUPLA) {
            total += contar_slots_patron(dst[j]);
        }
    }
    return total;
}

static bool prereservar_slots_patron_compr(Compilador *c, const Expr *patron,
                                              int linea) {
    int n_dst = patron->como.secuencia.n_elementos;
    Expr **dst = patron->como.secuencia.elementos;
    for (int j = 0; j < n_dst; j++) {
        const char *nm; int nm_len;
        if (dst[j]->tipo == EXPR_IDENT) {
            nm = dst[j]->como.ident.nombre;
            nm_len = dst[j]->como.ident.longitud;
        } else if (dst[j]->tipo == EXPR_STAR_BIND) {
            nm = dst[j]->como.star_bind.nombre;
            nm_len = dst[j]->como.star_bind.longitud;
        } else {
            nm = "$compsub";
            nm_len = 8;
        }
        chunk_emitir_byte(c->actual->chunk, OP_NULO, linea);
        int s = agregar_local(c, nm, nm_len, linea);
        if (s < 0) return false;
        if (dst[j]->tipo == EXPR_TUPLA) {
            if (!prereservar_slots_patron_compr(c, dst[j], linea)) return false;
        }
    }
    return true;
}

static bool emitir_destruct_patron_compr(Compilador *c, int slot_item,
                                            const Expr *patron, int *cursor,
                                            int linea);

/* Implementacion recursiva. */
static bool emitir_destruct_patron_compr(Compilador *c, int slot_item,
                                            const Expr *patron, int *cursor,
                                            int linea) {
    Chunk *chunk = c->actual->chunk;
    int n_dst = patron->como.secuencia.n_elementos;
    Expr **dst = patron->como.secuencia.elementos;
    int star_idx = -1;
    for (int j = 0; j < n_dst; j++) {
        if (dst[j]->tipo == EXPR_STAR_BIND) { star_idx = j; break; }
    }
    /* Aridad. */
    chunk_emitir_byte2(chunk, OP_OBTENER_LOCAL, (uint8_t)slot_item, linea);
    chunk_emitir_byte(chunk, OP_LONGITUD, linea);
    chunk_emitir_constante(chunk,
        valor_entero_de_long((long)(star_idx >= 0 ? n_dst - 1 : n_dst)),
        linea);
    chunk_emitir_byte(chunk,
        star_idx >= 0 ? OP_MAYOR_IGUAL : OP_IGUAL, linea);
    int salto_mal = emitir_salto(c, OP_SALTAR_SI_FALSO, linea);
    chunk_emitir_byte(chunk, OP_DESCARTAR, linea);
    /* Reservar los slots del NIVEL actual (consecutivos, en orden). */
    int slot_base = *cursor;
    *cursor += n_dst;
    /* Extraer cada destino y asignarlo a su slot. Recurrir para
     * sub-tuplas DESPUES de su asignacion. El sub-cursor avanza con
     * el cursor global (los slots de la sub-tupla son los siguientes
     * en el orden DFS pre-reservado). */
    for (int j = 0; j < n_dst; j++) {
        int slot_destino = slot_base + j;
        if (j == star_idx) {
            chunk_emitir_byte2(chunk, OP_OBTENER_LOCAL,
                                (uint8_t)slot_item, linea);
            chunk_emitir_constante(chunk,
                valor_entero_de_long((long)star_idx), linea);
            chunk_emitir_byte2(chunk, OP_OBTENER_LOCAL,
                                (uint8_t)slot_item, linea);
            chunk_emitir_byte(chunk, OP_LONGITUD, linea);
            int tail = n_dst - 1 - star_idx;
            if (tail > 0) {
                chunk_emitir_constante(chunk,
                    valor_entero_de_long((long)tail), linea);
                chunk_emitir_byte(chunk, OP_RESTAR, linea);
            }
            chunk_emitir_byte(chunk, OP_NULO, linea);
            chunk_emitir_byte(chunk, OP_REBANADA, linea);
        } else {
            long idx_real = (star_idx >= 0 && j > star_idx)
                ? (long)(j - n_dst) : (long)j;
            chunk_emitir_byte2(chunk, OP_OBTENER_LOCAL,
                                (uint8_t)slot_item, linea);
            chunk_emitir_constante(chunk,
                valor_entero_de_long(idx_real), linea);
            chunk_emitir_byte(chunk, OP_INDICE, linea);
        }
        chunk_emitir_byte2(chunk, OP_ASIGNAR_LOCAL,
                            (uint8_t)slot_destino, linea);
        if (dst[j]->tipo == EXPR_TUPLA) {
            if (!emitir_destruct_patron_compr(c, slot_destino, dst[j],
                                                cursor, linea)) return false;
        }
    }
    int salto_fin = emitir_salto(c, OP_SALTAR, linea);
    parchear_salto(c, salto_mal, linea);
    chunk_emitir_byte(chunk, OP_DESCARTAR, linea);
    int idx_err = agregar_nombre_global(c, "ErrorDeValor", 12);
    if (idx_err < 0 || idx_err > 255) {
        error_compilacion(c, linea, 0, "demasiadas constantes (>255)");
        return false;
    }
    chunk_emitir_byte2(chunk, OP_OBTENER_GLOBAL,
                        (uint8_t)idx_err, linea);
    chunk_emitir_byte2(chunk, 0, 0, linea);
    chunk_emitir_byte2(chunk, 0, 0, linea);
    chunk_emitir_constante(chunk,
        valor_cadena_duplicar(
            "aridad incorrecta en destructuring de comprehension",
            strlen("aridad incorrecta en destructuring de comprehension")),
        linea);
    chunk_emitir_byte2(chunk, OP_LLAMAR, 1, linea);
    chunk_emitir_byte(chunk, OP_LANZAR, linea);
    parchear_salto(c, salto_fin, linea);
    return true;
}

static bool emitir_asignacion_ident(Compilador *c, const Expr *destino,
                                      int linea) {
    /* Asume valor está en TOS. Lo asigna a `destino` (un EXPR_IDENT).
       Consume el TOS. */
    if (c->actual->es_funcion) {
        int slot = buscar_local(c->actual, destino->como.ident.nombre,
                                   destino->como.ident.longitud);
        if (slot >= 0) {
            /* Local existente: OP_ASIGNAR_LOCAL pop TOS y guarda en slot. */
            chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                (uint8_t)slot, linea);
            return true;
        }
        int upv = resolver_upvalue(c, c->actual,
                                      destino->como.ident.nombre,
                                      destino->como.ident.longitud, linea);
        if (upv >= 0) {
            chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_UPVALUE,
                                (uint8_t)upv, linea);
            return true;
        }
        /* Nuevo local. V ya está en TOS — registrar el slot allí.
           Limitación documentada: si el destructuring está dentro de un
           bucle, los nuevos locals creados en la primera iter no se
           re-asignan en iters siguientes (bug v0.11.5 que requiere la
           convención OP_NULO+agregar+ASIGNAR). Para destructuring en
           bucles, pre-declarar las variables fuera. */
        int nuevo = agregar_local(c, destino->como.ident.nombre,
                                       destino->como.ident.longitud, linea);
        if (nuevo < 0) return false;
        /* TOS → es el contenido del slot nuevo. No emit nada. */
        return true;
    }
    /* Top-level (global). */
    int idx = agregar_nombre_global(c, destino->como.ident.nombre,
                                      destino->como.ident.longitud);
    if (idx < 0 || idx > 255) {
        error_compilacion(c, linea, 0,
            "demasiadas constantes para v0.6 (operando byte)");
        return false;
    }
    chunk_emitir_byte2(c->actual->chunk, OP_DEFINIR_GLOBAL,
                        (uint8_t)idx, linea);
    return true;
}

static bool emitir_destructuring(Compilador *c, const Expr *patron, int linea) {
    /* Asume el valor a destructurar está en TOS.
       v1.28 fix: dentro de función, los destinos IDENT generan nuevos
       locales (cada uno consume el TOS pero deja el slot ocupado).
       Previo: el slot_iter quedaba por DEBAJO de los locales nuevos y
       el `OP_DESCARTAR` final descartaba el último valor del último
       destino en lugar del slot_iter, corrompiendo los locales del
       destructuring. Fix: en función, pre-reservar slots con OP_NULO
       para los destinos IDENT ANTES de evaluar (los destinos ya están
       en el stack cuando llega el valor a destructurar), luego usar
       OP_ASIGNAR_LOCAL (pop) para llenarlos. En top-level, los destinos
       son globales y OP_DEFINIR_GLOBAL ya pop, así que sigue funcionando
       como antes. */
    int n = patron->como.secuencia.n_elementos;
    Expr **elementos = patron->como.secuencia.elementos;
    bool en_funcion = c->actual->es_funcion;

    /* v1.28: en función, pre-reservar slots para destinos IDENT. Los
       destinos anidados (TUPLA/LISTA) seguirán el path recursivo y
       gestionarán sus propios slots. */
    int *slots_destinos = NULL;
    if (en_funcion) {
        slots_destinos = (int *)calloc((size_t)n, sizeof(int));
        if (!slots_destinos) {
            error_compilacion(c, linea, 0, "memoria insuficiente");
            return false;
        }
        /* TOS actualmente tiene el valor a destructurar. Necesitamos
           empujar nulos POR DEBAJO de él. La forma más simple: agregar
           local "_" para slot_iter, luego empujar OP_NULO por cada
           destino IDENT. Después de extraer cada V[i], usar
           OP_ASIGNAR_LOCAL para llenar el slot reservado. */
    }

    /* 1. Mover TOS a slot anónimo. */
    int slot_iter = agregar_local(c, "", 0, linea);
    if (slot_iter < 0) { free(slots_destinos); return false; }

    /* v1.28: en función, ahora reservar slots para cada IDENT empujando
       OP_NULO; anidados los gestionará la recursión.
       v1.122 fix: si la variable YA es local del scope actual (o upvalue
       capturado), NO crear un slot nuevo — reusar el existente. Antes
       cada iteración de un bucle creaba slots locales fantasma y la
       variable original (declarada fuera del bucle) quedaba congelada.
       Convencion de codificacion en slots_destinos[]:
         >= 0  : indice de slot LOCAL (nuevo pre-reservado o existente).
         == -1 : destino anidado (TUPLA/LISTA), gestion recursiva.
         <= -100 : indice de UPVALUE codificado como -100 - upv. */
    /* v1.129: detectar star binding `*nombre` entre los destinos. Solo
       UNO permitido. Su presencia cambia la check de aridad (>= n-1 en
       vez de == n) y el extractor del i-esimo elemento. */
    int star_idx = -1;
    for (int i = 0; i < n; i++) {
        if (elementos[i]->tipo == EXPR_STAR_BIND) {
            if (star_idx >= 0) {
                error_compilacion(c, linea, 0,
                    "ErrorDeSintaxis: solo se permite un '*' en destructuring");
                free(slots_destinos);
                return false;
            }
            star_idx = i;
        }
    }

    int n_nuevos_slots = 0;  /* contador de slots nuevos por encima de slot_iter */
    if (en_funcion) {
        for (int i = 0; i < n; i++) {
            const Expr *dst = elementos[i];
            const char *nm; int nm_len;
            if (dst->tipo == EXPR_IDENT) {
                nm = dst->como.ident.nombre;
                nm_len = dst->como.ident.longitud;
            } else if (dst->tipo == EXPR_STAR_BIND) {
                nm = dst->como.star_bind.nombre;
                nm_len = dst->como.star_bind.longitud;
            } else {
                slots_destinos[i] = -1;  /* anidado, recursion */
                continue;
            }
            int existente = buscar_local(c->actual, nm, nm_len);
            if (existente >= 0) {
                slots_destinos[i] = existente;
                continue;
            }
            int upv = resolver_upvalue(c, c->actual, nm, nm_len, linea);
            if (upv >= 0) {
                slots_destinos[i] = -100 - upv;
                continue;
            }
            chunk_emitir_byte(c->actual->chunk, OP_NULO, linea);
            int s = agregar_local(c, nm, nm_len, linea);
            if (s < 0) { free(slots_destinos); return false; }
            slots_destinos[i] = s;
            n_nuevos_slots++;
        }
    }

    /* 2. Verify aridad. Sin star: == n. Con star: >= n - 1. */
    chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                        (uint8_t)slot_iter, linea);
    chunk_emitir_byte(c->actual->chunk, OP_LONGITUD, linea);
    chunk_emitir_constante(c->actual->chunk,
                             valor_entero_de_long((long)(star_idx >= 0 ? n - 1 : n)),
                             linea);
    chunk_emitir_byte(c->actual->chunk,
                       star_idx >= 0 ? OP_MAYOR_IGUAL : OP_IGUAL, linea);
    int salto_mala_aridad = emitir_salto(c, OP_SALTAR_SI_FALSO, linea);
    chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, linea);  /* bool true */

    /* 3. Por cada elemento i: extraer V[i] (o slice para el star) y asignar.
     *
     * Con star_idx == k, n total destinos:
     *   i < k     -> V[i]                       (positivo)
     *   i == k    -> V[k : longitud(V) - (n-1-k)] (rebanada como lista)
     *   i > k     -> V[i - n]                   (negativo desde el final)
     */
    for (int i = 0; i < n; i++) {
        const Expr *dst_i = elementos[i];
        if (i == star_idx) {
            /* Stack: [..., obj, inicio, fin, paso] -> OP_REBANADA. */
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                                (uint8_t)slot_iter, linea);
            chunk_emitir_constante(c->actual->chunk,
                                     valor_entero_de_long((long)star_idx),
                                     linea);
            /* fin = longitud(iter) - (n - 1 - star_idx). */
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                                (uint8_t)slot_iter, linea);
            chunk_emitir_byte(c->actual->chunk, OP_LONGITUD, linea);
            int tail = n - 1 - star_idx;
            if (tail > 0) {
                chunk_emitir_constante(c->actual->chunk,
                                         valor_entero_de_long((long)tail),
                                         linea);
                chunk_emitir_byte(c->actual->chunk, OP_RESTAR, linea);
            }
            chunk_emitir_byte(c->actual->chunk, OP_NULO, linea);  /* paso = default */
            chunk_emitir_byte(c->actual->chunk, OP_REBANADA, linea);
        } else {
            long idx_real = (star_idx >= 0 && i > star_idx) ? (long)(i - n)
                                                              : (long)i;
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                                (uint8_t)slot_iter, linea);
            chunk_emitir_constante(c->actual->chunk,
                                     valor_entero_de_long(idx_real), linea);
            chunk_emitir_byte(c->actual->chunk, OP_INDICE, linea);
        }
        if (dst_i->tipo == EXPR_IDENT || dst_i->tipo == EXPR_STAR_BIND) {
            if (en_funcion) {
                int marca = slots_destinos[i];
                if (marca <= -100) {
                    int upv = -100 - marca;
                    chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_UPVALUE,
                                        (uint8_t)upv, linea);
                } else {
                    chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                        (uint8_t)marca, linea);
                }
            } else {
                /* Top-level: tratar STAR como IDENT para emitir_asignacion. */
                Expr ident_fake;
                Expr *destino_efectivo = (Expr *)dst_i;
                if (dst_i->tipo == EXPR_STAR_BIND) {
                    ident_fake.tipo = EXPR_IDENT;
                    ident_fake.linea = dst_i->linea;
                    ident_fake.columna = dst_i->columna;
                    ident_fake.como.ident.nombre = dst_i->como.star_bind.nombre;
                    ident_fake.como.ident.longitud = dst_i->como.star_bind.longitud;
                    destino_efectivo = &ident_fake;
                }
                if (!emitir_asignacion_ident(c, destino_efectivo, linea)) {
                    free(slots_destinos); return false;
                }
            }
        } else if (dst_i->tipo == EXPR_TUPLA || dst_i->tipo == EXPR_LISTA) {
            if (!emitir_destructuring(c, dst_i, linea)) {
                free(slots_destinos); return false;
            }
        } else {
            error_compilacion(c, linea, 0,
                "ErrorDeSintaxis: destino de destructuring debe ser "
                "identificador, '*ident' o tupla/lista anidada");
            free(slots_destinos);
            return false;
        }
    }

    /* 4. Cleanup: descartar slot_iter. En función está por DEBAJO de los
       destinos pre-reservados — sin OP_SWAP/ROT lo único que podemos
       hacer es marcarlo como muerto en c->n_locales (sin emit DESCARTAR
       que afectaría el tope). Pero entonces el stack queda con un
       slot huérfano. Solución: emitimos OP_DESCARTAR_BAJO conceptual
       via un truco: leer TOS → empujarlo → OP_ASIGNAR_LOCAL slot_iter
       → OP_DESCARTAR.
       Más simple aún: como slot_iter NO se usa más después de esto y
       persiste en el frame durante toda la función, lo dejamos como
       slot anónimo "muerto" que ocupa un slot. El cost es un slot
       extra hasta el fin del frame. Aceptable.
       Top-level: slot_iter es el TOS porque los globales hicieron pop.
       OP_DESCARTAR lo elimina limpiamente. */
    if (!en_funcion) {
        chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, linea);
        c->actual->n_locales--;
    } else if (n_nuevos_slots == 0) {
        /* v1.122 fix: si TODOS los destinos eran variables existentes
           (no se agregaron slots nuevos sobre slot_iter), entonces
           slot_iter ES el tope del stack ahora — descartarlo limpiamente.
           Crítico para bucles: sin esto, slot_iter sobrevivía iteración
           tras iteración y la lectura OP_OBTENER_LOCAL slot_iter siempre
           leía la tupla de la PRIMERA iter, congelando el destructuring. */
        chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, linea);
        c->actual->n_locales--;
    }
    /* En función con n_nuevos_slots > 0: slot_iter queda "muerto" debajo
       de los nuevos locals. Limitación conocida — el caso patológico es
       destructurar variables NUEVAS dentro de un bucle, que sigue
       creciendo el stack. Caso pedagógico común (variables declaradas
       fuera del bucle, swap dentro) ya queda resuelto. */
    free(slots_destinos);
    int salto_fin = emitir_salto(c, OP_SALTAR, linea);

    /* 5. Aterrizaje de aridad mala. Stack en este punto:
       - Top-level: [..., V, bool=false]. Descartar bool y V.
       - Función: [..., V, nulo_dst0, nulo_dst1, bool=false]. Descartar
         bool y V queda atrás. Lo mismo: el slot_iter (V) queda muerto. */
    parchear_salto(c, salto_mala_aridad, linea);
    chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, linea);  /* bool false */
    if (!en_funcion) {
        chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, linea);  /* slot_iter */
    }
    /* Emit `lanzar ErrorDeValor("aridad ...")`. */
    int idx_err = agregar_nombre_global(c, "ErrorDeValor", 12);
    if (idx_err < 0 || idx_err > 255) {
        error_compilacion(c, linea, 0, "demasiadas constantes (>255)");
        return false;
    }
    chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_GLOBAL,
                        (uint8_t)idx_err, linea);
    chunk_emitir_byte2(c->actual->chunk, 0, 0, linea);
    chunk_emitir_byte2(c->actual->chunk, 0, 0, linea);
    chunk_emitir_constante(c->actual->chunk,
        valor_cadena_duplicar("aridad incorrecta en destructuring",
                                strlen("aridad incorrecta en destructuring")),
        linea);
    chunk_emitir_byte2(c->actual->chunk, OP_LLAMAR, 1, linea);
    chunk_emitir_byte(c->actual->chunk, OP_LANZAR, linea);

    parchear_salto(c, salto_fin, linea);
    return true;
}

static bool compilar_asignar(Compilador *c, const Sent *s) {
    Expr *destino = s->como.asignar.destino;

    /* v1.21: destructuring `a, b = par` o `[a, b] = lista`. */
    if (destino->tipo == EXPR_TUPLA || destino->tipo == EXPR_LISTA) {
        if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;
        return emitir_destructuring(c, destino, s->linea);
    }

    /* Asignación a índice: `obj[key] = valor`. */
    if (destino->tipo == EXPR_INDICE) {
        if (!compilador_compilar_expr(c, destino->como.indice.objeto)) return false;
        if (!compilador_compilar_expr(c, destino->como.indice.indice)) return false;
        if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;
        chunk_emitir_byte(c->actual->chunk, OP_ASIGNAR_INDICE, s->linea);
        chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
        return true;
    }

    /* v1.44: asignación por rebanada: `lista[i:j:k] = iterable`. */
    if (destino->tipo == EXPR_REBANADA) {
        if (!compilador_compilar_expr(c, destino->como.rebanada.objeto)) return false;
        /* inicio/fin/paso: nulo si no presentes (mismo convenio que OP_REBANADA). */
        if (destino->como.rebanada.inicio) {
            if (!compilador_compilar_expr(c, destino->como.rebanada.inicio)) return false;
        } else {
            chunk_emitir_byte(c->actual->chunk, OP_NULO, s->linea);
        }
        if (destino->como.rebanada.fin) {
            if (!compilador_compilar_expr(c, destino->como.rebanada.fin)) return false;
        } else {
            chunk_emitir_byte(c->actual->chunk, OP_NULO, s->linea);
        }
        if (destino->como.rebanada.paso) {
            if (!compilador_compilar_expr(c, destino->como.rebanada.paso)) return false;
        } else {
            chunk_emitir_byte(c->actual->chunk, OP_NULO, s->linea);
        }
        if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;
        chunk_emitir_byte(c->actual->chunk, OP_ASIGNAR_REBANADA, s->linea);
        chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
        return true;
    }

    /* Asignación a atributo: `obj.attr = valor` (Fase 8 v0.7.0). */
    if (destino->tipo == EXPR_ATRIBUTO) {
        if (!compilador_compilar_expr(c, destino->como.atributo.objeto)) return false;
        if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;
        int idx = chunk_agregar_constante(c->actual->chunk,
            valor_cadena_duplicar(destino->como.atributo.nombre,
                                    destino->como.atributo.longitud));
        if (idx < 0 || idx > 255) {
            error_compilacion(c, s->linea, s->columna,
                "demasiadas constantes para v0.7 (operando byte)");
            return false;
        }
        chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_ATRIBUTO,
                            (uint8_t)idx, s->linea);
        chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
        return true;
    }

    if (destino->tipo != EXPR_IDENT) {
        error_compilacion(c, s->linea, s->columna,
            "ErrorDeSintaxis: destino de asignacion no soportado en bytecode v0.6 sesion 6");
        return false;
    }

    /*
     * Dentro de función: prioridad local → upvalue → nuevo local.
     * En el scope raíz (top-level): toda asignación va a globales.
     * v1.57: si `global X` se declaro antes, saltar a global directo.
     */
    if (c->actual->es_funcion) {
        bool forzar_global = es_global_declarado(c->actual,
                                                    destino->como.ident.nombre,
                                                    destino->como.ident.longitud);
        if (!forzar_global) {
            int slot = buscar_local(c->actual, destino->como.ident.nombre,
                                       destino->como.ident.longitud);
            if (slot >= 0) {
                /* Local existente: empujar valor, asignar al slot. */
                if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;
                chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                    (uint8_t)slot, s->linea);
                return true;
            }
            int upv = resolver_upvalue(c, c->actual,
                                          destino->como.ident.nombre,
                                          destino->como.ident.longitud, s->linea);
            if (upv >= 0) {
                if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;
                chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_UPVALUE,
                                    (uint8_t)upv, s->linea);
                return true;
            }
        } else {
            /* `global X`: empujar valor y guardar en globales. */
            if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;
            int gidx = agregar_nombre_global(c, destino->como.ident.nombre,
                                                destino->como.ident.longitud);
            if (gidx < 0 || gidx > 255) {
                error_compilacion(c, s->linea, s->columna,
                    "demasiadas constantes para 'global' en bytecode v0.6");
                return false;
            }
            chunk_emitir_byte2(c->actual->chunk, OP_DEFINIR_GLOBAL,
                                (uint8_t)gidx, s->linea);
            return true;
        }
        /*
         * Nuevo local. Antes de v0.11.5 usábamos "OLD convention":
         * empujar valor + agregar_local sin OP_ASIGNAR_LOCAL, asumiendo
         * que el push deja el valor en el slot del local. Eso solo
         * funciona en la PRIMERA ejecución; dentro de un bucle el slot
         * queda fijado y el push de iteraciones siguientes va a un
         * stack pos distinto, dejando el slot con el valor de la
         * primera iter (bug v0.11.5).
         *
         * Fix: emitir OP_NULO (reservar slot en stack), agregar_local,
         * compilar valor (push), OP_ASIGNAR_LOCAL al slot (pop+asign).
         */
        chunk_emitir_byte(c->actual->chunk, OP_NULO, s->linea);
        int nuevo = agregar_local(c, destino->como.ident.nombre,
                                      destino->como.ident.longitud,
                                      s->linea);
        if (nuevo < 0) return false;
        if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;
        chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                            (uint8_t)nuevo, s->linea);
        return true;
    }

    /* Top-level: empujar valor y guardar en global. */
    if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;

    int idx = agregar_nombre_global(c, destino->como.ident.nombre,
                                      destino->como.ident.longitud);
    if (idx < 0 || idx > 255) {
        error_compilacion(c, s->linea, s->columna,
            "demasiadas constantes para v0.6 (operando byte)");
        return false;
    }
    chunk_emitir_byte2(c->actual->chunk, OP_DEFINIR_GLOBAL, (uint8_t)idx, s->linea);
    return true;
}

/* SENT_ASIGNAR_AUG con destino IDENT: `x op= expr` se compila como
 * `x = x op expr`. Read-modify-write atomico desde la perspectiva del
 * usuario; el bytecode lo expresa con OP_OBTENER_GLOBAL + compilar
 * expr + op binario + OP_DEFINIR_GLOBAL.
 */
static bool compilar_asignar_aug(Compilador *c, const Sent *s) {
    Expr *destino = s->como.asignar_aug.destino;

    /* Destino EXPR_INDICE: `obj[key] op= valor` se desazucara a:
     *
     *   compile obj
     *   compile key
     *   OP_DUP_2                      ; preserva obj, key para asignar
     *   OP_INDICE                     ; pop obj, key; push obj[key]
     *   compile valor
     *   OP_op
     *   OP_ASIGNAR_INDICE             ; pop obj, key, resultado
     *   OP_DESCARTAR                  ; descarta el nulo de ASIGNAR_INDICE
     */
    if (destino->tipo == EXPR_INDICE) {
        TipoToken op_aug = s->como.asignar_aug.op;
        int op_byte = -1;
        switch (op_aug) {
            case TT_ASIGNAR_MAS:         op_byte = OP_SUMAR; break;
            case TT_ASIGNAR_MENOS:       op_byte = OP_RESTAR; break;
            case TT_ASIGNAR_ASTERISCO:   op_byte = OP_MULTIPLICAR; break;
            case TT_ASIGNAR_BARRA:       op_byte = OP_DIVIDIR; break;
            case TT_ASIGNAR_DOBLE_BARRA: op_byte = OP_DIVIDIR_ENTERO; break;
            case TT_ASIGNAR_PORCENTAJE:  op_byte = OP_MODULO; break;
            case TT_ASIGNAR_DOBLE_ASTER: op_byte = OP_POTENCIA; break;
            default:
                error_compilacion(c, s->linea, s->columna,
                    "operador de asignacion aumentada desconocido");
                return false;
        }
        if (!compilador_compilar_expr(c, destino->como.indice.objeto)) return false;
        if (!compilador_compilar_expr(c, destino->como.indice.indice)) return false;
        chunk_emitir_byte(c->actual->chunk, OP_DUP_2, s->linea);
        chunk_emitir_byte(c->actual->chunk, OP_INDICE, s->linea);
        if (!compilador_compilar_expr(c, s->como.asignar_aug.valor)) return false;
        chunk_emitir_byte(c->actual->chunk, (uint8_t)op_byte, s->linea);
        chunk_emitir_byte(c->actual->chunk, OP_ASIGNAR_INDICE, s->linea);
        chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
        return true;
    }

    /* v1.56: `obj.attr op= valor` se desazucara a:
     *
     *   compile obj           ; stack: [obj]
     *   OP_DUP                ; stack: [obj, obj]
     *   OP_OBTENER_ATRIBUTO   ; pop obj, push obj.attr → [obj, obj.attr]
     *                         ; (6 bytes: opcode + idx + 4 bytes cache)
     *   compile valor         ; [obj, obj.attr, valor]
     *   OP_op                 ; [obj, resultado]
     *   OP_ASIGNAR_ATRIBUTO   ; pop resultado + obj, set obj.attr → push nulo
     *   OP_DESCARTAR          ; descarta el nulo
     */
    if (destino->tipo == EXPR_ATRIBUTO) {
        TipoToken op_aug = s->como.asignar_aug.op;
        int op_byte = -1;
        switch (op_aug) {
            case TT_ASIGNAR_MAS:         op_byte = OP_SUMAR; break;
            case TT_ASIGNAR_MENOS:       op_byte = OP_RESTAR; break;
            case TT_ASIGNAR_ASTERISCO:   op_byte = OP_MULTIPLICAR; break;
            case TT_ASIGNAR_BARRA:       op_byte = OP_DIVIDIR; break;
            case TT_ASIGNAR_DOBLE_BARRA: op_byte = OP_DIVIDIR_ENTERO; break;
            case TT_ASIGNAR_PORCENTAJE:  op_byte = OP_MODULO; break;
            case TT_ASIGNAR_DOBLE_ASTER: op_byte = OP_POTENCIA; break;
            default:
                error_compilacion(c, s->linea, s->columna,
                    "operador de asignacion aumentada desconocido");
                return false;
        }
        if (!compilador_compilar_expr(c, destino->como.atributo.objeto)) return false;
        int idx = chunk_agregar_constante(c->actual->chunk,
            valor_cadena_duplicar(destino->como.atributo.nombre,
                                    destino->como.atributo.longitud));
        if (idx < 0 || idx > 255) {
            error_compilacion(c, s->linea, s->columna,
                "demasiadas constantes para 'obj.attr op= valor'");
            return false;
        }
        chunk_emitir_byte(c->actual->chunk, OP_DUP, s->linea);
        /* OP_OBTENER_ATRIBUTO: 6 bytes (opcode + idx + 4 cache bytes). */
        chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_ATRIBUTO,
                            (uint8_t)idx, s->linea);
        chunk_emitir_byte2(c->actual->chunk, 0, 0, s->linea);
        chunk_emitir_byte2(c->actual->chunk, 0, 0, s->linea);
        if (!compilador_compilar_expr(c, s->como.asignar_aug.valor)) return false;
        chunk_emitir_byte(c->actual->chunk, (uint8_t)op_byte, s->linea);
        chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_ATRIBUTO,
                            (uint8_t)idx, s->linea);
        chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
        return true;
    }

    if (destino->tipo != EXPR_IDENT) {
        error_compilacion(c, s->linea, s->columna,
            "ErrorDeSintaxis: destino de asignacion aumentada no soportado en bytecode v0.6");
        return false;
    }

    /* v1.57: si fue declarada `global`, saltar a global directo. */
    bool forzar_global = c->actual->es_funcion
        && es_global_declarado(c->actual,
                                destino->como.ident.nombre,
                                destino->como.ident.longitud);

    /* Decidir si es local, upvalue o global. */
    int slot_local = (c->actual->es_funcion && !forzar_global)
        ? buscar_local(c->actual, destino->como.ident.nombre,
                          destino->como.ident.longitud)
        : -1;
    int slot_upv = -1;
    if (slot_local < 0 && c->actual->es_funcion && !forzar_global) {
        slot_upv = resolver_upvalue(c, c->actual,
                                       destino->como.ident.nombre,
                                       destino->como.ident.longitud,
                                       s->linea);
    }

    if (slot_local >= 0) {
        chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                            (uint8_t)slot_local, s->linea);
    } else if (slot_upv >= 0) {
        chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_UPVALUE,
                            (uint8_t)slot_upv, s->linea);
    } else {
        int idx_get = agregar_nombre_global(c, destino->como.ident.nombre,
                                              destino->como.ident.longitud);
        if (idx_get < 0 || idx_get > 255) {
            error_compilacion(c, s->linea, s->columna,
                "demasiadas constantes para v0.6 (operando byte)");
            return false;
        }
        /* 6-byte form (v0.10 / F10) — ver nota en EXPR_IDENT arriba. */
        chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_GLOBAL,
                            (uint8_t)idx_get, s->linea);
        chunk_emitir_byte2(c->actual->chunk, 0, 0, s->linea);
        chunk_emitir_byte2(c->actual->chunk, 0, 0, s->linea);
    }

    /* expresion derecha. */
    if (!compilador_compilar_expr(c, s->como.asignar_aug.valor)) return false;

    /* Mapear el token aumentado a OpCode binario. */
    TipoToken op_aug = s->como.asignar_aug.op;
    int op_byte = -1;
    switch (op_aug) {
        case TT_ASIGNAR_MAS:         op_byte = OP_SUMAR; break;
        case TT_ASIGNAR_MENOS:       op_byte = OP_RESTAR; break;
        case TT_ASIGNAR_ASTERISCO:   op_byte = OP_MULTIPLICAR; break;
        case TT_ASIGNAR_BARRA:       op_byte = OP_DIVIDIR; break;
        case TT_ASIGNAR_DOBLE_BARRA: op_byte = OP_DIVIDIR_ENTERO; break;
        case TT_ASIGNAR_PORCENTAJE:  op_byte = OP_MODULO; break;
        case TT_ASIGNAR_DOBLE_ASTER: op_byte = OP_POTENCIA; break;
        default:
            error_compilacion(c, s->linea, s->columna,
                "operador de asignacion aumentada desconocido");
            return false;
    }
    chunk_emitir_byte(c->actual->chunk, (uint8_t)op_byte, s->linea);

    if (slot_local >= 0) {
        chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                            (uint8_t)slot_local, s->linea);
    } else if (slot_upv >= 0) {
        chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_UPVALUE,
                            (uint8_t)slot_upv, s->linea);
    } else {
        int idx_set = agregar_nombre_global(c, destino->como.ident.nombre,
                                              destino->como.ident.longitud);
        if (idx_set < 0 || idx_set > 255) {
            error_compilacion(c, s->linea, s->columna,
                "demasiadas constantes para v0.6 (operando byte)");
            return false;
        }
        chunk_emitir_byte2(c->actual->chunk, OP_DEFINIR_GLOBAL,
                            (uint8_t)idx_set, s->linea);
    }
    return true;
}

/*
 * v1.95: pre-pass que recolecta identificadores ASIGNADOS dentro de
 * las ramas de un `si` que NO existen aun como locales/globales/upvalues.
 *
 * Motivacion: una asignacion `v = expr` dentro de una rama emite
 * `OP_NULO + agregar_local + OP_ASIGNAR_LOCAL`. Si la rama no se
 * ejecuta (la otra se toma), el `OP_NULO` no corre — el slot del
 * stack queda desalineado. Las otras ramas que asignen al mismo
 * `v` encontraran el slot ya registrado en el compilador y emitiran
 * `OP_ASIGNAR_LOCAL slot` que pisa lo que sea que este en stack[slot].
 *
 * Fix: pre-declarar (OP_NULO + agregar_local) ANTES del si para
 * cualquier identificador que pueda quedar nuevo. Las asignaciones
 * dentro de las ramas reusan el slot, no crean uno.
 *
 * Recorre cuerpo (siempre SENT_BLOQUE) recursivamente, bajando solo
 * por sub-SENT_BLOQUE y sub-SENT_SI. NO entra en `funcion`, `clase`,
 * `para`, `mientras`, `intentar` (tienen su propio control flow).
 */
typedef struct {
    const char *nombre;
    int longitud;
} _IdentPendiente;

static bool _ya_recolectado(const _IdentPendiente *arr, int n,
                              const char *nombre, int len) {
    for (int i = 0; i < n; i++) {
        if (arr[i].longitud == len
            && memcmp(arr[i].nombre, nombre, (size_t)len) == 0) {
            return true;
        }
    }
    return false;
}

static void _recolectar_locales_nuevos_sent(Compilador *c, const Sent *s,
                                              _IdentPendiente *arr, int cap,
                                              int *n) {
    if (s == NULL || *n >= cap) return;
    switch (s->tipo) {
        case SENT_BLOQUE:
            for (int i = 0; i < s->como.bloque.n_sentencias && *n < cap; i++) {
                _recolectar_locales_nuevos_sent(c, s->como.bloque.sentencias[i],
                                                  arr, cap, n);
            }
            break;
        case SENT_SI:
            for (int i = 0; i < s->como.si.n_ramas && *n < cap; i++) {
                _recolectar_locales_nuevos_sent(c, s->como.si.ramas[i].cuerpo,
                                                  arr, cap, n);
            }
            break;
        case SENT_ASIGNAR: {
            Expr *dest = s->como.asignar.destino;
            if (dest && dest->tipo == EXPR_IDENT) {
                const char *nombre = dest->como.ident.nombre;
                int len = dest->como.ident.longitud;
                if (es_global_declarado(c->actual, nombre, len)) break;
                if (buscar_local(c->actual, nombre, len) >= 0) break;
                /* No bajamos a chequear upvalue aqui — solo necesitamos
                 * detectar nuevos LOCALES. Si la variable resuelve a
                 * upvalue/global, compilar_asignar lo manejara. */
                if (!_ya_recolectado(arr, *n, nombre, len)) {
                    arr[*n].nombre = nombre;
                    arr[*n].longitud = len;
                    (*n)++;
                }
            }
            break;
        }
        default:
            /* Para SENT_FUNCION, SENT_CLASE, SENT_PARA, SENT_MIENTRAS,
             * SENT_INTENTAR no entramos — tienen su propio scope o
             * control flow. */
            break;
    }
}

/*
 * SENT_SI: cadena de ramas (si / sino si* / sino?).
 *
 *   [pre-declarar nuevos locales con OP_NULO + agregar_local]   ; v1.95
 *   compile cond1
 *   OP_SALTAR_SI_FALSO L_else1
 *   OP_DESCARTAR             ; cond1 (truthy)
 *   compile cuerpo1
 *   OP_SALTAR L_end
 * L_else1:
 *   OP_DESCARTAR             ; cond1 (falsy)
 *   compile cond2
 *   OP_SALTAR_SI_FALSO L_else2
 *   ...
 * L_end:
 */
static bool compilar_si(Compilador *c, const Sent *s) {
    int n = s->como.si.n_ramas;
    /* Hasta 64 ramas razonables en una cadena de `sino si`. */
    int saltos_fin[64];
    int n_saltos_fin = 0;

    /* v1.95: pre-declarar locales nuevos. Solo dentro de funciones —
     * en top-level las asignaciones van a globals (no hay stack slot). */
    if (c->actual->es_funcion) {
        _IdentPendiente pendientes[32];
        int n_pendientes = 0;
        for (int i = 0; i < n; i++) {
            _recolectar_locales_nuevos_sent(c, s->como.si.ramas[i].cuerpo,
                                              pendientes, 32, &n_pendientes);
        }
        for (int i = 0; i < n_pendientes; i++) {
            chunk_emitir_byte(c->actual->chunk, OP_NULO, s->linea);
            if (agregar_local(c, pendientes[i].nombre,
                               pendientes[i].longitud, s->linea) < 0) {
                return false;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        RamaSi *r = &s->como.si.ramas[i];
        if (r->condicion != NULL) {
            if (!compilador_compilar_expr(c, r->condicion)) return false;
            int salto_else = emitir_salto(c, OP_SALTAR_SI_FALSO, r->linea);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, r->linea);
            if (!compilador_compilar_sent(c, r->cuerpo)) return false;
            /* Salto al final de toda la cadena (parchear al cerrar). */
            if (n_saltos_fin >= 64) {
                error_compilacion(c, s->linea, s->columna,
                    "demasiadas ramas en `si`");
                return false;
            }
            saltos_fin[n_saltos_fin++] = emitir_salto(c, OP_SALTAR, r->linea);
            parchear_salto(c, salto_else, r->linea);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, r->linea);
        } else {
            /* Rama `sino` final. */
            if (!compilador_compilar_sent(c, r->cuerpo)) return false;
        }
    }
    /* Parchear todos los saltos al fin. */
    for (int i = 0; i < n_saltos_fin; i++) {
        parchear_salto(c, saltos_fin[i], s->linea);
    }
    return true;
}

/*
 * SENT_COINCIDIR v1.15+v1.16: desugar a if/else chain en dos pasadas.
 *
 * Estrategia de match:
 *   - El sujeto raíz vive en un slot de local anónimo (`slot_sujeto`).
 *   - Pasada 1 (`emitir_verify`): verifica tipo, longitud y literales
 *     recursivamente. Sin tocar locales. Cada test fallido emite un
 *     salto a la lista `saltos[]` con UN bool false en stack, NADA más.
 *   - Pasada 2 (`emitir_binds`): tras verify exitoso, emite las
 *     asignaciones de los binds. Cada bind navega el sujeto vía cadena
 *     de OP_INDICE desde slot_sujeto siguiendo el path almacenado.
 *
 * Esta separación garantiza stack invariante: el aterrizaje L_no solo
 * descarta UN bool; sin locales zombies entre cláusulas.
 *
 * Ventajas: anidación arbitraria de tuplas/listas funciona sin
 * complejidad adicional. La pasada de binds solo se ejecuta si toda
 * la verify pasó.
 */
#define MATCH_MAX_SALTOS 64
#define MATCH_MAX_PROFUNDIDAD 16

/* Indices_path: cadena de índices desde slot_sujeto. p.ej. para acceder
   a S[1][0], path = [1, 0]. Vacío para el sujeto mismo. */

/* Emite código que navega de slot_sujeto a S[indices[0]][indices[1]]...
   y deja el resultado en el TOS. */
static void emitir_navegar(Compilador *c, int slot_sujeto,
                             const int *indices, int n_indices, int linea) {
    chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                        (uint8_t)slot_sujeto, linea);
    for (int i = 0; i < n_indices; i++) {
        chunk_emitir_constante(c->actual->chunk,
                                 valor_entero_de_long((long)indices[i]), linea);
        chunk_emitir_byte(c->actual->chunk, OP_INDICE, linea);
    }
}

static bool emitir_verify(Compilador *c, const Patron *pat,
                            int slot_sujeto,
                            int *indices, int n_indices,
                            int *saltos, int *n_saltos);

static bool emitir_verify(Compilador *c, const Patron *pat,
                            int slot_sujeto,
                            int *indices, int n_indices,
                            int *saltos, int *n_saltos) {
    int pl = pat->linea;
    switch (pat->tipo) {
        case PATRON_WILDCARD:
        case PATRON_BIND:
            /* Verify pass: ambos siempre matchean. Nada que comprobar. */
            return true;

        case PATRON_LITERAL:
            emitir_navegar(c, slot_sujeto, indices, n_indices, pl);
            if (!compilador_compilar_expr(c, pat->como.literal)) return false;
            chunk_emitir_byte(c->actual->chunk, OP_IGUAL, pl);
            if (*n_saltos >= MATCH_MAX_SALTOS) {
                error_compilacion(c, pl, 0,
                    "patron 'cuando' demasiado complejo (>%d sub-tests)",
                    MATCH_MAX_SALTOS);
                return false;
            }
            saltos[(*n_saltos)++] = emitir_salto(c, OP_SALTAR_SI_FALSO, pl);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, pl);
            return true;

        case PATRON_OR: {
            /* `p1 | p2 | ... | pn`: matchea si alguno coincide.
               Restricción del parser: cada alternativa es LITERAL o
               WILDCARD. Como WILDCARD siempre matchea, su sola
               presencia hace que el OR completo matchee — el resto
               se ignora. Aquí solo manejamos LITERAL en sub-patrones
               (un WILDCARD lo trataríamos como "no test" pero el
               parser podría haberlo simplificado al construir; igual
               cubrimos el caso por defensa). */
            int n = pat->como.estructural.n;
            /* Si hay un WILDCARD, no hay test. */
            for (int i = 0; i < n; i++) {
                if (pat->como.estructural.elementos[i]->tipo == PATRON_WILDCARD) {
                    return true;
                }
            }
            /* Emitir cadena de tests: si alguno coincide, saltar a
               un punto común (L_match_ok) que cae al siguiente código.
               Si todos fallan, el último deja bool false en stack y
               salta al no_match estándar.

               Layout:
                 test_1: navegar; push lit_1; OP_IGUAL;
                         SALTAR_SI_FALSO L_falla_1;
                         OP_DESCARTAR; OP_SALTAR L_match_ok.
                 L_falla_1: OP_DESCARTAR.
                 test_2: ...
                 test_n: navegar; push lit_n; OP_IGUAL;
                         SALTAR_SI_FALSO L_no_match (en saltos[]).
                         OP_DESCARTAR.
                 L_match_ok: (caída natural). */
            int saltos_match_ok[MATCH_MAX_SALTOS];
            int n_saltos_match_ok = 0;
            for (int i = 0; i < n; i++) {
                Patron *alt = pat->como.estructural.elementos[i];
                emitir_navegar(c, slot_sujeto, indices, n_indices, pl);
                if (!compilador_compilar_expr(c, alt->como.literal)) return false;
                chunk_emitir_byte(c->actual->chunk, OP_IGUAL, pl);
                if (i == n - 1) {
                    /* Último: si false, fall through al no_match común. */
                    if (*n_saltos >= MATCH_MAX_SALTOS) {
                        error_compilacion(c, pl, 0,
                            "OR-patron con demasiadas alternativas");
                        return false;
                    }
                    saltos[(*n_saltos)++] = emitir_salto(c, OP_SALTAR_SI_FALSO, pl);
                    chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, pl);
                } else {
                    int salto_falla = emitir_salto(c, OP_SALTAR_SI_FALSO, pl);
                    chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, pl);
                    if (n_saltos_match_ok >= MATCH_MAX_SALTOS) {
                        error_compilacion(c, pl, 0,
                            "OR-patron con demasiadas alternativas");
                        return false;
                    }
                    saltos_match_ok[n_saltos_match_ok++] =
                        emitir_salto(c, OP_SALTAR, pl);
                    /* Aterrizaje de falla del test i: descartar bool false. */
                    parchear_salto(c, salto_falla, pl);
                    chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, pl);
                }
            }
            /* L_match_ok: parchear todos los saltos_match_ok aquí. */
            for (int i = 0; i < n_saltos_match_ok; i++) {
                parchear_salto(c, saltos_match_ok[i], pl);
            }
            return true;
        }

        case PATRON_TUPLA:
        case PATRON_LISTA: {
            if (n_indices >= MATCH_MAX_PROFUNDIDAD) {
                error_compilacion(c, pl, 0,
                    "patron 'cuando' anidado demasiado profundo (>%d niveles)",
                    MATCH_MAX_PROFUNDIDAD);
                return false;
            }
            int n = pat->como.estructural.n;
            OpCode op_es = (pat->tipo == PATRON_TUPLA) ? OP_ES_TUPLA : OP_ES_LISTA;

            /* v1.16.2: detectar PATRON_STAR_BIND. Solo permitido en
               PATRON_LISTA. Solo uno por lista (lo garantiza el parser). */
            int star_idx = -1;
            for (int i = 0; i < n; i++) {
                if (pat->como.estructural.elementos[i]->tipo == PATRON_STAR_BIND) {
                    if (pat->tipo != PATRON_LISTA) {
                        error_compilacion(c, pl, 0,
                            "patron '*' solo permitido en patron de lista");
                        return false;
                    }
                    star_idx = i;
                    break;
                }
            }

            /* 1. Verificar tipo. */
            emitir_navegar(c, slot_sujeto, indices, n_indices, pl);
            chunk_emitir_byte(c->actual->chunk, (uint8_t)op_es, pl);
            if (*n_saltos >= MATCH_MAX_SALTOS) {
                error_compilacion(c, pl, 0,
                    "patron 'cuando' demasiado complejo (>%d sub-tests)",
                    MATCH_MAX_SALTOS);
                return false;
            }
            saltos[(*n_saltos)++] = emitir_salto(c, OP_SALTAR_SI_FALSO, pl);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, pl);

            /* 2. Verificar longitud. */
            emitir_navegar(c, slot_sujeto, indices, n_indices, pl);
            chunk_emitir_byte(c->actual->chunk, OP_LONGITUD, pl);
            if (star_idx < 0) {
                /* Sin star: longitud == n exacto. */
                chunk_emitir_constante(c->actual->chunk,
                                         valor_entero_de_long((long)n), pl);
                chunk_emitir_byte(c->actual->chunk, OP_IGUAL, pl);
            } else {
                /* Con star: longitud >= n - 1 (todos los fijos). */
                chunk_emitir_constante(c->actual->chunk,
                                         valor_entero_de_long((long)(n - 1)), pl);
                chunk_emitir_byte(c->actual->chunk, OP_MAYOR_IGUAL, pl);
            }
            if (*n_saltos >= MATCH_MAX_SALTOS) {
                error_compilacion(c, pl, 0,
                    "patron 'cuando' demasiado complejo (>%d sub-tests)",
                    MATCH_MAX_SALTOS);
                return false;
            }
            saltos[(*n_saltos)++] = emitir_salto(c, OP_SALTAR_SI_FALSO, pl);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, pl);

            /* 3. Recursar por cada elemento (extender path).
               - Antes del star (o todos si no hay star): índice positivo i.
               - El star NO se chequea en verify.
               - Después del star: índice negativo -(n - i). */
            int nuevo_path[MATCH_MAX_PROFUNDIDAD + 1];
            for (int i = 0; i < n_indices; i++) nuevo_path[i] = indices[i];
            for (int i = 0; i < n; i++) {
                Patron *sub = pat->como.estructural.elementos[i];
                if (sub->tipo == PATRON_STAR_BIND) continue;  /* verify skip */
                int idx_runtime;
                if (star_idx < 0 || i < star_idx) {
                    idx_runtime = i;
                } else {
                    /* i > star_idx; índice negativo desde el fin. */
                    idx_runtime = -(n - i);
                }
                nuevo_path[n_indices] = idx_runtime;
                if (!emitir_verify(c, sub, slot_sujeto,
                                    nuevo_path, n_indices + 1,
                                    saltos, n_saltos)) return false;
            }
            return true;
        }

        case PATRON_STAR_BIND:
            /* Solo se invoca desde dentro de PATRON_LISTA (verify pass
               salta los star). Si llegamos aquí, es un error de uso. */
            error_compilacion(c, pl, 0,
                "patron '*' solo permitido como elemento de lista");
            return false;

        case PATRON_TIPO: {
            /* `Foo()`: matchea si sujeto es instancia de Foo (vía
               cadena de superclases). Reusa el built-in `instancia_de`
               emitiendo:
                   push instancia_de (global)
                   push sujeto navegado
                   push Foo (global)
                   OP_LLAMAR 2
                   OP_SALTAR_SI_FALSO no_match
                   OP_DESCARTAR
               Si el usuario sombrea `instancia_de` con su propio
               binding, el patrón usará esa versión. */
            int idx_inst = agregar_nombre_global(c, "instancia_de", 12);
            int idx_clase = agregar_nombre_global(c, pat->como.bind.nombre,
                                                     pat->como.bind.longitud);
            if (idx_inst < 0 || idx_inst > 255
                || idx_clase < 0 || idx_clase > 255) {
                error_compilacion(c, pl, 0,
                    "demasiadas constantes en 'coincidir' (>255)");
                return false;
            }
            /* OP_OBTENER_GLOBAL: 6 bytes (opcode + name_idx + 4 zeros). */
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_GLOBAL,
                                (uint8_t)idx_inst, pl);
            chunk_emitir_byte2(c->actual->chunk, 0, 0, pl);
            chunk_emitir_byte2(c->actual->chunk, 0, 0, pl);
            emitir_navegar(c, slot_sujeto, indices, n_indices, pl);
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_GLOBAL,
                                (uint8_t)idx_clase, pl);
            chunk_emitir_byte2(c->actual->chunk, 0, 0, pl);
            chunk_emitir_byte2(c->actual->chunk, 0, 0, pl);
            chunk_emitir_byte2(c->actual->chunk, OP_LLAMAR, 2, pl);
            if (*n_saltos >= MATCH_MAX_SALTOS) {
                error_compilacion(c, pl, 0,
                    "patron 'cuando' demasiado complejo");
                return false;
            }
            saltos[(*n_saltos)++] = emitir_salto(c, OP_SALTAR_SI_FALSO, pl);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, pl);
            return true;
        }
    }
    return false;
}

static bool emitir_binds(Compilador *c, const Patron *pat,
                           int slot_sujeto,
                           int *indices, int n_indices) {
    int pl = pat->linea;
    switch (pat->tipo) {
        case PATRON_WILDCARD:
        case PATRON_LITERAL:
        case PATRON_OR:
        case PATRON_TIPO:
            /* OR/TIPO: no crean binds en su sub-pattern (TIPO solo
               valida tipo; el binding del sujeto se hace con `como`
               en la cláusula). */
            return true;

        case PATRON_BIND: {
            emitir_navegar(c, slot_sujeto, indices, n_indices, pl);
            int slot = agregar_local(c, pat->como.bind.nombre,
                                          pat->como.bind.longitud, pl);
            if (slot < 0) return false;
            return true;
        }

        case PATRON_TUPLA:
        case PATRON_LISTA: {
            int n = pat->como.estructural.n;
            /* Detectar star idx (solo en PATRON_LISTA por construcción). */
            int star_idx = -1;
            for (int i = 0; i < n; i++) {
                if (pat->como.estructural.elementos[i]->tipo == PATRON_STAR_BIND) {
                    star_idx = i;
                    break;
                }
            }

            int nuevo_path[MATCH_MAX_PROFUNDIDAD + 1];
            for (int i = 0; i < n_indices; i++) nuevo_path[i] = indices[i];
            for (int i = 0; i < n; i++) {
                Patron *sub = pat->como.estructural.elementos[i];
                if (sub->tipo == PATRON_STAR_BIND) {
                    /* Star: emitir slice y bindear. */
                    int cola_count = n - star_idx - 1;
                    /* Push sujeto navegado (a la lista padre del star). */
                    emitir_navegar(c, slot_sujeto, indices, n_indices, pl);
                    /* inicio = star_idx. */
                    chunk_emitir_constante(c->actual->chunk,
                                             valor_entero_de_long((long)star_idx), pl);
                    /* fin = len - cola_count. Si cola_count=0, fin = len. */
                    emitir_navegar(c, slot_sujeto, indices, n_indices, pl);
                    chunk_emitir_byte(c->actual->chunk, OP_LONGITUD, pl);
                    if (cola_count > 0) {
                        chunk_emitir_constante(c->actual->chunk,
                                                 valor_entero_de_long((long)cola_count), pl);
                        chunk_emitir_byte(c->actual->chunk, OP_RESTAR, pl);
                    }
                    /* paso = nulo (default). */
                    chunk_emitir_byte(c->actual->chunk, OP_NULO, pl);
                    chunk_emitir_byte(c->actual->chunk, OP_REBANADA, pl);
                    /* Bindear. Si nombre = "_", aún creamos el local
                       (más simple). */
                    int slot = agregar_local(c, sub->como.bind.nombre,
                                                  sub->como.bind.longitud, pl);
                    if (slot < 0) return false;
                    continue;
                }
                int idx_runtime;
                if (star_idx < 0 || i < star_idx) {
                    idx_runtime = i;
                } else {
                    idx_runtime = -(n - i);
                }
                nuevo_path[n_indices] = idx_runtime;
                if (!emitir_binds(c, sub, slot_sujeto,
                                    nuevo_path, n_indices + 1)) {
                    return false;
                }
            }
            return true;
        }

        case PATRON_STAR_BIND:
            /* Defensa: solo invocado desde dentro de PATRON_LISTA. */
            return true;
    }
    return false;
}

static bool compilar_coincidir(Compilador *c, const Sent *s) {
    int n = s->como.coincidir.n_clausulas;
    if (n > 256) {
        error_compilacion(c, s->linea, s->columna,
            "demasiadas clausulas 'cuando' en 'coincidir' (>256)");
        return false;
    }

    /* Eval sujeto y dejarlo como local anónimo. */
    if (!compilador_compilar_expr(c, s->como.coincidir.sujeto)) return false;
    int slot_sujeto = agregar_local(c, "", 0, s->linea);
    if (slot_sujeto < 0) return false;

    int saltos_fin[256];
    int n_saltos_fin = 0;

    for (int i = 0; i < n; i++) {
        ClausulaCuando *cw = &s->como.coincidir.clausulas[i];

        /* Snapshot del compilador antes de la cláusula. Los binds y
           los temporales se rebobinan al fin de la cláusula para que
           la siguiente vea el estado pre-cláusula. */
        int n_locales_pre = c->actual->n_locales;

        /* Pasada 1: verify. Cada test fallido deja UN bool false en
           stack y salta. Sin tocar locales. */
        int saltos_no_match[MATCH_MAX_SALTOS];
        int n_saltos_no_match = 0;
        if (!emitir_verify(c, cw->patron, slot_sujeto,
                             NULL, 0,
                             saltos_no_match, &n_saltos_no_match)) {
            return false;
        }

        /* Pasada 2: binds. Solo se ejecuta si la verify pasó. Aquí sí
           se crean locales (cada bind hace OP_OBTENER_LOCAL+OP_INDICE
           y agregar_local sobre el TOS resultante). */
        if (!emitir_binds(c, cw->patron, slot_sujeto, NULL, 0)) return false;

        /* v1.16.3: `como <nombre>` opcional — bindea el sujeto entero. */
        if (cw->bind_completo_texto != NULL) {
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                                (uint8_t)slot_sujeto, cw->linea);
            int slot_bc = agregar_local(c, cw->bind_completo_texto,
                                              cw->bind_completo_longitud,
                                              cw->linea);
            if (slot_bc < 0) return false;
        }

        /* Guarda opcional. La guarda puede usar los binds. */
        int salto_guarda_falso = -1;
        if (cw->guarda) {
            if (!compilador_compilar_expr(c, cw->guarda)) return false;
            salto_guarda_falso = emitir_salto(c, OP_SALTAR_SI_FALSO, cw->linea);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, cw->linea);
        }

        /* Cuerpo. */
        if (!compilador_compilar_sent(c, cw->cuerpo)) return false;

        /* Cleanup: descartar los locales creados por los binds (en
           runtime). Restaurar `c->n_locales` para que la siguiente
           cláusula vea el estado correcto. */
        int n_binds = c->actual->n_locales - n_locales_pre;
        for (int j = 0; j < n_binds; j++) {
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, cw->linea);
        }
        c->actual->n_locales = n_locales_pre;

        /* Salto al fin del coincidir. */
        if (n_saltos_fin >= 256) {
            error_compilacion(c, cw->linea, cw->columna,
                "demasiados saltos en 'coincidir'");
            return false;
        }
        saltos_fin[n_saltos_fin++] = emitir_salto(c, OP_SALTAR, cw->linea);

        /* Aterrizajes de fallo. Dos casos:
           - no_match (verify): stack tiene UN bool false, NO hay binds.
           - guarda_falso: stack tiene UN bool false Y los binds creados
             antes de evaluar la guarda.
           Si ambos están presentes, el primero NO debe caer en el
           segundo (descartaría binds inexistentes). Añadimos un salto
           entre ellos a un punto post-aterrizajes. */
        int salto_post = -1;
        if (n_saltos_no_match > 0 && salto_guarda_falso >= 0) {
            /* Caso ambos. */
            for (int j = 0; j < n_saltos_no_match; j++) {
                parchear_salto(c, saltos_no_match[j], cw->linea);
            }
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, cw->linea);
            salto_post = emitir_salto(c, OP_SALTAR, cw->linea);
            parchear_salto(c, salto_guarda_falso, cw->linea);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, cw->linea);
            for (int j = 0; j < n_binds; j++) {
                chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, cw->linea);
            }
            parchear_salto(c, salto_post, cw->linea);
        } else if (n_saltos_no_match > 0) {
            for (int j = 0; j < n_saltos_no_match; j++) {
                parchear_salto(c, saltos_no_match[j], cw->linea);
            }
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, cw->linea);
        } else if (salto_guarda_falso >= 0) {
            parchear_salto(c, salto_guarda_falso, cw->linea);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, cw->linea);
            for (int j = 0; j < n_binds; j++) {
                chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, cw->linea);
            }
        }
    }

    /* Parchear todos los saltos al fin. */
    for (int i = 0; i < n_saltos_fin; i++) {
        parchear_salto(c, saltos_fin[i], s->linea);
    }
    /* Descartar el slot del sujeto del stack runtime y rebobinar el
       compilador. Crítico: si `coincidir` está dentro de un bucle, el
       slot acumularía un valor por iteración. */
    chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
    c->actual->n_locales--;
    return true;
}

/*
 * SENT_MIENTRAS:
 *
 * inicio_cond:
 *   compile cond
 *   OP_SALTAR_SI_FALSO salir
 *   OP_DESCARTAR              ; cond (truthy)
 *   compile cuerpo
 *   OP_BUCLE inicio_cond
 * salir:
 *   OP_DESCARTAR              ; cond (falsy)
 *   compile sino?              ; si la cláusula sino existe
 */
/*
 * Pre-reserva los slots de los nuevos locales que `s` y sus
 * sub-bloques introducen, emitiendo OP_NULO + agregar_local UNA vez
 * antes del bucle. Esto evita el bug v0.11.5b donde dentro de un
 * `mientras`, cada iteración del cuerpo emitía OP_NULO + ASIGNAR
 * para un "nuevo local", haciendo crecer el stack sin límite.
 *
 * Recurre por SENT_BLOQUE y SENT_SI (mismo scope). NO recurre en
 * SENT_FUNCION/SENT_CLASE (scope nuevo). NO recurre en SENT_MIENTRAS/
 * SENT_PARA/SENT_INTENTAR anidados — esos compilarán su propia
 * pre-reserva cuando sea su turno.
 */
static bool pre_reservar_locales(Compilador *c, const Sent *s, int linea_default) {
    if (s == NULL || c->error.tuvo_error) return true;
    switch (s->tipo) {
        case SENT_BLOQUE: {
            for (int i = 0; i < s->como.bloque.n_sentencias; i++) {
                if (!pre_reservar_locales(c, s->como.bloque.sentencias[i],
                                            linea_default))
                    return false;
            }
            return true;
        }
        case SENT_SI: {
            for (int i = 0; i < s->como.si.n_ramas; i++) {
                if (!pre_reservar_locales(c, s->como.si.ramas[i].cuerpo,
                                            linea_default))
                    return false;
            }
            return true;
        }
        case SENT_ASIGNAR: {
            const Expr *destino = s->como.asignar.destino;
            if (destino == NULL) return true;
            if (!c->actual->es_funcion) return true;  /* top-level usa globales */
            /* Caso 1: destino simple `x = ...` */
            if (destino->tipo == EXPR_IDENT) {
                const char *nombre = destino->como.ident.nombre;
                int len = destino->como.ident.longitud;
                int existente = buscar_local(c->actual, nombre, len);
                if (existente >= 0) return true;
                chunk_emitir_byte(c->actual->chunk, OP_NULO, linea_default);
                int slot = agregar_local(c, nombre, len, linea_default);
                if (slot < 0) return false;
                return true;
            }
            /* Caso 2 (v1.123): destructuring `a, b = ...` o `[a, b] = ...`.
             * Recorrer cada destino IDENT y pre-reservar slot si es nuevo.
             * Si el destino es a su vez una tupla/lista anidada, recurrir.
             * Asi, cuando emitir_destructuring se ejecute dentro del bucle,
             * todas las variables seran "existentes" en el scope y reusara
             * sus slots — evita los slots fantasma que crecian el stack. */
            if (destino->tipo == EXPR_TUPLA || destino->tipo == EXPR_LISTA) {
                int n_el = destino->como.secuencia.n_elementos;
                Expr **elementos = destino->como.secuencia.elementos;
                for (int i = 0; i < n_el; i++) {
                    Expr *e = elementos[i];
                    if (e->tipo == EXPR_IDENT) {
                        int existente = buscar_local(c->actual,
                                                       e->como.ident.nombre,
                                                       e->como.ident.longitud);
                        if (existente >= 0) continue;
                        chunk_emitir_byte(c->actual->chunk, OP_NULO,
                                            linea_default);
                        int slot = agregar_local(c, e->como.ident.nombre,
                                                       e->como.ident.longitud,
                                                       linea_default);
                        if (slot < 0) return false;
                    } else if (e->tipo == EXPR_STAR_BIND) {
                        /* v1.134: star_bind tambien necesita pre-reserva.
                         * Sin esto, cuando el SENT_ASIGNAR sintetico que
                         * envuelve `para *previos, x en it:` se compile
                         * DENTRO del loop, emitir_destructuring crearia
                         * un slot nuevo cada iteracion (el OP_NULO se
                         * reejecuta en cada vuelta), creciendo el stack. */
                        int existente = buscar_local(c->actual,
                                                       e->como.star_bind.nombre,
                                                       e->como.star_bind.longitud);
                        if (existente >= 0) continue;
                        chunk_emitir_byte(c->actual->chunk, OP_NULO,
                                            linea_default);
                        int slot = agregar_local(c,
                                                  e->como.star_bind.nombre,
                                                  e->como.star_bind.longitud,
                                                  linea_default);
                        if (slot < 0) return false;
                    } else if (e->tipo == EXPR_TUPLA || e->tipo == EXPR_LISTA) {
                        /* Anidado: simular un SENT_ASIGNAR con este
                         * destino para reusar la logica. Construimos un
                         * SENT temporal en el stack. */
                        Sent fake;
                        fake.tipo = SENT_ASIGNAR;
                        fake.como.asignar.destino = e;
                        fake.como.asignar.valor = NULL;
                        if (!pre_reservar_locales(c, &fake, linea_default))
                            return false;
                    }
                    /* Otros tipos de destino (EXPR_INDICE, EXPR_ATRIBUTO):
                     * no se pueden destructurar a ellos directamente; lo
                     * detectara emitir_destructuring en su validacion. */
                }
                return true;
            }
            return true;
        }
        default:
            /* SENT_MIENTRAS/SENT_PARA/SENT_INTENTAR/SENT_FUNCION/
               SENT_CLASE/etc. no descienden — manejan sus propios
               locales cuando se compilen (compilar_mientras y
               compilar_para llaman a pre_reservar_locales sobre su
               cuerpo, lo cual cubre el caso interno).
               Nota: una version anterior de v1.123 si descendia en
               bucles, pero rompia `nolocal n` porque pre-reservaba
               n como local de la funcion interna antes de procesar
               la declaracion `nolocal`. Mantener el comportamiento
               original es mas seguro; el caso destructuring de
               variables nuevas dentro de un bucle se cubre con la
               extension de SENT_ASIGNAR a EXPR_TUPLA/EXPR_LISTA. */
            return true;
    }
}

static bool compilar_mientras(Compilador *c, const Sent *s) {
    /* v0.11.5b fix: pre-reservar slots de nuevos locales del cuerpo
       (incluyendo recursión por SENT_BLOQUE y SENT_SI) antes del
       bucle. Sin esto, el cuerpo emitiría OP_NULO+ASIGNAR cada
       iteración haciendo crecer el stack sin límite. */
    int n_locales_entrada = c->actual->n_locales;
    if (c->actual->es_funcion) {
        if (!pre_reservar_locales(c, s->como.mientras.cuerpo, s->linea))
            return false;
    }
    int inicio_cond = c->actual->chunk->cuenta;
    if (!compilador_compilar_expr(c, s->como.mientras.condicion)) return false;
    int salto_salir = emitir_salto(c, OP_SALTAR_SI_FALSO, s->linea);
    chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);

    if (!empujar_bucle(c, inicio_cond, s->linea)) return false;
    if (!compilador_compilar_sent(c, s->como.mientras.cuerpo)) return false;
    emitir_bucle(c, inicio_cond, s->linea);

    /* salir: */
    parchear_salto(c, salto_salir, s->linea);
    chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);

    /* Cláusula sino: ejecutada si terminamos por condición falsa.
     * Emitimos directamente después del descart. Los `romper` ya
     * fueron parcheados en cerrar_bucle; pero queremos que `romper`
     * salte AL FIN de todo, NO al sino. Por eso parcheamos el cierre
     * del bucle DESPUÉS de la cláusula sino. */
    if (s->como.mientras.sino != NULL) {
        if (!compilador_compilar_sent(c, s->como.mientras.sino)) return false;
    }

    cerrar_bucle(c, s->linea);

    /* v1.130 fix: limpiar los locales pre-reservados al salir del while
     * (mismo cleanup que compilar_para). Sin esto los slots quedaban
     * vivos en el stack mientras el compilador creia que estaban
     * libres, y un `para` interno colocado dentro de un `mientras`
     * dentro de otro `para` exterior crasheaba con "OP_ITER_SIGUIENTE
     * sin iterador en slot N" desde la segunda iteracion del exterior.
     * Causa: el OP_NULO del pre_reservar se ejecutaba cada iteracion
     * del exterior, creciendo el stack +N, y el slot calculado para
     * $iter del `para` interno (en compile time) ya no apuntaba al
     * iter en runtime. Workaround documentado en stdlib/grafos.cor:
     * componentes (v1.119) usaba mientras+indice manual; eliminable
     * tras este fix. */
    {
        int drops = c->actual->n_locales - n_locales_entrada;
        for (int j = 0; j < drops; j++) {
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
        }
    }
    c->actual->n_locales = n_locales_entrada;
    return true;
}

bool compilador_compilar_sent(Compilador *c, const Sent *s) {
    if (c->error.tuvo_error) return false;

    switch (s->tipo) {
        case SENT_PASAR:
            return true;

        case SENT_EXPR:
            if (!compilador_compilar_expr(c, s->como.expr.expr)) return false;
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
            return true;

        case SENT_ASIGNAR:
            return compilar_asignar(c, s);

        case SENT_ASIGNAR_AUG:
            return compilar_asignar_aug(c, s);

        case SENT_SI:
            return compilar_si(c, s);

        case SENT_COINCIDIR:
            return compilar_coincidir(c, s);

        case SENT_MIENTRAS:
            return compilar_mientras(c, s);

        case SENT_ROMPER: {
            if (!bucle_actual(c)) {
                error_compilacion(c, s->linea, s->columna,
                    "'romper' fuera de un bucle");
                return false;
            }
            int salto = emitir_salto(c, OP_SALTAR, s->linea);
            return registrar_parche_romper(c, salto, s->linea);
        }
        case SENT_CONTINUAR: {
            BucleAbierto *b = bucle_actual(c);
            if (!b) {
                error_compilacion(c, s->linea, s->columna,
                    "'continuar' fuera de un bucle");
                return false;
            }
            emitir_bucle(c, b->inicio_continuar, s->linea);
            return true;
        }

        case SENT_BLOQUE: {
            int n = s->como.bloque.n_sentencias;
            for (int i = 0; i < n; i++) {
                if (!compilador_compilar_sent(c, s->como.bloque.sentencias[i])) {
                    return false;
                }
            }
            return true;
        }

        case SENT_FUNCION:
            return compilar_funcion(c, s);

        case SENT_RETORNAR: {
            if (!c->actual->es_funcion) {
                error_compilacion(c, s->linea, s->columna,
                    "'retornar' fuera de una funcion");
                return false;
            }
            if (s->como.retornar.valor != NULL) {
                if (!compilador_compilar_expr(c, s->como.retornar.valor)) return false;
            } else {
                chunk_emitir_byte(c->actual->chunk, OP_NULO, s->linea);
            }
            chunk_emitir_byte(c->actual->chunk, OP_RETORNAR, s->linea);
            return true;
        }

        case SENT_PRODUCIR: {
            if (!c->actual->es_funcion) {
                error_compilacion(c, s->linea, s->columna,
                    "'producir' fuera de una funcion");
                return false;
            }
            if (!compilador_compilar_expr(c, s->como.producir.valor)) return false;
            chunk_emitir_byte(c->actual->chunk, OP_PRODUCIR, s->linea);
            /* Marca el scope actual como generador. */
            if (c->actual->funcion) {
                c->actual->funcion->es_generador = true;
            }
            return true;
        }

        case SENT_PARA:
            return compilar_para(c, s);

        case SENT_INTENTAR:
            return compilar_intentar(c, s);

        case SENT_LANZAR: {
            if (s->como.lanzar.valor == NULL) {
                /* Re-raise: válido dentro de cualquier `atrapar` (con
                   o sin alias — v1.14). Lee el slot interno donde el
                   compilador guardó la excepción al hacer match y
                   re-emite OP_LANZAR. */
                if (c->n_atrapadores_activos == 0) {
                    error_compilacion(c, s->linea, s->columna,
                        "'lanzar' sin valor solo es valido dentro de 'atrapar'");
                    return false;
                }
                int slot = c->atrapador_alias_slots[c->n_atrapadores_activos - 1];
                chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                                    (uint8_t)slot, s->linea);
                chunk_emitir_byte(c->actual->chunk, OP_LANZAR, s->linea);
                return true;
            }
            if (!compilador_compilar_expr(c, s->como.lanzar.valor)) return false;
            chunk_emitir_byte(c->actual->chunk, OP_LANZAR, s->linea);
            return true;
        }

        case SENT_CLASE:
            return compilar_clase(c, s);

        case SENT_IMPORTAR: {
            /*
             * v0.9.1: soporta subsegmentos (`mat.geometria`) y alias
             * (`importar X como Y`). El nombre del módulo se construye
             * uniendo segmentos con `.`. Si no hay alias, el nombre
             * para el binding global es el último segmento (no la
             * cadena completa, para coherencia con `mat.geometria`
             * accediéndose como `geometria.foo`). Con alias, el alias.
             */
            int n_seg = s->como.importar.n_segmentos;
            if (n_seg < 1) {
                error_compilacion(c, s->linea, s->columna,
                    "importar requiere al menos un segmento");
                return false;
            }

            /* Construir el nombre del módulo: seg1.seg2.segN */
            int total_len = 0;
            for (int i = 0; i < n_seg; i++) {
                total_len += s->como.importar.segmentos[i].longitud;
                if (i > 0) total_len += 1;   /* `.` separador */
            }
            char *nombre_modulo = (char *)malloc((size_t)total_len + 1);
            if (!nombre_modulo) {
                error_compilacion(c, s->linea, s->columna, "memoria insuficiente");
                return false;
            }
            int pos = 0;
            for (int i = 0; i < n_seg; i++) {
                if (i > 0) nombre_modulo[pos++] = '.';
                memcpy(nombre_modulo + pos,
                       s->como.importar.segmentos[i].texto,
                       (size_t)s->como.importar.segmentos[i].longitud);
                pos += s->como.importar.segmentos[i].longitud;
            }
            nombre_modulo[total_len] = '\0';

            /* Determinar el binding name: alias si existe, sino el
               último segmento. */
            const char *binding_text;
            int binding_len;
            if (s->como.importar.alias.texto != NULL) {
                binding_text = s->como.importar.alias.texto;
                binding_len = s->como.importar.alias.longitud;
            } else {
                const Nombre *ultimo = &s->como.importar.segmentos[n_seg - 1];
                binding_text = ultimo->texto;
                binding_len = ultimo->longitud;
            }

            /* Constantes: nombre_modulo y binding_name. */
            int idx_modulo = chunk_agregar_constante(c->actual->chunk,
                valor_cadena_duplicar(nombre_modulo, total_len));
            free(nombre_modulo);
            int idx_binding = chunk_agregar_constante(c->actual->chunk,
                valor_cadena_duplicar(binding_text, binding_len));
            if (idx_modulo < 0 || idx_modulo > 255
                || idx_binding < 0 || idx_binding > 255) {
                error_compilacion(c, s->linea, s->columna,
                    "demasiadas constantes para v0.9 (operando byte)");
                return false;
            }
            chunk_emitir_byte(c->actual->chunk, OP_IMPORTAR, s->linea);
            chunk_emitir_byte(c->actual->chunk, (uint8_t)idx_modulo, s->linea);
            chunk_emitir_byte(c->actual->chunk, (uint8_t)idx_binding, s->linea);
            /* OP_RETORNAR del frame del módulo empuja `nulo` al stack
               del importador. Como `importar` es una sentencia (no
               expresión), descartamos ese nulo aquí. */
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
            return true;
        }

        case SENT_DESDE_IMPORTAR: {
            /*
             * v0.9.1: `desde X importar Y, Z` — carga el módulo (en
             * cache si no estaba), extrae `X.Y` y `X.Z`, y los registra
             * como globales del importador (con alias si los items lo
             * llevan). El nombre del módulo NO queda como global.
             *
             * Estrategia bytecode:
             *   OP_IMPORTAR_PARA_DESDE [name_idx]      ; pushea mod a stack
             *   por cada item:
             *     OP_DUP                                ; clona mod
             *     OP_OBTENER_ATRIBUTO [item_name_idx]   ; mod, attr → attr
             *     OP_DEFINIR_GLOBAL [binding_idx]       ; pop attr a global
             *   OP_DESCARTAR                            ; quita el último mod
             *
             * `desde X importar *` no soportado en v0.9.1.
             */
            if (s->como.desde_importar.importa_todo) {
                error_compilacion(c, s->linea, s->columna,
                    "'desde X importar *' no esta soportado en v0.9.1");
                return false;
            }
            int n_seg = s->como.desde_importar.n_segmentos_modulo;
            int n_items = s->como.desde_importar.n_items;
            if (n_seg < 1 || n_items < 1) {
                error_compilacion(c, s->linea, s->columna,
                    "'desde X importar Y' requiere al menos un segmento y un item");
                return false;
            }

            /* Construir nombre del módulo: seg1.seg2.segN */
            int total_len = 0;
            for (int i = 0; i < n_seg; i++) {
                total_len += s->como.desde_importar.segmentos_modulo[i].longitud;
                if (i > 0) total_len += 1;
            }
            char *nombre_modulo = (char *)malloc((size_t)total_len + 1);
            if (!nombre_modulo) {
                error_compilacion(c, s->linea, s->columna, "memoria insuficiente");
                return false;
            }
            int pos = 0;
            for (int i = 0; i < n_seg; i++) {
                if (i > 0) nombre_modulo[pos++] = '.';
                memcpy(nombre_modulo + pos,
                       s->como.desde_importar.segmentos_modulo[i].texto,
                       (size_t)s->como.desde_importar.segmentos_modulo[i].longitud);
                pos += s->como.desde_importar.segmentos_modulo[i].longitud;
            }
            nombre_modulo[total_len] = '\0';

            int idx_modulo = chunk_agregar_constante(c->actual->chunk,
                valor_cadena_duplicar(nombre_modulo, total_len));
            free(nombre_modulo);
            if (idx_modulo < 0 || idx_modulo > 255) {
                error_compilacion(c, s->linea, s->columna,
                    "demasiadas constantes para v0.9 (operando byte)");
                return false;
            }

            chunk_emitir_byte2(c->actual->chunk, OP_IMPORTAR_PARA_DESDE,
                                (uint8_t)idx_modulo, s->linea);

            for (int i = 0; i < n_items; i++) {
                const ItemImportado *it = &s->como.desde_importar.items[i];
                const Nombre *attr = &it->nombre;
                const Nombre *binding = it->alias.texto != NULL
                    ? &it->alias : &it->nombre;

                chunk_emitir_byte(c->actual->chunk, OP_DUP, it->linea);

                int idx_attr = chunk_agregar_constante(c->actual->chunk,
                    valor_cadena_duplicar(attr->texto, attr->longitud));
                if (idx_attr < 0 || idx_attr > 255) {
                    error_compilacion(c, it->linea, it->columna,
                        "demasiadas constantes");
                    return false;
                }
                chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_ATRIBUTO,
                                    (uint8_t)idx_attr, it->linea);
                /* 4 bytes de cache (v0.10 / F10) — ver nota en
                   EXPR_ATRIBUTO arriba. */
                chunk_emitir_byte2(c->actual->chunk, 0, 0, it->linea);
                chunk_emitir_byte2(c->actual->chunk, 0, 0, it->linea);

                int idx_bind = chunk_agregar_constante(c->actual->chunk,
                    valor_cadena_duplicar(binding->texto, binding->longitud));
                if (idx_bind < 0 || idx_bind > 255) {
                    error_compilacion(c, it->linea, it->columna,
                        "demasiadas constantes");
                    return false;
                }
                chunk_emitir_byte2(c->actual->chunk, OP_DEFINIR_GLOBAL,
                                    (uint8_t)idx_bind, it->linea);
            }

            /* Quitar el módulo restante del stack. */
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
            return true;
        }

        case SENT_NOLOCAL: {
            /*
             * v1.4: declara que las variables listadas pertenecen a un
             * scope envolvente, NO al actual. En Cornamusa la
             * asignación a una variable existente en un scope padre YA
             * va a ese scope por default (semántica Lua), así que
             * `nolocal` es principalmente:
             *   1. Documentación explícita de la intención.
             *   2. Validación temprana — error si el nombre no existe
             *      como local en ningún padre.
             *   3. Marca para evitar que una asignación POSTERIOR a un
             *      `agregar_local` "shadow accidental" cree una local.
             *      (En Cornamusa esto no ocurre por la regla actual,
             *      pero registrar el marker hace que cualquier cambio
             *      futuro al resolver mantenga la garantía.)
             *
             * Validación inmediata: cada nombre debe existir como local
             * en algún scope envolvente (no en el actual).
             */
            if (!c->actual->es_funcion) {
                error_compilacion(c, s->linea, s->columna,
                    "ErrorDeSintaxis: `nolocal` solo se permite dentro de una funcion");
                return false;
            }
            int n = s->como.global_o_nolocal.n_nombres;
            for (int i = 0; i < n; i++) {
                const Nombre *nm = &s->como.global_o_nolocal.nombres[i];
                /* No debe ya ser local del scope actual. */
                if (buscar_local(c->actual, nm->texto, nm->longitud) >= 0) {
                    error_compilacion(c, s->linea, s->columna,
                        "ErrorDeSintaxis: '%.*s' es local del scope actual; "
                        "no se puede declarar `nolocal`",
                        nm->longitud, nm->texto);
                    return false;
                }
                /* Debe encontrarse en algún padre. resolver_upvalue
                 * busca recursivamente en padres y registra como
                 * upvalue del scope actual; aprovechamos su efecto
                 * lateral para que la asignación posterior tenga el
                 * upvalue ya registrado. */
                int upv = resolver_upvalue(c, c->actual,
                                              nm->texto, nm->longitud,
                                              s->linea);
                if (upv < 0) {
                    error_compilacion(c, s->linea, s->columna,
                        "ErrorDeNombre: `nolocal %.*s` pero el nombre no "
                        "existe en ningun scope envolvente",
                        nm->longitud, nm->texto);
                    return false;
                }
                /* Registrar marker. Si excedemos el cap, es un error
                 * raro (programas con muchas declaraciones nolocal). */
                if (c->actual->n_nolocales >= COMPILADOR_NOLOCALES_MAX) {
                    error_compilacion(c, s->linea, s->columna,
                        "demasiadas declaraciones `nolocal` en un mismo scope");
                    return false;
                }
                c->actual->nolocales[c->actual->n_nolocales].nombre = nm->texto;
                c->actual->nolocales[c->actual->n_nolocales].longitud_nombre = nm->longitud;
                c->actual->n_nolocales++;
            }
            return true;
        }
        case SENT_GLOBAL: {
            /*
             * v1.57: declara que los nombres listados pertenecen al
             * scope de modulo (top-level). Asignaciones y lecturas
             * posteriores se enrutan a OP_DEFINIR_GLOBAL /
             * OP_OBTENER_GLOBAL en lugar de local/upvalue.
             *
             * A diferencia de `nolocal`, NO requerimos que el nombre
             * ya exista a nivel modulo — su primera asignacion puede
             * ser la que lo crea (semantica Python).
             */
            if (!c->actual->es_funcion) {
                error_compilacion(c, s->linea, s->columna,
                    "ErrorDeSintaxis: `global` solo se permite dentro de una funcion");
                return false;
            }
            int n = s->como.global_o_nolocal.n_nombres;
            for (int i = 0; i < n; i++) {
                const Nombre *nm = &s->como.global_o_nolocal.nombres[i];
                /* No debe ya ser local del scope actual (eso seria
                 * contradictorio). */
                if (buscar_local(c->actual, nm->texto, nm->longitud) >= 0) {
                    error_compilacion(c, s->linea, s->columna,
                        "ErrorDeSintaxis: '%.*s' es local del scope actual; "
                        "no se puede declarar `global`",
                        nm->longitud, nm->texto);
                    return false;
                }
                /* Tampoco debe estar marcada como `nolocal`. */
                for (int j = 0; j < c->actual->n_nolocales; j++) {
                    NolocalMarker *m = &c->actual->nolocales[j];
                    if (m->longitud_nombre == nm->longitud
                        && memcmp(m->nombre, nm->texto, (size_t)nm->longitud) == 0) {
                        error_compilacion(c, s->linea, s->columna,
                            "ErrorDeSintaxis: '%.*s' ya declarada `nolocal`; "
                            "no puede ser tambien `global`",
                            nm->longitud, nm->texto);
                        return false;
                    }
                }
                /* Idempotencia: si ya esta marcado global, no duplicar. */
                if (es_global_declarado(c->actual, nm->texto, nm->longitud)) {
                    continue;
                }
                if (c->actual->n_globales >= COMPILADOR_NOLOCALES_MAX) {
                    error_compilacion(c, s->linea, s->columna,
                        "demasiadas declaraciones `global` en un mismo scope");
                    return false;
                }
                c->actual->globales[c->actual->n_globales].nombre = nm->texto;
                c->actual->globales[c->actual->n_globales].longitud_nombre = nm->longitud;
                c->actual->n_globales++;
            }
            return true;
        }

        case SENT_BORRAR: {
            /* v1.56: `borrar d[k]` o `borrar obj.attr`. */
            Expr *destino = s->como.borrar.destino;
            if (destino == NULL) {
                error_compilacion(c, s->linea, s->columna,
                    "ErrorDeSintaxis: 'borrar' requiere un destino");
                return false;
            }
            if (destino->tipo == EXPR_INDICE) {
                /* Compila obj, indice, OP_BORRAR_INDICE. */
                if (!compilador_compilar_expr(c, destino->como.indice.objeto)) return false;
                if (!compilador_compilar_expr(c, destino->como.indice.indice)) return false;
                chunk_emitir_byte(c->actual->chunk, OP_BORRAR_INDICE, s->linea);
                return true;
            }
            if (destino->tipo == EXPR_ATRIBUTO) {
                if (!compilador_compilar_expr(c, destino->como.atributo.objeto)) return false;
                int idx = chunk_agregar_constante(c->actual->chunk,
                    valor_cadena_duplicar(destino->como.atributo.nombre,
                                            destino->como.atributo.longitud));
                if (idx < 0 || idx > 255) {
                    error_compilacion(c, s->linea, s->columna,
                        "demasiadas constantes (operando byte) para 'borrar obj.attr'");
                    return false;
                }
                chunk_emitir_byte2(c->actual->chunk, OP_BORRAR_ATRIBUTO,
                                    (uint8_t)idx, s->linea);
                return true;
            }
            error_compilacion(c, s->linea, s->columna,
                "ErrorDeSintaxis: 'borrar' requiere `d[k]` o `obj.attr` como destino");
            return false;
        }
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
       nulo como "valor del programa". */
    int linea_final = (n > 0) ? sents[n - 1]->linea : 1;
    chunk_emitir_byte(c->actual->chunk, OP_NULO, linea_final);
    chunk_emitir_byte(c->actual->chunk, OP_RETORNAR, linea_final);
    return true;
}

/*
 * v1.5: detector de "dunder inlinable". Si el cuerpo de la función
 * encaja en un patrón trivial reconocido, llena `fn->inline_desc`
 * para que la VM pueda saltar la creación de CallFrame al
 * despachar el dunder.
 *
 * Patrones soportados:
 *   `retornar yo.A OP otro.B`  →  DUNDER_INLINE_BIN_ATTR_OP_ATTR
 *
 * Restricciones:
 *   - El cuerpo debe ser exactamente UN SENT_RETORNAR (en SENT_BLOQUE
 *     de 1 elemento o directo).
 *   - La expresión retornada debe ser EXPR_BINARIO con op aritmético
 *     o de comparación.
 *   - Los operandos deben ser EXPR_ATRIBUTO sobre IDENT `yo` (izq) y
 *     IDENT del segundo parámetro (der). Caso especial: el segundo
 *     parámetro es típicamente `otro` pero puede ser cualquier nombre.
 *
 * Sólo aplica a funciones de aridad 2 (yo + 1 arg). Las cadenas se
 * duplican en heap; las libera `funcion_bc_liberar`.
 */
/*
 * v1.7: detector para `__iniciar__` trivial con exactamente 2 args:
 *   funcion __iniciar__(yo, p1, p2):
 *     yo.A = p1
 *     yo.B = p2
 *   fin funcion
 * Llena `fn->inline_desc` con tipo INIT_INLINE_TRIVIAL_2 si encaja.
 *
 * Restrictivo a propósito (solo 2 args) para limitar el espacio de
 * estados a manejar en runtime. Cubre el caso `Vector` típico.
 */
static void detectar_init_inline(FuncionBC *fn, const Sent *fn_def) {
    if (fn->aridad != 3) return;  /* yo + 2 params */
    Sent *cuerpo = fn_def->como.funcion.cuerpo;
    if (!cuerpo || cuerpo->tipo != SENT_BLOQUE) return;
    if (cuerpo->como.bloque.n_sentencias != 2) return;
    if (fn_def->como.funcion.n_parametros != 2) return;

    Sent *s1 = cuerpo->como.bloque.sentencias[0];
    Sent *s2 = cuerpo->como.bloque.sentencias[1];
    if (!s1 || s1->tipo != SENT_ASIGNAR) return;
    if (!s2 || s2->tipo != SENT_ASIGNAR) return;

    /* Cada destino: EXPR_ATRIBUTO sobre IDENT yo. */
    Expr *d1 = s1->como.asignar.destino;
    Expr *d2 = s2->como.asignar.destino;
    if (!d1 || d1->tipo != EXPR_ATRIBUTO) return;
    if (!d2 || d2->tipo != EXPR_ATRIBUTO) return;
    Expr *o1 = d1->como.atributo.objeto;
    Expr *o2 = d2->como.atributo.objeto;
    if (!o1 || o1->tipo != EXPR_IDENT) return;
    if (!o2 || o2->tipo != EXPR_IDENT) return;

    /* Slot 0 nombre = la propia función; slots 1+ = params. Verificamos
       que `yo` es el primer parámetro (típicamente literal "yo"). */
    const Parametro *p_yo = &fn_def->como.funcion.parametros[0];
    const Parametro *p1 = &fn_def->como.funcion.parametros[1];
    const Parametro *p2_p = NULL;
    /* n_parametros = 2 → el segundo es índice [1] desde nuestra cuenta? */
    /* Re-leyendo: parametros = lista de N params. p[0]=yo, p[1]=p1, p[2]=p2.
       Pero ¿n_parametros incluye yo? Mirando contexto, sí lo incluye
       porque la aridad coincide. */
    /* fn->aridad == 3 implica n_parametros == 3 (yo + 2). */
    if (fn_def->como.funcion.n_parametros < 3) return;
    p1 = &fn_def->como.funcion.parametros[1];
    p2_p = &fn_def->como.funcion.parametros[2];

    /* Verificar que destino[i] es yo.* */
    #define IDENT_MATCH(nodo, p) \
        ((nodo)->como.ident.longitud == (p)->longitud_nombre \
         && memcmp((nodo)->como.ident.nombre, (p)->nombre, \
                    (size_t)(p)->longitud_nombre) == 0)

    if (!IDENT_MATCH(o1, p_yo)) return;
    if (!IDENT_MATCH(o2, p_yo)) return;

    /* Cada valor: EXPR_IDENT del param correspondiente. */
    Expr *v1 = s1->como.asignar.valor;
    Expr *v2 = s2->como.asignar.valor;
    if (!v1 || v1->tipo != EXPR_IDENT) return;
    if (!v2 || v2->tipo != EXPR_IDENT) return;
    if (!IDENT_MATCH(v1, p1)) return;
    if (!IDENT_MATCH(v2, p2_p)) return;

    #undef IDENT_MATCH

    /* Patron OK: duplicar nombres de atributos y guardar. */
    int la = d1->como.atributo.longitud;
    int lb = d2->como.atributo.longitud;
    char *a1 = (char *)malloc((size_t)la + 1);
    char *a2 = (char *)malloc((size_t)lb + 1);
    if (!a1 || !a2) { free(a1); free(a2); return; }
    memcpy(a1, d1->como.atributo.nombre, (size_t)la); a1[la] = '\0';
    memcpy(a2, d2->como.atributo.nombre, (size_t)lb); a2[lb] = '\0';
    fn->inline_desc.tipo = INIT_INLINE_TRIVIAL_2;
    fn->inline_desc.init_attr1 = a1;
    fn->inline_desc.init_attr1_len = la;
    fn->inline_desc.init_attr2 = a2;
    fn->inline_desc.init_attr2_len = lb;
}

/*
 * Detector de patrón binario simple: `yo.A OP otro.B`. Devuelve true
 * si encaja y rellena los punteros (alocados en heap). Reusable para
 * el caso `__sumar__` simple y para los args del constructor.
 */
static bool extraer_attr_op_attr(const Expr *e,
                                   const Parametro *p_yo,
                                   const Parametro *p_otro,
                                   char **out_attr_yo, int *out_len_yo,
                                   char **out_attr_otro, int *out_len_otro,
                                   int *out_op) {
    if (!e || e->tipo != EXPR_BINARIO) return false;
    Expr *izq = e->como.binario.izq;
    Expr *der = e->como.binario.der;
    if (!izq || izq->tipo != EXPR_ATRIBUTO) return false;
    if (!der || der->tipo != EXPR_ATRIBUTO) return false;
    Expr *izq_obj = izq->como.atributo.objeto;
    Expr *der_obj = der->como.atributo.objeto;
    if (!izq_obj || izq_obj->tipo != EXPR_IDENT) return false;
    if (!der_obj || der_obj->tipo != EXPR_IDENT) return false;
    if (izq_obj->como.ident.longitud != p_yo->longitud_nombre
        || memcmp(izq_obj->como.ident.nombre, p_yo->nombre,
                   (size_t)p_yo->longitud_nombre) != 0) return false;
    if (der_obj->como.ident.longitud != p_otro->longitud_nombre
        || memcmp(der_obj->como.ident.nombre, p_otro->nombre,
                   (size_t)p_otro->longitud_nombre) != 0) return false;
    TipoToken op = e->como.binario.op;
    switch (op) {
        case TT_MAS: case TT_MENOS: case TT_ASTERISCO: case TT_BARRA:
        case TT_DOBLE_BARRA: case TT_PORCENTAJE: case TT_DOBLE_ASTERISCO:
        case TT_IGUAL: case TT_DISTINTO:
        case TT_MENOR: case TT_MENOR_IGUAL:
        case TT_MAYOR: case TT_MAYOR_IGUAL:
            break;
        default:
            return false;
    }
    int la = izq->como.atributo.longitud;
    int lb = der->como.atributo.longitud;
    char *a1 = (char *)malloc((size_t)la + 1);
    char *a2 = (char *)malloc((size_t)lb + 1);
    if (!a1 || !a2) { free(a1); free(a2); return false; }
    memcpy(a1, izq->como.atributo.nombre, (size_t)la); a1[la] = '\0';
    memcpy(a2, der->como.atributo.nombre, (size_t)lb); a2[lb] = '\0';
    *out_attr_yo = a1;
    *out_len_yo = la;
    *out_attr_otro = a2;
    *out_len_otro = lb;
    *out_op = (int)op;
    return true;
}

/*
 * v1.7: detector para `__sumar__/etc.` con constructor de 2 args:
 *   funcion __sumar__(yo, otro):
 *     retornar V(yo.A OP otro.B, yo.C OP2 otro.D)
 *   fin funcion
 */
static bool detectar_dunder_ctor(FuncionBC *fn, const Sent *fn_def, Expr *e) {
    if (fn->aridad != 2) return false;
    if (fn_def->como.funcion.n_parametros != 2) return false;
    if (!e || e->tipo != EXPR_LLAMADA) return false;
    if (e->como.llamada.n_args != 2) return false;
    Expr *callee = e->como.llamada.callee;
    if (!callee || callee->tipo != EXPR_IDENT) return false;
    const Parametro *p_yo = &fn_def->como.funcion.parametros[0];
    const Parametro *p_otro = &fn_def->como.funcion.parametros[1];
    /* Cada arg debe ser yo.A OP otro.B. */
    char *a1y = NULL, *a1o = NULL;
    char *a2y = NULL, *a2o = NULL;
    int l1y = 0, l1o = 0, l2y = 0, l2o = 0;
    int op1 = 0, op2 = 0;
    if (!extraer_attr_op_attr(e->como.llamada.args[0], p_yo, p_otro,
                                &a1y, &l1y, &a1o, &l1o, &op1)) {
        return false;
    }
    if (!extraer_attr_op_attr(e->como.llamada.args[1], p_yo, p_otro,
                                &a2y, &l2y, &a2o, &l2o, &op2)) {
        free(a1y); free(a1o);
        return false;
    }
    /* Duplicar nombre de clase. */
    int lc = callee->como.ident.longitud;
    char *cls = (char *)malloc((size_t)lc + 1);
    if (!cls) { free(a1y); free(a1o); free(a2y); free(a2o); return false; }
    memcpy(cls, callee->como.ident.nombre, (size_t)lc);
    cls[lc] = '\0';
    fn->inline_desc.tipo = DUNDER_INLINE_BIN_CTOR_2;
    fn->inline_desc.attr_yo = a1y;
    fn->inline_desc.len_attr_yo = l1y;
    fn->inline_desc.attr_otro = a1o;
    fn->inline_desc.len_attr_otro = l1o;
    fn->inline_desc.op_token = op1;
    fn->inline_desc.nombre_clase = cls;
    fn->inline_desc.len_nombre_clase = lc;
    fn->inline_desc.ctor_arg2_attr_yo = a2y;
    fn->inline_desc.ctor_arg2_len_yo = l2y;
    fn->inline_desc.ctor_arg2_attr_otro = a2o;
    fn->inline_desc.ctor_arg2_len_otro = l2o;
    fn->inline_desc.ctor_arg2_op = op2;
    return true;
}

static void detectar_inline_dunder(FuncionBC *fn, const Sent *fn_def) {
    fn->inline_desc.tipo = DUNDER_INLINE_NONE;
    if (!fn_def || fn_def->tipo != SENT_FUNCION) return;
    Sent *cuerpo = fn_def->como.funcion.cuerpo;
    if (!cuerpo || cuerpo->tipo != SENT_BLOQUE) return;

    /* v1.7: detectar __iniciar__ trivial (2 args). */
    detectar_init_inline(fn, fn_def);
    if (fn->inline_desc.tipo != DUNDER_INLINE_NONE) return;

    if (cuerpo->como.bloque.n_sentencias != 1) return;
    Sent *body = cuerpo->como.bloque.sentencias[0];
    if (!body || body->tipo != SENT_RETORNAR) return;
    Expr *e = body->como.retornar.valor;
    if (!e) return;

    /* v1.7: detectar __sumar__/etc. con constructor (`retornar V(...)`). */
    if (detectar_dunder_ctor(fn, fn_def, e)) return;

    /* v1.6: patrón unario `retornar yo.A`. Aridad 1, cuerpo es
       EXPR_ATRIBUTO sobre IDENT del primer parámetro. */
    if (fn->aridad == 1 && e->tipo == EXPR_ATRIBUTO) {
        Expr *obj = e->como.atributo.objeto;
        if (!obj || obj->tipo != EXPR_IDENT) return;
        if (fn_def->como.funcion.n_parametros != 1) return;
        const Parametro *p0 = &fn_def->como.funcion.parametros[0];
        if (obj->como.ident.longitud != p0->longitud_nombre
            || memcmp(obj->como.ident.nombre, p0->nombre,
                       (size_t)p0->longitud_nombre) != 0) return;
        int la = e->como.atributo.longitud;
        char *attr = (char *)malloc((size_t)la + 1);
        if (!attr) return;
        memcpy(attr, e->como.atributo.nombre, (size_t)la);
        attr[la] = '\0';
        fn->inline_desc.tipo = DUNDER_INLINE_UNARIO_ATTR;
        fn->inline_desc.attr_yo = attr;
        fn->inline_desc.len_attr_yo = la;
        return;
    }

    if (fn->aridad != 2) return;
    if (e->tipo != EXPR_BINARIO) return;
    Expr *izq = e->como.binario.izq;
    Expr *der = e->como.binario.der;
    if (!izq || izq->tipo != EXPR_ATRIBUTO) return;
    if (!der || der->tipo != EXPR_ATRIBUTO) return;
    /* izq.objeto debe ser ident `yo`. */
    Expr *izq_obj = izq->como.atributo.objeto;
    Expr *der_obj = der->como.atributo.objeto;
    if (!izq_obj || izq_obj->tipo != EXPR_IDENT) return;
    if (!der_obj || der_obj->tipo != EXPR_IDENT) return;
    /* Slot 1 es `yo` por convención del scope. Slot 2 es el segundo
       param (típicamente `otro`). Verificamos por nombre del primer
       parámetro y del segundo. */
    if (fn_def->como.funcion.n_parametros != 2) return;
    const Parametro *p0 = &fn_def->como.funcion.parametros[0];
    const Parametro *p1 = &fn_def->como.funcion.parametros[1];
    if (izq_obj->como.ident.longitud != p0->longitud_nombre
        || memcmp(izq_obj->como.ident.nombre, p0->nombre,
                   (size_t)p0->longitud_nombre) != 0) {
        return;
    }
    if (der_obj->como.ident.longitud != p1->longitud_nombre
        || memcmp(der_obj->como.ident.nombre, p1->nombre,
                   (size_t)p1->longitud_nombre) != 0) {
        return;
    }
    /* Op debe ser aritmético/comparación (no `es`/`en`/lógicos). */
    TipoToken op = e->como.binario.op;
    switch (op) {
        case TT_MAS:
        case TT_MENOS:
        case TT_ASTERISCO:
        case TT_BARRA:
        case TT_DOBLE_BARRA:
        case TT_PORCENTAJE:
        case TT_DOBLE_ASTERISCO:
        case TT_IGUAL:
        case TT_DISTINTO:
        case TT_MENOR:
        case TT_MENOR_IGUAL:
        case TT_MAYOR:
        case TT_MAYOR_IGUAL:
            break;
        default:
            return;
    }
    /* Duplicar nombres de atributos en heap. */
    int la = izq->como.atributo.longitud;
    int lb = der->como.atributo.longitud;
    char *attr_yo = (char *)malloc((size_t)la + 1);
    char *attr_otro = (char *)malloc((size_t)lb + 1);
    if (!attr_yo || !attr_otro) {
        free(attr_yo); free(attr_otro);
        return;
    }
    memcpy(attr_yo, izq->como.atributo.nombre, (size_t)la);
    attr_yo[la] = '\0';
    memcpy(attr_otro, der->como.atributo.nombre, (size_t)lb);
    attr_otro[lb] = '\0';
    fn->inline_desc.tipo = DUNDER_INLINE_BIN_ATTR_OP_ATTR;
    fn->inline_desc.attr_yo = attr_yo;
    fn->inline_desc.len_attr_yo = la;
    fn->inline_desc.attr_otro = attr_otro;
    fn->inline_desc.len_attr_otro = lb;
    fn->inline_desc.op_token = (int)op;
}

/*
 * Helper compartido por funciones (`SENT_FUNCION`) y métodos de clase
 * (cuerpo de `SENT_CLASE`): compila el cuerpo de la función como una
 * `FuncionBC` independiente y emite **OP_CLOSURE** en el chunk actual,
 * dejando la closure recién creada en el tope del stack.
 *
 * NO emite código de binding (DEFINIR_GLOBAL / ASIGNAR_LOCAL / METODO):
 * eso lo decide el llamador.
 */
static bool emitir_closure_de_funcion(Compilador *c, const Sent *s) {
    const char *nombre = s->como.funcion.nombre;
    int len_nombre = s->como.funcion.longitud_nombre;
    int n_params = s->como.funcion.n_parametros;
    Parametro *params = s->como.funcion.parametros;

    /* v1.17: validar defaults — solo permitidos en la cola. Contar n_defaults.
       v1.22: `*resto` debe ser el último (o penúltimo si hay `**kw`).
       v1.24: `**kw` SIEMPRE debe ser el último. */
    int n_defaults = 0;
    bool vio_default = false;
    bool tiene_estrella = false;
    bool tiene_doble_estrella = false;
    int idx_doble = -1;
    int idx_estrella = -1;
    for (int i = 0; i < n_params; i++) {
        if (params[i].es_doble_estrella) {
            if (tiene_doble_estrella) {
                error_compilacion(c, s->linea, s->columna,
                    "solo se permite un '**kw'");
                return false;
            }
            tiene_doble_estrella = true;
            idx_doble = i;
        } else if (params[i].es_estrella) {
            if (tiene_estrella) {
                error_compilacion(c, s->linea, s->columna,
                    "solo se permite un '*resto'");
                return false;
            }
            tiene_estrella = true;
            idx_estrella = i;
        } else if (params[i].valor_defecto != NULL) {
            vio_default = true;
            n_defaults++;
        } else if (vio_default) {
            error_compilacion(c, s->linea, s->columna,
                "parametro sin valor por defecto despues de uno con default");
            return false;
        }
    }
    if (tiene_doble_estrella && idx_doble != n_params - 1) {
        error_compilacion(c, s->linea, s->columna,
            "'**kw' debe ser el ultimo parametro");
        return false;
    }
    if (tiene_estrella) {
        int esperado = tiene_doble_estrella ? n_params - 2 : n_params - 1;
        if (idx_estrella != esperado) {
            error_compilacion(c, s->linea, s->columna,
                "'*resto' debe ir justo antes de '**kw' o ser el ultimo");
            return false;
        }
    }
    if ((tiene_estrella || tiene_doble_estrella) && n_defaults > 0) {
        error_compilacion(c, s->linea, s->columna,
            "variádicos no se combinan con defaults (v1.24)");
        return false;
    }

    FuncionBC *fn = funcion_bc_nueva(nombre, len_nombre, n_params);
    if (!fn) {
        error_compilacion(c, s->linea, s->columna, "memoria insuficiente");
        return false;
    }

    ScopeCompilador scope_fn;
    scope_iniciar(&scope_fn, &fn->chunk, true, c->actual);
    scope_fn.funcion = fn;
    scope_fn.locales[0].nombre = nombre;
    scope_fn.locales[0].longitud_nombre = len_nombre;
    for (int i = 0; i < n_params; i++) {
        if (scope_fn.n_locales >= COMPILADOR_LOCALES_MAX) {
            error_compilacion(c, s->linea, s->columna,
                "demasiados parametros");
            funcion_bc_liberar(fn);
            return false;
        }
        scope_fn.locales[scope_fn.n_locales].nombre = params[i].nombre;
        scope_fn.locales[scope_fn.n_locales].longitud_nombre = params[i].longitud_nombre;
        scope_fn.locales[scope_fn.n_locales].capturado = false;
        scope_fn.n_locales++;
    }

    ScopeCompilador *prev = c->actual;
    c->actual = &scope_fn;
    bool ok = compilador_compilar_sent(c, s->como.funcion.cuerpo);
    chunk_emitir_byte(&fn->chunk, OP_NULO, s->linea);
    chunk_emitir_byte(&fn->chunk, OP_RETORNAR, s->linea);
    c->actual = prev;

    if (!ok) {
        funcion_bc_liberar(fn);
        return false;
    }

    /* v1.5: detector de dunder inlinable. Si el cuerpo encaja en un
       patrón trivial reconocido, llena `fn->inline_desc` para que la
       VM pueda fast-pathear el dispatch. */
    detectar_inline_dunder(fn, s);

    /* v1.17: registrar n_defaults en la plantilla. La VM lo lee al
       procesar OP_CLOSURE para saber cuántos valores pop del stack. */
    fn->n_defaults = n_defaults;
    /* v1.22: registrar si tiene `*resto`. */
    fn->tiene_estrella = tiene_estrella;
    /* v1.24: registrar si tiene `**kw`. */
    fn->tiene_doble_estrella = tiene_doble_estrella;
    /* v1.23: duplicar nombres de parámetros para matching de kwargs. */
    if (n_params > 0) {
        fn->nombres_params = (char **)malloc(sizeof(char *) * (size_t)n_params);
        fn->long_nombres_params = (int *)malloc(sizeof(int) * (size_t)n_params);
        if (!fn->nombres_params || !fn->long_nombres_params) {
            error_compilacion(c, s->linea, s->columna, "memoria insuficiente");
            funcion_bc_liberar(fn);
            return false;
        }
        for (int i = 0; i < n_params; i++) {
            int ln = params[i].longitud_nombre;
            char *copia = (char *)malloc((size_t)ln + 1);
            if (!copia) {
                error_compilacion(c, s->linea, s->columna, "memoria insuficiente");
                funcion_bc_liberar(fn);
                return false;
            }
            if (ln > 0) memcpy(copia, params[i].nombre, (size_t)ln);
            copia[ln] = '\0';
            fn->nombres_params[i] = copia;
            fn->long_nombres_params[i] = ln;
        }
    }

    Valor v_plantilla = valor_plantilla(fn);
    int fn_idx = chunk_agregar_constante(c->actual->chunk, v_plantilla);
    if (fn_idx < 0 || fn_idx > 255) {
        error_compilacion(c, s->linea, s->columna,
            "demasiadas constantes para v0.7 (operando byte)");
        return false;
    }
    /* v1.17: antes de OP_CLOSURE, emitir las expresiones de default en
       orden (primer default → primero en stack). Las evalúan en el
       scope donde se DEFINE la función (Python-like: defaults se
       capturan al `def`, no al call). */
    for (int i = n_params - n_defaults; i < n_params; i++) {
        if (!compilador_compilar_expr(c, params[i].valor_defecto)) return false;
    }
    chunk_emitir_byte2(c->actual->chunk, OP_CLOSURE,
                        (uint8_t)fn_idx, s->linea);
    for (int i = 0; i < scope_fn.n_upvalues; i++) {
        chunk_emitir_byte(c->actual->chunk,
            scope_fn.upvalues[i].es_local ? 1 : 0, s->linea);
        chunk_emitir_byte(c->actual->chunk,
            scope_fn.upvalues[i].indice, s->linea);
    }
    return true;
}

/*
 * Compila una declaración de función:
 *   funcion nombre(p1, p2, ...): cuerpo fin funcion
 *
 * Construye la closure (vía `emitir_closure_de_funcion`) y la registra
 * como local (dentro de función) o como global (top-level).
 */
static bool compilar_funcion(Compilador *c, const Sent *s) {
    const char *nombre = s->como.funcion.nombre;
    int len_nombre = s->como.funcion.longitud_nombre;

    if (!emitir_closure_de_funcion(c, s)) return false;

    /* La closure recién creada está en el tope del stack. */
    int slot_local = -1;
    int idx_global = -1;
    if (c->actual->es_funcion) {
        /* Verificar si ya existe un local con ese nombre (redefinir). */
        int existente = buscar_local(c->actual, nombre, len_nombre);
        if (existente >= 0) {
            chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                (uint8_t)existente, s->linea);
            slot_local = existente;
        } else {
            int slot = agregar_local(c, nombre, len_nombre, s->linea);
            if (slot < 0) return false;
            slot_local = slot;
            /* OLD convention para nuevo local: el closure ya está en
             * el slot por la convención "tope = n_locales". */
        }
    } else {
        int idx_nombre = agregar_nombre_global(c, nombre, len_nombre);
        if (idx_nombre < 0 || idx_nombre > 255) {
            error_compilacion(c, s->linea, s->columna,
                "demasiadas constantes para v0.6 (operando byte)");
            return false;
        }
        chunk_emitir_byte2(c->actual->chunk, OP_DEFINIR_GLOBAL,
                            (uint8_t)idx_nombre, s->linea);
        idx_global = idx_nombre;
    }

    /* v1.72: aplicar decoradores. `@a` + `@b` + `funcion f` produce
     * `f = a(b(f))`. Iteramos de adentro hacia afuera (orden inverso
     * al fuente): para cada decorador, empujamos dec(f) y reasignamos. */
    int n_decs = s->como.funcion.n_decoradores;
    Expr **decs = s->como.funcion.decoradores;
    for (int i = n_decs - 1; i >= 0; i--) {
        /* Compilar la expresión del decorador → top del stack. */
        if (!compilador_compilar_expr(c, decs[i])) return false;
        /* Obtener el valor actual de la función (debajo del decorador). */
        if (slot_local >= 0) {
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                                (uint8_t)slot_local, s->linea);
        } else {
            /* OP_OBTENER_GLOBAL es de 6 bytes (v0.10/F10): opcode +
             * name_idx + 4 bytes de cache. */
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_GLOBAL,
                                (uint8_t)idx_global, s->linea);
            chunk_emitir_byte2(c->actual->chunk, 0, 0, s->linea);
            chunk_emitir_byte2(c->actual->chunk, 0, 0, s->linea);
        }
        /* Llamar el decorador con la función actual como argumento. */
        chunk_emitir_byte2(c->actual->chunk, OP_LLAMAR, 1, s->linea);
        /* Reasignar el resultado al mismo nombre. Ambos ASIGNAR_* hacen
         * pop del valor, no dejan nada en el stack. */
        if (slot_local >= 0) {
            chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                (uint8_t)slot_local, s->linea);
        } else {
            chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_GLOBAL,
                                (uint8_t)idx_global, s->linea);
        }
    }
    return true;
}

/*
 * SENT_PARA en bytecode:
 *
 *   compile iterable
 *   OP_ITER_INICIAR              ; pop iterable, push iterador
 *   OP_ASIGNAR_LOCAL [iter_slot] ; pop iterador a slot oculto
 * inicio_loop:
 *   OP_ITER_SIGUIENTE [iter_slot] [u16 offset_salir]
 *   ASIGNAR objetivo             ; el valor está en tope
 *   compile cuerpo
 *   OP_BUCLE inicio_loop
 * salir:
 *   compile sino?                ; si la cláusula sino existe
 *   (los `romper` saltan a aquí, después del sino)
 *
 * El iterador vive en un slot del frame durante toda la iteración,
 * para que el cuerpo pueda usar locales arbitrarios sin interferir.
 *
 * Funciona tanto en top-level como dentro de función gracias a que
 * los locales también son válidos en el scope raíz desde v0.6.1.
 */
static bool compilar_para(Compilador *c, const Sent *s) {
    Expr *objetivo = s->como.para.objetivo;
    if (objetivo->tipo != EXPR_IDENT) {
        error_compilacion(c, s->linea, s->columna,
            "ErrorDeSintaxis: objetivo de 'para' debe ser un identificador");
        return false;
    }

    /* Guardar n_locales para restaurar al salir (mismo motivo que en
       compilar_intentar: locals transitorios — $iter, objetivo en
       función — no deben quedar como "ocupados" tras el bucle). */
    int n_locales_entrada = c->actual->n_locales;

    /* Compilar iterable y emitir OP_ITER_INICIAR. El iterador queda
       en el tope del stack como un local oculto. */
    if (!compilador_compilar_expr(c, s->como.para.iterable)) return false;
    chunk_emitir_byte(c->actual->chunk, OP_ITER_INICIAR, s->linea);

    static const char NOMBRE_ITER_OCULTO[] = "$iter";
    int iter_slot = agregar_local(c, NOMBRE_ITER_OCULTO, 5, s->linea);
    if (iter_slot < 0) return false;
    /* OLD convention: el iter ya está en el tope tras OP_ITER_INICIAR
       y queda en su slot por la convención "tope = n_locales". Esta
       convención sigue funcionando para locales transitorias cuya vida
       cabe dentro de un solo path lineal. */

    /*
     * Si el objetivo es local (estamos en función o se trata como
     * tal), preasignamos su slot con OP_NULO para que la primera
     * iteración pueda hacer un OP_ASIGNAR_LOCAL "destruyendo el nulo
     * y asignando el primer valor".
     *
     * En top-level, el objetivo se trata como global y se asigna con
     * OP_DEFINIR_GLOBAL en cada iteración (sin slot reservado).
     */
    int objetivo_slot = -1;
    if (c->actual->es_funcion) {
        int existente = buscar_local(c->actual, objetivo->como.ident.nombre,
                                        objetivo->como.ident.longitud);
        if (existente >= 0) {
            objetivo_slot = existente;
        } else {
            chunk_emitir_byte(c->actual->chunk, OP_NULO, s->linea);
            objetivo_slot = agregar_local(c, objetivo->como.ident.nombre,
                                              objetivo->como.ident.longitud,
                                              s->linea);
            if (objetivo_slot < 0) return false;
        }
    }

    /* v1.32 fix: pre-reservar slots para nuevos locales del cuerpo del
       `para` ANTES de capturar inicio_loop. El bug previo (v0.11.5b
       puso pre_reservar DENTRO del loop): el OP_NULO de cada slot se
       re-ejecutaba en cada iteración, creciendo el stack +1 por vuelta.
       Tolerable para variables simples (slot fijo se sigue leyendo
       bien aunque haya basura encima), pero ROMPÍA comprehensions o
       bucles anidados que dependen de `tope == n_locales` (su
       `$comp_iter` quedaba en slot equivocado → "OP_ITER_SIGUIENTE
       sin iterador"). Emitirlos UNA vez antes del loop arregla ambos. */
    if (c->actual->es_funcion) {
        if (!pre_reservar_locales(c, s->como.para.cuerpo, s->linea))
            return false;
    }

    /* inicio_loop: aquí saltan `continuar` y el OP_BUCLE final. */
    int inicio_loop = c->actual->chunk->cuenta;

    /* OP_ITER_SIGUIENTE [byte iter_slot] [u16 offset_fin]. */
    chunk_emitir_byte2(c->actual->chunk, OP_ITER_SIGUIENTE,
                        (uint8_t)iter_slot, s->linea);
    chunk_emitir_byte(c->actual->chunk, 0xff, s->linea);
    chunk_emitir_byte(c->actual->chunk, 0xff, s->linea);
    int offset_placeholder = c->actual->chunk->cuenta - 2;

    /* Asignar el valor producido por SIGUIENTE al objetivo. */
    if (objetivo_slot >= 0) {
        chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                            (uint8_t)objetivo_slot, s->linea);
    } else {
        int idx = agregar_nombre_global(c, objetivo->como.ident.nombre,
                                          objetivo->como.ident.longitud);
        if (idx < 0 || idx > 255) {
            error_compilacion(c, s->linea, s->columna,
                "demasiadas constantes para v0.6 (operando byte)");
            return false;
        }
        chunk_emitir_byte2(c->actual->chunk, OP_DEFINIR_GLOBAL,
                            (uint8_t)idx, s->linea);
    }

    if (!empujar_bucle(c, inicio_loop, s->linea)) return false;

    /* pre_reservar ya se hizo antes de inicio_loop (v1.32 fix). */

    if (!compilador_compilar_sent(c, s->como.para.cuerpo)) return false;

    emitir_bucle(c, inicio_loop, s->linea);

    /* Patchear el salto de OP_ITER_SIGUIENTE al fin del bucle. */
    parchear_salto(c, offset_placeholder, s->linea);

    if (s->como.para.sino != NULL) {
        if (!compilador_compilar_sent(c, s->como.para.sino)) return false;
    }

    cerrar_bucle(c, s->linea);
    /* Limpiar locals introducidos por el bucle ($iter, target si es
       función-scope, locals declaradas en el cuerpo). Sin esto los
       slots quedan ocupados con basura al volver al contexto exterior. */
    {
        int drops = c->actual->n_locales - n_locales_entrada;
        for (int j = 0; j < drops; j++) {
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
        }
    }
    c->actual->n_locales = n_locales_entrada;
    return true;
}

/*
 * SENT_INTENTAR en bytecode (v0.8.3 — completo).
 *
 * Soporta:
 *   - Múltiples atrapadores con discriminación por tipo (nombre).
 *   - `atrapar Excepcion as e:` atrapa todo (tipo genérico).
 *   - `atrapar Tipo as e:` solo si excepción.clase == "Tipo".
 *   - `sino:` ejecuta solo si NO hubo excepción.
 *   - `finalmente:` ejecuta SIEMPRE: tras salida limpia, tras cada
 *     atrapar exitoso, y antes del re-lanzar si ningún atrapador
 *     coincide. (Limitación: NO se ejecuta cuando hay un `retornar`,
 *     `romper` o `continuar` que sale del intentar — llega en
 *     v0.8.4 si se necesita.)
 *   - `lanzar` sin valor (re-raise) dentro de un atrapar con alias.
 *
 * Estructura emitida:
 *
 *   OP_INTENTAR_INICIAR offset_handler
 *   ... cuerpo ...
 *   OP_INTENTAR_FIN
 *   ... [opcional] sino ...
 *   ... [opcional] finalmente ...
 *   OP_SALTAR fin
 *
 * [handler]:
 *   ; excepción en stack[-1].
 *   por cada atrapador:
 *     ; chequear tipo (si lo tiene):
 *     OP_COMPROBAR_TIPO_EXC name_idx       ; push bool sin descartar exc
 *     OP_SALTAR_SI_FALSO siguiente_atr
 *     OP_DESCARTAR                          ; pop bool
 *     ; (sin tipo: saltar el chequeo y caer aquí directamente)
 *     ... gestionar alias o descartar exc ...
 *     ... compile cuerpo ...
 *     ... compile finalmente (si hay) ...
 *     OP_SALTAR fin
 *   siguiente_atr:
 *     OP_DESCARTAR                          ; pop bool del COMPROBAR previo
 *
 *   ; ningún atrapador coincidió — re-lanzar.
 *   ... compile finalmente (si hay) ...
 *   OP_LANZAR                                ; exc todavía en stack
 *
 * [fin]:
 */
static bool compilar_intentar(Compilador *c, const Sent *s) {
    int n_atrapadores = s->como.intentar.n_atrapadores;
    Sent *clausula_sino = s->como.intentar.sino;
    Sent *clausula_finalmente = s->como.intentar.finalmente;

    if (n_atrapadores == 0 && clausula_finalmente == NULL) {
        error_compilacion(c, s->linea, s->columna,
            "'intentar' requiere al menos un 'atrapar' o 'finalmente'");
        return false;
    }

    /*
     * Guardamos n_locales al entrar para restaurarlo al salir. Los
     * locals declarados dentro del intentar (alias de atrapadores,
     * locals en cuerpos de atrapar) son transitorios — solo válidos
     * dentro del bloque. Sin esta restauración, el compilador pierde
     * sincronía con el runtime stack, que es especialmente crítico en
     * top-level donde múltiples bloques intentar comparten scope.
     */
    int n_locales_entrada = c->actual->n_locales;

    /* Emitir OP_INTENTAR_INICIAR con offset placeholder. */
    int salto_handler = emitir_salto(c, OP_INTENTAR_INICIAR, s->linea);

    /* Compilar el cuerpo del intentar. */
    if (!compilador_compilar_sent(c, s->como.intentar.cuerpo)) return false;

    /* Salida limpia: pop handler y (opcional) ejecutar sino. */
    chunk_emitir_byte(c->actual->chunk, OP_INTENTAR_FIN, s->linea);
    if (clausula_sino != NULL) {
        if (!compilador_compilar_sent(c, clausula_sino)) return false;
    }
    /* (Salida limpia) ejecutar finalmente si lo hay, después salto al fin. */
    if (clausula_finalmente != NULL) {
        if (!compilador_compilar_sent(c, clausula_finalmente)) return false;
    }
    /*
     * Limpiar locals introducidos durante el body o el sino (drops del
     * runtime stack hasta volver a la posición de entrada). Necesario
     * porque sin esto los slots de los locals quedan ocupados con
     * basura, y bloques posteriores al intentar leen valores stale.
     */
    {
        int drops = c->actual->n_locales - n_locales_entrada;
        for (int j = 0; j < drops; j++) {
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
        }
    }
    /* Saltos al fin desde rutas exitosas (salida limpia + cada atrapar). */
    int saltos_fin[COMPILADOR_ATRAPADORES_MAX + 1];
    int n_saltos_fin = 0;
    saltos_fin[n_saltos_fin++] = emitir_salto(c, OP_SALTAR, s->linea);

    /* Aquí empieza el handler (parchear el OP_INTENTAR_INICIAR). */
    parchear_salto(c, salto_handler, s->linea);

    /*
     * En el momento de entrar al handler, el runtime garantiza que la
     * excepción está en el slot `n_locales_handler` (mismo número que
     * n_locales tenía al entrar al intentar). Para que cada atrapador
     * vea la excepción en el mismo slot, reseteamos n_locales a este
     * valor al inicio de cada atrapador. Locals declaradas dentro de
     * un cuerpo de atrapador son válidas solo dentro de ese cuerpo
     * (mutuamente exclusivas con otros atrapadores).
     */
    int n_locales_handler = c->actual->n_locales;

    /* Para cada atrapador, comprobar tipo (si tiene) y ejecutar cuerpo. */
    int salto_anterior_no_match = -1;
    for (int i = 0; i < n_atrapadores; i++) {
        ClausulaAtrapar *atr = &s->como.intentar.atrapadores[i];

        /* Reset de locals al estado pre-handler para que cada
           atrapador asigne sus locals (incluyendo el alias) en los
           mismos slots, coincidiendo con la posición real en stack. */
        c->actual->n_locales = n_locales_handler;

        /* Si hubo un atrapador anterior con tipo, parchear su salto
           "no match" aquí. Antes hacer pop del bool del COMPROBAR. */
        if (salto_anterior_no_match >= 0) {
            parchear_salto(c, salto_anterior_no_match, atr->linea);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, atr->linea);
        }
        salto_anterior_no_match = -1;

        if (atr->tipo != NULL) {
            /* Limitación v0.8.3: solo soportamos `atrapar Tipo` con
               Tipo siendo un identificador simple. Se compara la
               cadena del nombre del identificador con excepcion.clase. */
            if (atr->tipo->tipo != EXPR_IDENT) {
                error_compilacion(c, atr->linea, atr->columna,
                    "'atrapar Tipo' solo admite un identificador simple en v0.8.3");
                return false;
            }
            int idx_nombre = chunk_agregar_constante(c->actual->chunk,
                valor_cadena_duplicar(atr->tipo->como.ident.nombre,
                                        atr->tipo->como.ident.longitud));
            if (idx_nombre < 0 || idx_nombre > 255) {
                error_compilacion(c, atr->linea, atr->columna,
                    "demasiadas constantes para v0.8 (operando byte)");
                return false;
            }
            chunk_emitir_byte2(c->actual->chunk, OP_COMPROBAR_TIPO_EXC,
                                (uint8_t)idx_nombre, atr->linea);
            /* Si bool=falso, salta al siguiente atrapador. */
            salto_anterior_no_match = emitir_salto(c, OP_SALTAR_SI_FALSO,
                                                     atr->linea);
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, atr->linea);
        }

        /* Match: gestionar alias o descartar la excepción.
           Siempre añadimos un nuevo local para el alias (no reusamos
           el slot de un atrapador previo): los slots reusados causarían
           que la asignación pop/write desplazara `tope` por debajo del
           local existente. Cada atrapador tiene su propio scope
           conceptual; locals con el mismo nombre se sombrean de modo
           que `buscar_local` (que itera de n_locales-1 hacia abajo)
           encuentra siempre el más reciente. */
        /* v1.14: incluso sin alias, asignamos la excepción a un local
           con nombre vacío. Permite que `lanzar` sin valor (re-raise)
           funcione dentro de cualquier `atrapar`, no solo los que
           tienen alias. `buscar_local` con len > 0 nunca encuentra un
           slot anónimo, así que no hay shadowing. */
        const char *nombre_local;
        int len_nombre_local;
        if (atr->alias.texto != NULL) {
            nombre_local = atr->alias.texto;
            len_nombre_local = atr->alias.longitud;
        } else {
            nombre_local = "";   /* slot anónimo, no buscable por nombre */
            len_nombre_local = 0;
        }
        int alias_slot = agregar_local(c, nombre_local, len_nombre_local,
                                            atr->linea);
        if (alias_slot < 0) return false;
        /* La excepción está en el tope cuando se agregar_local; cada
           atrapador resetea n_locales al handler-entry, así que el
           slot es consistente. Push al stack de atrapadores activos
           para `lanzar` sin valor (re-raise). */
        if (c->n_atrapadores_activos >= COMPILADOR_ATRAPADORES_MAX) {
            error_compilacion(c, atr->linea, atr->columna,
                "anidamiento de atrapadores excede %d",
                COMPILADOR_ATRAPADORES_MAX);
            return false;
        }
        c->atrapador_alias_slots[c->n_atrapadores_activos++] = alias_slot;

        /* Compilar cuerpo del atrapar. */
        if (!compilador_compilar_sent(c, atr->cuerpo)) return false;

        if (alias_slot >= 0) {
            c->n_atrapadores_activos--;
        }

        /* Tras atrapar exitoso: ejecutar finalmente si lo hay, después
           limpiar locals (alias + locals declaradas dentro del cuerpo)
           y saltar al fin. */
        if (clausula_finalmente != NULL) {
            if (!compilador_compilar_sent(c, clausula_finalmente)) return false;
        }
        {
            int drops = c->actual->n_locales - n_locales_entrada;
            for (int j = 0; j < drops; j++) {
                chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, atr->linea);
            }
        }
        if (n_saltos_fin >= COMPILADOR_ATRAPADORES_MAX + 1) {
            error_compilacion(c, s->linea, s->columna,
                "demasiados atrapadores");
            return false;
        }
        saltos_fin[n_saltos_fin++] = emitir_salto(c, OP_SALTAR, atr->linea);
    }

    /* Si ningún atrapador coincidió: ejecutar finalmente y re-lanzar.
       La excepción todavía está en stack. */
    if (salto_anterior_no_match >= 0) {
        parchear_salto(c, salto_anterior_no_match, s->linea);
        chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, s->linea);
    }
    if (clausula_finalmente != NULL) {
        if (!compilador_compilar_sent(c, clausula_finalmente)) return false;
    }
    chunk_emitir_byte(c->actual->chunk, OP_LANZAR, s->linea);

    /* Parchear todos los saltos al fin. */
    for (int i = 0; i < n_saltos_fin; i++) {
        parchear_salto(c, saltos_fin[i], s->linea);
    }

    /* Restaurar n_locales — los locals introducidos dentro del intentar
       (aliases, locals de cuerpos) son transitorios. */
    c->actual->n_locales = n_locales_entrada;
    return true;
}

/*
 * SENT_CLASE en bytecode (Fase 8 v0.7.0).
 *
 * Plan de compilación (con métodos S2):
 *   OP_CLASE [name_idx]              ; clase en stack
 *   por cada método m en cuerpo:
 *     emitir_closure_de_funcion(m)   ; closure encima de la clase
 *     OP_METODO [m_name_idx]         ; pop closure, set clase.metodos[m]
 *   binding (DEFINIR_GLOBAL / local) ; pop clase y guardarla
 *
 * El cuerpo de la clase admite:
 *   - SENT_PASAR (no-op).
 *   - SENT_FUNCION (declaración de método).
 * Otras sentencias se rechazan con error claro.
 *
 * Limitaciones de v0.7.0:
 *   - Sin herencia: `extiende` se rechaza (llega en S4).
 *   - Sin atributos de clase ni statements arbitrarios en el cuerpo.
 */
static bool compilar_clase(Compilador *c, const Sent *s) {
    if (s->como.clase.n_superclases > 1) {
        error_compilacion(c, s->linea, s->columna,
            "herencia multiple aun no esta en bytecode v0.7.0 (solo un padre permitido)");
        return false;
    }

    /* Empujar el nombre como constante y emitir OP_CLASE. */
    int idx_nombre = chunk_agregar_constante(c->actual->chunk,
        valor_cadena_duplicar(s->como.clase.nombre,
                                s->como.clase.longitud_nombre));
    if (idx_nombre < 0 || idx_nombre > 255) {
        error_compilacion(c, s->linea, s->columna,
            "demasiadas constantes para v0.7 (operando byte)");
        return false;
    }
    chunk_emitir_byte2(c->actual->chunk, OP_CLASE,
                        (uint8_t)idx_nombre, s->linea);

    /* Si tiene un padre: compilar la expresión del padre, dejándola
       en el tope. Después OP_HEREDAR pop super, copia métodos en la
       clase, deja la clase en el tope para los métodos siguientes. */
    if (s->como.clase.n_superclases == 1) {
        if (!compilador_compilar_expr(c, s->como.clase.superclases[0])) return false;
        chunk_emitir_byte(c->actual->chunk, OP_HEREDAR, s->linea);
    }

    /* Recorrer el cuerpo emitiendo OP_METODO por cada SENT_FUNCION. */
    Sent *cuerpo = s->como.clase.cuerpo;
    Sent **items = NULL;
    int n_items = 0;
    if (cuerpo && cuerpo->tipo == SENT_BLOQUE) {
        items = cuerpo->como.bloque.sentencias;
        n_items = cuerpo->como.bloque.n_sentencias;
    } else if (cuerpo) {
        /* Cuerpo de una sola sentencia (one-liner). */
        items = &cuerpo;
        n_items = 1;
    }

    for (int i = 0; i < n_items; i++) {
        Sent *body = items[i];
        if (body->tipo == SENT_PASAR) continue;
        if (body->tipo != SENT_FUNCION) {
            error_compilacion(c, body->linea, body->columna,
                "el cuerpo de una clase solo admite metodos ('funcion ...') o 'pasar' en v0.7.0");
            return false;
        }
        /* Emitir la closure del método. La clase sigue en el stack
           debajo. Tras OP_CLOSURE el stack es [..., clase, closure]. */
        if (!emitir_closure_de_funcion(c, body)) return false;
        /* v1.77: aplicar decoradores en orden inverso al fuente. Cada
         * decorador recibe la closure actual y debe devolver una
         * nueva. Patron por iteracion:
         *   [..., clase, closure]
         *   compilar(dec)            → [..., clase, closure, dec]
         *   OP_INTERCAMBIAR          → [..., clase, dec, closure]
         *   OP_LLAMAR 1              → [..., clase, dec(closure)]
         * Tras todos los decoradores, OP_METODO toma el resultado
         * final. */
        int n_decs = body->como.funcion.n_decoradores;
        Expr **decs = body->como.funcion.decoradores;
        for (int j = n_decs - 1; j >= 0; j--) {
            if (!compilador_compilar_expr(c, decs[j])) return false;
            chunk_emitir_byte(c->actual->chunk, OP_INTERCAMBIAR, body->linea);
            chunk_emitir_byte2(c->actual->chunk, OP_LLAMAR, 1, body->linea);
        }
        /* OP_METODO pops la closure y la guarda en clase.metodos[name];
           la clase queda en el tope del stack. */
        int idx_metodo = chunk_agregar_constante(c->actual->chunk,
            valor_cadena_duplicar(body->como.funcion.nombre,
                                    body->como.funcion.longitud_nombre));
        if (idx_metodo < 0 || idx_metodo > 255) {
            error_compilacion(c, body->linea, body->columna,
                "demasiadas constantes para v0.7 (operando byte)");
            return false;
        }
        chunk_emitir_byte2(c->actual->chunk, OP_METODO,
                            (uint8_t)idx_metodo, body->linea);
    }

    /* Registrar la clase como local o global. */
    if (c->actual->es_funcion) {
        int existente = buscar_local(c->actual, s->como.clase.nombre,
                                        s->como.clase.longitud_nombre);
        if (existente >= 0) {
            chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                (uint8_t)existente, s->linea);
        } else {
            int slot = agregar_local(c, s->como.clase.nombre,
                                          s->como.clase.longitud_nombre,
                                          s->linea);
            if (slot < 0) return false;
            /* OLD convention para nuevo local. */
        }
    } else {
        int idx_g = agregar_nombre_global(c, s->como.clase.nombre,
                                              s->como.clase.longitud_nombre);
        if (idx_g < 0 || idx_g > 255) {
            error_compilacion(c, s->linea, s->columna,
                "demasiadas constantes para v0.7 (operando byte)");
            return false;
        }
        chunk_emitir_byte2(c->actual->chunk, OP_DEFINIR_GLOBAL,
                            (uint8_t)idx_g, s->linea);
    }
    return true;
}
