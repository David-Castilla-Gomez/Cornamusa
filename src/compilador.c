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
            /* Prioridad: local del scope actual → upvalue (búsqueda
               recursiva en scopes padres) → global. */
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
            for (int i = 0; i < n_params; i++) {
                if (params[i].valor_defecto != NULL) {
                    error_compilacion(c, e->linea, e->columna,
                        "valores por defecto en parametros aun no estan en bytecode v0.6.2");
                    return false;
                }
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

            Valor v_plantilla = valor_plantilla(fn);
            int fn_idx = chunk_agregar_constante(c->actual->chunk, v_plantilla);
            if (fn_idx < 0 || fn_idx > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "demasiadas constantes para v0.6 (operando byte)");
                return false;
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
                    if (!compilador_compilar_expr(c, p->expr)) return false;
                    chunk_emitir_byte(c->actual->chunk, OP_FORMATO_F, e->linea);
                    /* v1.2: validar que el resultado de OP_FORMATO_F
                     * (posiblemente venido de `__cadena__`) sea cadena. */
                    chunk_emitir_byte(c->actual->chunk, OP_ASEGURAR_CADENA, e->linea);
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

static bool compilar_asignar(Compilador *c, const Sent *s) {
    Expr *destino = s->como.asignar.destino;

    /* Asignación a índice: `obj[key] = valor`. */
    if (destino->tipo == EXPR_INDICE) {
        if (!compilador_compilar_expr(c, destino->como.indice.objeto)) return false;
        if (!compilador_compilar_expr(c, destino->como.indice.indice)) return false;
        if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;
        chunk_emitir_byte(c->actual->chunk, OP_ASIGNAR_INDICE, s->linea);
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
     */
    if (c->actual->es_funcion) {
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

    if (destino->tipo != EXPR_IDENT) {
        error_compilacion(c, s->linea, s->columna,
            "ErrorDeSintaxis: destino de asignacion aumentada no soportado en bytecode v0.6");
        return false;
    }

    /* Decidir si es local, upvalue o global. */
    int slot_local = c->actual->es_funcion
        ? buscar_local(c->actual, destino->como.ident.nombre,
                          destino->como.ident.longitud)
        : -1;
    int slot_upv = -1;
    if (slot_local < 0 && c->actual->es_funcion) {
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
 * SENT_SI: cadena de ramas (si / sino si* / sino?).
 *
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
            if (destino == NULL || destino->tipo != EXPR_IDENT) return true;
            const char *nombre = destino->como.ident.nombre;
            int len = destino->como.ident.longitud;
            if (!c->actual->es_funcion) return true;  /* en top-level usa globales */
            int existente = buscar_local(c->actual, nombre, len);
            if (existente >= 0) return true;  /* ya reservado */
            /* Reservar slot. */
            chunk_emitir_byte(c->actual->chunk, OP_NULO, linea_default);
            int slot = agregar_local(c, nombre, len, linea_default);
            if (slot < 0) return false;
            return true;
        }
        default:
            /* SENT_MIENTRAS/SENT_PARA/SENT_INTENTAR/SENT_FUNCION/
               SENT_CLASE/etc. no descienden — manejan sus propios
               locales cuando se compilen. */
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
    (void)n_locales_entrada;  /* reservado por si hay que restaurar */
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

        case SENT_PARA:
            return compilar_para(c, s);

        case SENT_INTENTAR:
            return compilar_intentar(c, s);

        case SENT_LANZAR: {
            if (s->como.lanzar.valor == NULL) {
                /* Re-raise (v0.8.3): solo válido dentro de un atrapar
                   con alias. Emitimos OBTENER_LOCAL del alias top + LANZAR. */
                if (c->n_atrapadores_activos == 0) {
                    error_compilacion(c, s->linea, s->columna,
                        "'lanzar' sin valor solo es valido dentro de 'atrapar Tipo como e:'");
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
        /* Sin soporte aún. */
        case SENT_GLOBAL:
            error_compilacion(c, s->linea, s->columna,
                "esta sentencia aun no esta implementada en bytecode v0.9");
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
       nulo como "valor del programa". */
    int linea_final = (n > 0) ? sents[n - 1]->linea : 1;
    chunk_emitir_byte(c->actual->chunk, OP_NULO, linea_final);
    chunk_emitir_byte(c->actual->chunk, OP_RETORNAR, linea_final);
    return true;
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

    for (int i = 0; i < n_params; i++) {
        if (params[i].valor_defecto != NULL) {
            error_compilacion(c, s->linea, s->columna,
                "valores por defecto en parametros aun no estan en bytecode v0.7");
            return false;
        }
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

    Valor v_plantilla = valor_plantilla(fn);
    int fn_idx = chunk_agregar_constante(c->actual->chunk, v_plantilla);
    if (fn_idx < 0 || fn_idx > 255) {
        error_compilacion(c, s->linea, s->columna,
            "demasiadas constantes para v0.7 (operando byte)");
        return false;
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
    if (c->actual->es_funcion) {
        /* Verificar si ya existe un local con ese nombre (redefinir). */
        int existente = buscar_local(c->actual, nombre, len_nombre);
        if (existente >= 0) {
            chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                (uint8_t)existente, s->linea);
        } else {
            int slot = agregar_local(c, nombre, len_nombre, s->linea);
            if (slot < 0) return false;
            /* OLD convention para nuevo local. */
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

    /* v0.11.5b fix: pre-reservar slots para nuevos locales del cuerpo
       del `para`. Sin esto, asignar a un nuevo local en el cuerpo del
       `para` haría crecer el stack en cada iteración (mismo bug que en
       `mientras`). */
    if (c->actual->es_funcion) {
        if (!pre_reservar_locales(c, s->como.para.cuerpo, s->linea))
            return false;
    }

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
        int alias_slot = -1;
        if (atr->alias.texto != NULL) {
            int slot = agregar_local(c, atr->alias.texto,
                                          atr->alias.longitud,
                                          atr->linea);
            if (slot < 0) return false;
            alias_slot = slot;
            /* OLD convention: la excepción está en el tope cuando se
               agregar_local. Cada atrapador resetea n_locales al
               handler-entry, así que el slot es consistente. */
            /* Push al stack de atrapadores activos para `lanzar` re-raise. */
            if (c->n_atrapadores_activos >= COMPILADOR_ATRAPADORES_MAX) {
                error_compilacion(c, atr->linea, atr->columna,
                    "anidamiento de atrapadores excede %d",
                    COMPILADOR_ATRAPADORES_MAX);
                return false;
            }
            c->atrapador_alias_slots[c->n_atrapadores_activos++] = alias_slot;
        } else {
            chunk_emitir_byte(c->actual->chunk, OP_DESCARTAR, atr->linea);
        }

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
