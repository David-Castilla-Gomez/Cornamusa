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
    s->n_bucles = 0;
    s->padre = padre;
    if (es_funcion) {
        s->locales[0].nombre = "";
        s->locales[0].longitud_nombre = 0;
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
    s->n_locales++;
    return slot;
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
            /* Prioridad: local del scope actual → global. Sin closures
               (decisión Fase 6 S5): no se busca en scopes padres. */
            int slot = buscar_local(c->actual, e->como.ident.nombre,
                                       e->como.ident.longitud);
            if (slot >= 0) {
                chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                                    (uint8_t)slot, e->linea);
                return true;
            }
            int idx = agregar_nombre_global(c, e->como.ident.nombre,
                                              e->como.ident.longitud);
            if (idx < 0 || idx > 255) {
                error_compilacion(c, e->linea, e->columna,
                    "demasiadas constantes para v0.6 (operando byte)");
                return false;
            }
            chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_GLOBAL, (uint8_t)idx, e->linea);
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
                }
                chunk_emitir_byte2(c->actual->chunk, OP_IMPRIMIR,
                                   (uint8_t)n_args, e->linea);
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

        /* Aplazadas a sesiones siguientes. */
        case EXPR_LITERAL_F_CADENA:
        case EXPR_ATRIBUTO:
        case EXPR_LAMBDA:
        case EXPR_REBANADA:
            error_compilacion(c, e->linea, e->columna,
                "esta forma de expresion no esta implementada en bytecode v0.6 sesion 6");
            return false;
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

    if (destino->tipo != EXPR_IDENT) {
        error_compilacion(c, s->linea, s->columna,
            "ErrorDeSintaxis: destino de asignacion no soportado en bytecode v0.6 sesion 6");
        return false;
    }

    if (!compilador_compilar_expr(c, s->como.asignar.valor)) return false;

    /*
     * Dentro de función: la primera asignación a un nombre nuevo lo
     * convierte en local; las siguientes reasignan ese local.
     * En el scope raíz (top-level): toda asignación va a globales.
     */
    if (c->actual->es_funcion) {
        int slot = buscar_local(c->actual, destino->como.ident.nombre,
                                   destino->como.ident.longitud);
        if (slot >= 0) {
            chunk_emitir_byte2(c->actual->chunk, OP_ASIGNAR_LOCAL,
                                (uint8_t)slot, s->linea);
        } else {
            /* Nuevo local: el valor ya está en el slot correcto del
               stack; solo registramos el nombre. */
            int nuevo = agregar_local(c, destino->como.ident.nombre,
                                          destino->como.ident.longitud,
                                          s->linea);
            if (nuevo < 0) return false;
        }
        return true;
    }

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
    if (destino->tipo != EXPR_IDENT) {
        error_compilacion(c, s->linea, s->columna,
            "ErrorDeSintaxis: destino de asignacion aumentada no soportado en bytecode v0.6");
        return false;
    }

    /* Decidir si es local o global. La aug requiere que la variable
       ya exista — para locales eso significa estar en el mapa; para
       globales, lo verifica la VM en tiempo de ejecución. */
    int slot_local = c->actual->es_funcion
        ? buscar_local(c->actual, destino->como.ident.nombre,
                          destino->como.ident.longitud)
        : -1;

    if (slot_local >= 0) {
        chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_LOCAL,
                            (uint8_t)slot_local, s->linea);
    } else {
        int idx_get = agregar_nombre_global(c, destino->como.ident.nombre,
                                              destino->como.ident.longitud);
        if (idx_get < 0 || idx_get > 255) {
            error_compilacion(c, s->linea, s->columna,
                "demasiadas constantes para v0.6 (operando byte)");
            return false;
        }
        chunk_emitir_byte2(c->actual->chunk, OP_OBTENER_GLOBAL,
                            (uint8_t)idx_get, s->linea);
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
static bool compilar_mientras(Compilador *c, const Sent *s) {
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

        /* Sin soporte aún. */
        case SENT_PARA:
        case SENT_CLASE:
        case SENT_INTENTAR:
        case SENT_LANZAR:
        case SENT_IMPORTAR:
        case SENT_DESDE_IMPORTAR:
        case SENT_GLOBAL:
        case SENT_NOLOCAL:
            error_compilacion(c, s->linea, s->columna,
                "esta sentencia aun no esta implementada en bytecode v0.6 sesion 5");
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
 * Compila una declaración de función:
 *   funcion nombre(p1, p2, ...): cuerpo fin funcion
 *
 * Crea una FuncionBC nueva con su propio chunk, abre un scope hijo
 * con los parámetros como locales en slots 1..aridad, compila el
 * cuerpo en ese scope, emite OP_NULO+OP_RETORNAR si el cuerpo no
 * terminaba ya con `retornar`, y deja la FuncionBC como constante
 * en el chunk padre seguida de OP_DEFINIR_GLOBAL <nombre>.
 *
 * Sin closures: el scope hijo no puede ver locales del padre. Las
 * funciones anidadas tienen acceso solo a sus propios params/locales
 * y a las globales.
 */
static bool compilar_funcion(Compilador *c, const Sent *s) {
    const char *nombre = s->como.funcion.nombre;
    int len_nombre = s->como.funcion.longitud_nombre;
    int n_params = s->como.funcion.n_parametros;
    Parametro *params = s->como.funcion.parametros;

    /* Aridad mínima (parámetros sin valor por defecto) y máxima
       (todos los parámetros). En v0.6 S5 NO soportamos defaults — el
       usuario debe pasar exactamente n_params argumentos. */
    for (int i = 0; i < n_params; i++) {
        if (params[i].valor_defecto != NULL) {
            error_compilacion(c, s->linea, s->columna,
                "valores por defecto en parametros aun no estan en bytecode v0.6 sesion 5");
            return false;
        }
    }

    FuncionBC *fn = funcion_bc_nueva(nombre, len_nombre, n_params);
    if (!fn) {
        error_compilacion(c, s->linea, s->columna, "memoria insuficiente");
        return false;
    }

    /* Abrir scope anidado. */
    ScopeCompilador scope_fn;
    scope_iniciar(&scope_fn, &fn->chunk, true, c->actual);
    /* Slot 0 = la propia función (callee), nombre por convención. */
    scope_fn.locales[0].nombre = nombre;
    scope_fn.locales[0].longitud_nombre = len_nombre;
    /* Registrar parámetros como locales en slots 1..n_params. */
    for (int i = 0; i < n_params; i++) {
        if (scope_fn.n_locales >= COMPILADOR_LOCALES_MAX) {
            error_compilacion(c, s->linea, s->columna,
                "demasiados parametros");
            funcion_bc_liberar(fn);
            return false;
        }
        scope_fn.locales[scope_fn.n_locales].nombre = params[i].nombre;
        scope_fn.locales[scope_fn.n_locales].longitud_nombre = params[i].longitud_nombre;
        scope_fn.n_locales++;
    }

    ScopeCompilador *prev = c->actual;
    c->actual = &scope_fn;
    bool ok = compilador_compilar_sent(c, s->como.funcion.cuerpo);
    /* Si el cuerpo terminó sin `retornar`, emitimos OP_NULO+OP_RETORNAR
       implícitos. Esto añade bytes "muertos" si el cuerpo SÍ terminó
       con `retornar` — pero no se ejecutan. Al estilo clox. */
    chunk_emitir_byte(&fn->chunk, OP_NULO, s->linea);
    chunk_emitir_byte(&fn->chunk, OP_RETORNAR, s->linea);
    c->actual = prev;

    if (!ok) {
        funcion_bc_liberar(fn);
        return false;
    }

    /* Emitir en el chunk padre: OP_CONST <fn> seguido de
       OP_DEFINIR_GLOBAL <nombre> (asignación al scope global). */
    Valor v_fn = valor_funcion_bc(fn);   /* toma posesión del refcount */
    chunk_emitir_constante(c->actual->chunk, v_fn, s->linea);

    int idx_nombre = agregar_nombre_global(c, nombre, len_nombre);
    if (idx_nombre < 0 || idx_nombre > 255) {
        error_compilacion(c, s->linea, s->columna,
            "demasiadas constantes para v0.6 (operando byte)");
        return false;
    }
    chunk_emitir_byte2(c->actual->chunk, OP_DEFINIR_GLOBAL,
                        (uint8_t)idx_nombre, s->linea);
    return true;
}
