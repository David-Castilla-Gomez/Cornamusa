#include "linter.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"

/* ──────────────────────────────────────────────────────────────────
 * Almacen de imports vs referencias.
 *
 * Para detectar imports no usados hacemos dos pasadas implicitas en
 * la misma travesia del AST: cuando vemos un `importar` registramos
 * el nombre (con su posicion); cuando vemos un `EXPR_IDENT` lo
 * marcamos como referenciado. Al final, los imports no marcados
 * generan warnings.
 *
 * Los nombres apuntan al buffer fuente — no se copian.
 * ────────────────────────────────────────────────────────────────── */

typedef struct {
    const char *texto;
    int longitud;
    int linea;
    int columna;
    bool usado;
} ImportEntry;

#define MAX_IMPORTS 256

/* ──────────────────────────────────────────────────────────────────
 * v1.50: scope stack para detectar locales/parametros no usados.
 *
 * Cada funcion (incluyendo lambdas y metodos) abre un scope. Las
 * locales se registran por primera asignacion como destino simple
 * (ident). `nolocal`/`global` marcan el nombre como `es_extern`:
 * sigue en el scope para no re-declararlo, pero no se warnea unused.
 *
 * Una referencia (EXPR_IDENT) marca el primer match subiendo por la
 * cadena de padres (semantica de closures).
 * ────────────────────────────────────────────────────────────────── */

typedef enum {
    DECL_VAR,
    DECL_PARAM,
    DECL_LOOP_VAR,    /* v1.55: target de `para X en ...:` */
} TipoDecl;

typedef struct {
    const char *texto;
    int longitud;
    TipoDecl tipo;
    int linea;
    int columna;
    bool usado;
    bool es_extern;   /* declarado nolocal/global: skip warnings */
} DeclLocal;

#define MAX_DECLS_FUNCION 512

typedef struct ScopeFunc {
    DeclLocal decls[MAX_DECLS_FUNCION];
    int n;
    struct ScopeFunc *padre;
} ScopeFunc;

typedef struct {
    Warning *avisos;
    int n;
    int capacidad;

    ImportEntry imports[MAX_IMPORTS];
    int n_imports;

    ScopeFunc *scope_actual;   /* NULL en modulo (top-level) */
} Ctx;

/* ──────────────────────────────────────────────────────────────────
 * Helpers de Warning.
 * ────────────────────────────────────────────────────────────────── */

static void emitir(Ctx *ctx, TipoWarning tipo, int linea, int columna,
                    const char *fmt, ...) {
    if (ctx->n >= ctx->capacidad) {
        int nuevo = ctx->capacidad ? ctx->capacidad * 2 : 16;
        Warning *nv = (Warning *)realloc(ctx->avisos, sizeof(Warning) * (size_t)nuevo);
        if (!nv) return;
        ctx->avisos = nv;
        ctx->capacidad = nuevo;
    }
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Warning *w = &ctx->avisos[ctx->n++];
    w->tipo = tipo;
    w->linea = linea;
    w->columna = columna;
    w->mensaje = strdup(buf);
}

/* ──────────────────────────────────────────────────────────────────
 * Imports: registrar y marcar.
 * ────────────────────────────────────────────────────────────────── */

static void registrar_import(Ctx *ctx, const char *texto, int longitud,
                              int linea, int columna) {
    if (ctx->n_imports >= MAX_IMPORTS) return;
    ImportEntry *e = &ctx->imports[ctx->n_imports++];
    e->texto = texto;
    e->longitud = longitud;
    e->linea = linea;
    e->columna = columna;
    e->usado = false;
}

static void marcar_ident_usado(Ctx *ctx, const char *texto, int longitud) {
    for (int i = 0; i < ctx->n_imports; i++) {
        ImportEntry *e = &ctx->imports[i];
        if (e->longitud == longitud
            && memcmp(e->texto, texto, (size_t)longitud) == 0) {
            e->usado = true;
        }
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Scope: declarar y resolver referencias.
 * ────────────────────────────────────────────────────────────────── */

static int scope_buscar(ScopeFunc *s, const char *texto, int longitud) {
    for (int i = 0; i < s->n; i++) {
        if (s->decls[i].longitud == longitud
            && memcmp(s->decls[i].texto, texto, (size_t)longitud) == 0) {
            return i;
        }
    }
    return -1;
}

/* Devuelve true si se agrego una nueva entrada; false si ya existia.
 * El llamador usa el retorno para gatillar shadow check solo en nuevos
 * locales. */
static bool scope_declarar(ScopeFunc *s, const char *texto, int longitud,
                            TipoDecl tipo, int linea, int columna,
                            bool es_extern) {
    if (!s) return false;
    int existe = scope_buscar(s, texto, longitud);
    if (existe >= 0) {
        /* Si ya estaba como variable y ahora viene nolocal/global,
         * actualizar la marca para no warnear. */
        if (es_extern) s->decls[existe].es_extern = true;
        return false;
    }
    if (s->n >= MAX_DECLS_FUNCION) return false;
    DeclLocal *d = &s->decls[s->n++];
    d->texto = texto;
    d->longitud = longitud;
    d->tipo = tipo;
    d->linea = linea;
    d->columna = columna;
    d->usado = false;
    d->es_extern = es_extern;
    return true;
}

/* v1.55: chequea si el nombre ya esta declarado (como NO es_extern) en
 * algun scope ancestro. Si si, emite warning de shadow. */
static void verificar_shadow(Ctx *ctx, ScopeFunc *desde_padre,
                              const char *texto, int longitud,
                              int linea, int columna) {
    if (!desde_padre) return;
    /* Saltarse nombres convencionales (`_`, `yo`). */
    if (longitud == 0) return;
    if (texto[0] == '_') return;
    if (longitud == 2 && memcmp(texto, "yo", 2) == 0) return;

    for (ScopeFunc *s = desde_padre; s; s = s->padre) {
        int idx = scope_buscar(s, texto, longitud);
        if (idx >= 0 && !s->decls[idx].es_extern) {
            emitir(ctx, LINT_SHADOW, linea, columna,
                    "'%.*s' sombrea variable del scope exterior",
                    longitud, texto);
            return;
        }
    }
}

static void marcar_uso_scopes(Ctx *ctx, const char *texto, int longitud) {
    for (ScopeFunc *s = ctx->scope_actual; s; s = s->padre) {
        int idx = scope_buscar(s, texto, longitud);
        if (idx >= 0) {
            /* Entradas `es_extern` (nolocal/global) son placeholders — el
             * nombre real vive en el scope padre. Saltamos y seguimos. */
            if (s->decls[idx].es_extern) continue;
            s->decls[idx].usado = true;
            return;
        }
    }
}

/* Recorre la expresion destino de un SENT_ASIGNAR registrando como
 * locales todos los identificadores simples (incluye destructuring
 * `a, b = par` y nested `(a, (b, c)) = ...`). Atributos e indices no
 * son nuevos locales. v1.55: si es nuevo declarador y sombrea outer,
 * emite warning. */
static void declarar_destino_asignacion(Ctx *ctx, Expr *destino) {
    if (!ctx->scope_actual || !destino) return;
    if (destino->tipo == EXPR_IDENT) {
        bool nuevo = scope_declarar(ctx->scope_actual,
                                      destino->como.ident.nombre,
                                      destino->como.ident.longitud,
                                      DECL_VAR, destino->linea, destino->columna,
                                      false);
        if (nuevo) {
            verificar_shadow(ctx, ctx->scope_actual->padre,
                              destino->como.ident.nombre,
                              destino->como.ident.longitud,
                              destino->linea, destino->columna);
        }
    } else if (destino->tipo == EXPR_TUPLA || destino->tipo == EXPR_LISTA) {
        for (int i = 0; i < destino->como.secuencia.n_elementos; i++) {
            declarar_destino_asignacion(ctx, destino->como.secuencia.elementos[i]);
        }
    }
}

/* True si el nombre debe skip-earse al emitir unused-local/unused-param.
 * Convenciones: `yo` (self implicit), nombres que empiezan con `_`. */
static bool nombre_se_omite(const char *texto, int longitud) {
    if (longitud == 0) return true;
    if (texto[0] == '_') return true;
    if (longitud == 2 && memcmp(texto, "yo", 2) == 0) return true;
    return false;
}

/* v1.55: registra el objetivo de un `para X en ...:` como DECL_LOOP_VAR.
 * Soporta destructuring `para a, b en items`. */
static void declarar_objetivo_para(Ctx *ctx, Expr *destino) {
    if (!ctx->scope_actual || !destino) return;
    if (destino->tipo == EXPR_IDENT) {
        bool nv = scope_declarar(ctx->scope_actual,
                                   destino->como.ident.nombre,
                                   destino->como.ident.longitud,
                                   DECL_LOOP_VAR,
                                   destino->linea, destino->columna, false);
        if (nv) {
            verificar_shadow(ctx, ctx->scope_actual->padre,
                              destino->como.ident.nombre,
                              destino->como.ident.longitud,
                              destino->linea, destino->columna);
        }
    } else if (destino->tipo == EXPR_TUPLA || destino->tipo == EXPR_LISTA) {
        for (int i = 0; i < destino->como.secuencia.n_elementos; i++) {
            declarar_objetivo_para(ctx, destino->como.secuencia.elementos[i]);
        }
    }
}

/* v1.55: chequea si un valor default es un literal mutable (lista,
 * dict, conjunto). Estos se evaluan una sola vez al definir la
 * funcion y se comparten entre llamadas — bug clasico. */
static void verificar_mutable_default(Ctx *ctx, Parametro *p) {
    Expr *d = p->valor_defecto;
    if (!d) return;
    const char *tipo_str = NULL;
    if (d->tipo == EXPR_LISTA)       tipo_str = "lista";
    else if (d->tipo == EXPR_DICCIONARIO) tipo_str = "diccionario";
    else if (d->tipo == EXPR_CONJUNTO)    tipo_str = "conjunto";
    if (!tipo_str) return;
    emitir(ctx, LINT_MUTABLE_DEFAULT, p->linea, p->columna,
            "parametro '%.*s' tiene default mutable (%s literal); se comparte entre llamadas",
            p->longitud_nombre, p->nombre, tipo_str);
}

static void emitir_warnings_scope(Ctx *ctx, ScopeFunc *s);

/* Empuja un nuevo scope sobre `*nuevo`, declara parametros segun
 * `params[]`, y deja `ctx->scope_actual` apuntando a `*nuevo`. El
 * llamador debe haber visitado los valores por defecto en el scope
 * exterior antes de llamar a esto (semantica Python: defaults se
 * evaluan al definir, no al llamar). */
static void empujar_scope_funcion(Ctx *ctx, ScopeFunc *nuevo,
                                    Parametro *params, int n_params) {
    memset(nuevo, 0, sizeof(*nuevo));
    nuevo->padre = ctx->scope_actual;
    ctx->scope_actual = nuevo;

    for (int i = 0; i < n_params; i++) {
        Parametro *p = &params[i];
        /* *args / **kwargs no se warnean por convencion. */
        if (p->es_estrella || p->es_doble_estrella) continue;
        bool nv = scope_declarar(nuevo, p->nombre, p->longitud_nombre,
                                   DECL_PARAM, p->linea, p->columna, false);
        /* v1.55: shadow check para parametros vs scope exterior. */
        if (nv) {
            verificar_shadow(ctx, nuevo->padre,
                              p->nombre, p->longitud_nombre,
                              p->linea, p->columna);
        }
    }
}

static void salir_scope_funcion(Ctx *ctx, ScopeFunc *nuevo) {
    emitir_warnings_scope(ctx, nuevo);
    ctx->scope_actual = nuevo->padre;
}

static void emitir_warnings_scope(Ctx *ctx, ScopeFunc *s) {
    for (int i = 0; i < s->n; i++) {
        DeclLocal *d = &s->decls[i];
        if (d->usado || d->es_extern) continue;
        if (nombre_se_omite(d->texto, d->longitud)) continue;

        TipoWarning t;
        const char *categoria;
        if (d->tipo == DECL_PARAM) {
            t = LINT_UNUSED_PARAM; categoria = "parametro";
        } else if (d->tipo == DECL_LOOP_VAR) {
            t = LINT_UNUSED_LOOP_VAR; categoria = "variable de bucle";
        } else {
            t = LINT_UNUSED_LOCAL; categoria = "variable local";
        }
        emitir(ctx, t, d->linea, d->columna,
                "%s '%.*s' no se usa", categoria, d->longitud, d->texto);
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Visitor del AST.
 * ────────────────────────────────────────────────────────────────── */

static void visitar_expr(Expr *e, Ctx *ctx);
static void visitar_sent(Sent *s, Ctx *ctx);

static void visitar_expr_quizas(Expr *e, Ctx *ctx) {
    if (e) visitar_expr(e, ctx);
}

static void visitar_sent_quizas(Sent *s, Ctx *ctx) {
    if (s) visitar_sent(s, ctx);
}

/* True si `op` es == o != (comparacion de igualdad). */
static bool es_eq_o_neq(TipoToken op) {
    return op == TT_IGUAL || op == TT_DISTINTO;
}

static void visitar_patron(Patron *p, Ctx *ctx) {
    if (!p) return;
    switch (p->tipo) {
        case PATRON_LITERAL:
            visitar_expr_quizas(p->como.literal, ctx);
            break;
        case PATRON_TUPLA:
        case PATRON_LISTA:
        case PATRON_OR:
            for (int i = 0; i < p->como.estructural.n; i++) {
                visitar_patron(p->como.estructural.elementos[i], ctx);
            }
            break;
        case PATRON_WILDCARD:
        case PATRON_BIND:
        case PATRON_STAR_BIND:
        case PATRON_TIPO:
        default:
            break;
    }
}

static void visitar_expr(Expr *e, Ctx *ctx) {
    if (!e) return;
    switch (e->tipo) {
        case EXPR_LITERAL_ENTERO:
        case EXPR_LITERAL_DECIMAL:
        case EXPR_LITERAL_CADENA:
        case EXPR_LITERAL_BOOLEANO:
        case EXPR_LITERAL_NULO:
            break;

        case EXPR_IDENT:
            marcar_ident_usado(ctx, e->como.ident.nombre, e->como.ident.longitud);
            marcar_uso_scopes(ctx, e->como.ident.nombre, e->como.ident.longitud);
            break;

        case EXPR_BINARIO: {
            /* Check 3: `x == nulo` / `x != nulo` */
            if (es_eq_o_neq(e->como.binario.op)) {
                Expr *iz = e->como.binario.izq;
                Expr *de = e->como.binario.der;
                bool iz_nulo = iz && iz->tipo == EXPR_LITERAL_NULO;
                bool de_nulo = de && de->tipo == EXPR_LITERAL_NULO;
                if (iz_nulo || de_nulo) {
                    const char *op_txt = (e->como.binario.op == TT_IGUAL) ? "==" : "!=";
                    const char *sug = (e->como.binario.op == TT_IGUAL) ? "es nulo" : "no es nulo";
                    emitir(ctx, LINT_EQ_NULO, e->linea, e->columna,
                            "comparacion con nulo via '%s' — prefiere '%s'",
                            op_txt, sug);
                }
            }
            visitar_expr_quizas(e->como.binario.izq, ctx);
            visitar_expr_quizas(e->como.binario.der, ctx);
            break;
        }

        case EXPR_UNARIO:
            visitar_expr_quizas(e->como.unario.operando, ctx);
            break;

        case EXPR_LOGICA:
            visitar_expr_quizas(e->como.logica.izq, ctx);
            visitar_expr_quizas(e->como.logica.der, ctx);
            break;

        case EXPR_LLAMADA:
            visitar_expr_quizas(e->como.llamada.callee, ctx);
            for (int i = 0; i < e->como.llamada.n_args; i++) {
                visitar_expr_quizas(e->como.llamada.args[i], ctx);
            }
            break;

        case EXPR_ATRIBUTO:
            visitar_expr_quizas(e->como.atributo.objeto, ctx);
            break;

        case EXPR_GRUPO:
            visitar_expr_quizas(e->como.grupo.interna, ctx);
            break;

        case EXPR_LAMBDA: {
            /* Defaults se evaluan en el scope exterior (no en el lambda).
             * v1.55: chequear defaults mutables. */
            for (int i = 0; i < e->como.lambda.n_parametros; i++) {
                Parametro *p = &e->como.lambda.parametros[i];
                verificar_mutable_default(ctx, p);
                visitar_expr_quizas(p->valor_defecto, ctx);
            }
            ScopeFunc nuevo;
            empujar_scope_funcion(ctx, &nuevo,
                                    e->como.lambda.parametros,
                                    e->como.lambda.n_parametros);
            visitar_expr_quizas(e->como.lambda.cuerpo, ctx);
            salir_scope_funcion(ctx, &nuevo);
            break;
        }

        case EXPR_LISTA:
        case EXPR_CONJUNTO:
        case EXPR_TUPLA:
            for (int i = 0; i < e->como.secuencia.n_elementos; i++) {
                visitar_expr_quizas(e->como.secuencia.elementos[i], ctx);
            }
            break;

        case EXPR_DICCIONARIO:
            for (int i = 0; i < e->como.diccionario.n_pares; i++) {
                visitar_expr_quizas(e->como.diccionario.claves[i], ctx);
                visitar_expr_quizas(e->como.diccionario.valores[i], ctx);
            }
            break;

        case EXPR_COMPREHENSION:
            visitar_expr_quizas(e->como.comprehension.iterable, ctx);
            visitar_expr_quizas(e->como.comprehension.guarda, ctx);
            visitar_expr_quizas(e->como.comprehension.expr_elem, ctx);
            visitar_expr_quizas(e->como.comprehension.expr_valor, ctx);
            break;

        case EXPR_INDICE:
            visitar_expr_quizas(e->como.indice.objeto, ctx);
            visitar_expr_quizas(e->como.indice.indice, ctx);
            break;

        case EXPR_REBANADA:
            visitar_expr_quizas(e->como.rebanada.objeto, ctx);
            visitar_expr_quizas(e->como.rebanada.inicio, ctx);
            visitar_expr_quizas(e->como.rebanada.fin, ctx);
            visitar_expr_quizas(e->como.rebanada.paso, ctx);
            break;

        case EXPR_SUPER:
            break;

        case EXPR_TERNARIA:
            visitar_expr_quizas(e->como.ternaria.cond, ctx);
            visitar_expr_quizas(e->como.ternaria.si_si, ctx);
            visitar_expr_quizas(e->como.ternaria.si_no, ctx);
            break;

        case EXPR_LITERAL_F_CADENA:
            for (int i = 0; i < e->como.f_cadena.n_partes; i++) {
                visitar_expr_quizas(e->como.f_cadena.partes[i].expr, ctx);
            }
            break;

        default:
            break;
    }
}

/* True si la sentencia es un "terminator" estructural: no transfiere
 * control al siguiente statement del bloque. */
static bool sentencia_termina_flujo(Sent *s) {
    if (!s) return false;
    switch (s->tipo) {
        case SENT_RETORNAR:
        case SENT_ROMPER:
        case SENT_CONTINUAR:
        case SENT_LANZAR:
            return true;
        default:
            return false;
    }
}

static void visitar_bloque(Sent *bloque, Ctx *ctx) {
    if (!bloque || bloque->tipo != SENT_BLOQUE) {
        visitar_sent_quizas(bloque, ctx);
        return;
    }
    Sent **ss = bloque->como.bloque.sentencias;
    int n = bloque->como.bloque.n_sentencias;

    /* Check 1: codigo inalcanzable. Solo la primera sentencia tras el
     * terminator se reporta; las posteriores se asume que vienen del
     * mismo bug. */
    for (int i = 0; i + 1 < n; i++) {
        if (sentencia_termina_flujo(ss[i])) {
            Sent *siguiente = ss[i + 1];
            const char *cual =
                ss[i]->tipo == SENT_RETORNAR ? "retornar" :
                ss[i]->tipo == SENT_ROMPER ? "romper" :
                ss[i]->tipo == SENT_CONTINUAR ? "continuar" : "lanzar";
            emitir(ctx, LINT_UNREACHABLE, siguiente->linea, siguiente->columna,
                    "codigo inalcanzable tras '%s'", cual);
            break;
        }
    }

    /* Check 2: `pasar` redundante en bloque con otras sentencias. */
    if (n > 1) {
        for (int i = 0; i < n; i++) {
            if (ss[i]->tipo == SENT_PASAR) {
                emitir(ctx, LINT_REDUNDANT_PASAR, ss[i]->linea, ss[i]->columna,
                        "'pasar' redundante en bloque no vacio");
            }
        }
    }

    for (int i = 0; i < n; i++) {
        visitar_sent_quizas(ss[i], ctx);
    }
}

static void visitar_sent(Sent *s, Ctx *ctx) {
    if (!s) return;
    switch (s->tipo) {
        case SENT_EXPR:
            visitar_expr_quizas(s->como.expr.expr, ctx);
            break;

        case SENT_ASIGNAR:
            /* No visitamos `destino` si es solo un EXPR_IDENT — eso es la
             * declaracion, no una lectura. Si es atributo o indice si que
             * lee el objeto. */
            if (s->como.asignar.destino) {
                Expr *d = s->como.asignar.destino;
                if (d->tipo == EXPR_ATRIBUTO) {
                    visitar_expr_quizas(d->como.atributo.objeto, ctx);
                } else if (d->tipo == EXPR_INDICE) {
                    visitar_expr_quizas(d->como.indice.objeto, ctx);
                    visitar_expr_quizas(d->como.indice.indice, ctx);
                } else if (d->tipo == EXPR_REBANADA) {
                    visitar_expr_quizas(d->como.rebanada.objeto, ctx);
                    visitar_expr_quizas(d->como.rebanada.inicio, ctx);
                    visitar_expr_quizas(d->como.rebanada.fin, ctx);
                    visitar_expr_quizas(d->como.rebanada.paso, ctx);
                } else if (d->tipo == EXPR_IDENT
                            || d->tipo == EXPR_TUPLA
                            || d->tipo == EXPR_LISTA) {
                    /* Destinos simples / destructuring: registrar como
                     * locales en el scope actual (si lo hay). */
                    declarar_destino_asignacion(ctx, d);
                } else {
                    /* Cualquier otro destino — tratamos como expresion. */
                    visitar_expr_quizas(d, ctx);
                }
            }
            visitar_expr_quizas(s->como.asignar.valor, ctx);
            break;

        case SENT_ASIGNAR_AUG:
            /* `x += y` lee y escribe x — el destino SI cuenta como lectura. */
            visitar_expr_quizas(s->como.asignar_aug.destino, ctx);
            visitar_expr_quizas(s->como.asignar_aug.valor, ctx);
            break;

        case SENT_PASAR:
        case SENT_ROMPER:
        case SENT_CONTINUAR:
            break;

        case SENT_RETORNAR:
            visitar_expr_quizas(s->como.retornar.valor, ctx);
            break;

        case SENT_PRODUCIR:
            visitar_expr_quizas(s->como.producir.valor, ctx);
            break;

        case SENT_SI:
            for (int i = 0; i < s->como.si.n_ramas; i++) {
                visitar_expr_quizas(s->como.si.ramas[i].condicion, ctx);
                visitar_bloque(s->como.si.ramas[i].cuerpo, ctx);
            }
            break;

        case SENT_MIENTRAS:
            visitar_expr_quizas(s->como.mientras.condicion, ctx);
            visitar_bloque(s->como.mientras.cuerpo, ctx);
            visitar_bloque(s->como.mientras.sino, ctx);
            break;

        case SENT_PARA:
            /* `objetivo` es destino — registramos como DECL_LOOP_VAR para
             * detectar unused-loop-var. */
            visitar_expr_quizas(s->como.para.iterable, ctx);
            declarar_objetivo_para(ctx, s->como.para.objetivo);
            visitar_bloque(s->como.para.cuerpo, ctx);
            visitar_bloque(s->como.para.sino, ctx);
            break;

        case SENT_BLOQUE:
            visitar_bloque(s, ctx);
            break;

        case SENT_FUNCION: {
            /* Defaults se evaluan en el scope exterior. v1.55: chequear
             * defaults mutables. */
            for (int i = 0; i < s->como.funcion.n_parametros; i++) {
                Parametro *p = &s->como.funcion.parametros[i];
                verificar_mutable_default(ctx, p);
                visitar_expr_quizas(p->valor_defecto, ctx);
            }
            ScopeFunc nuevo;
            empujar_scope_funcion(ctx, &nuevo,
                                    s->como.funcion.parametros,
                                    s->como.funcion.n_parametros);
            visitar_bloque(s->como.funcion.cuerpo, ctx);
            salir_scope_funcion(ctx, &nuevo);
            break;
        }

        case SENT_CLASE:
            for (int i = 0; i < s->como.clase.n_superclases; i++) {
                visitar_expr_quizas(s->como.clase.superclases[i], ctx);
            }
            visitar_bloque(s->como.clase.cuerpo, ctx);
            break;

        case SENT_INTENTAR:
            visitar_bloque(s->como.intentar.cuerpo, ctx);
            for (int i = 0; i < s->como.intentar.n_atrapadores; i++) {
                visitar_expr_quizas(s->como.intentar.atrapadores[i].tipo, ctx);
                visitar_bloque(s->como.intentar.atrapadores[i].cuerpo, ctx);
            }
            visitar_bloque(s->como.intentar.sino, ctx);
            visitar_bloque(s->como.intentar.finalmente, ctx);
            break;

        case SENT_LANZAR:
            visitar_expr_quizas(s->como.lanzar.valor, ctx);
            break;

        case SENT_IMPORTAR: {
            /* `importar a.b.c` introduce el nombre `a` en el scope (a menos
             * que se use `como X`, en cuyo caso es `X`). */
            const char *nombre;
            int longitud;
            if (s->como.importar.alias.texto) {
                nombre = s->como.importar.alias.texto;
                longitud = s->como.importar.alias.longitud;
            } else if (s->como.importar.n_segmentos > 0) {
                nombre = s->como.importar.segmentos[0].texto;
                longitud = s->como.importar.segmentos[0].longitud;
            } else {
                break;
            }
            registrar_import(ctx, nombre, longitud, s->linea, s->columna);
            break;
        }

        case SENT_DESDE_IMPORTAR:
            /* `desde X importar Y como Z, W, ...` introduce los aliases Z, W. */
            for (int i = 0; i < s->como.desde_importar.n_items; i++) {
                ItemImportado *it = &s->como.desde_importar.items[i];
                const char *nombre;
                int longitud;
                if (it->alias.texto) {
                    nombre = it->alias.texto;
                    longitud = it->alias.longitud;
                } else {
                    nombre = it->nombre.texto;
                    longitud = it->nombre.longitud;
                }
                registrar_import(ctx, nombre, longitud, it->linea, it->columna);
            }
            /* `desde X importar *`: no podemos saber que se importo — no
             * generamos warning de unused-import. */
            break;

        case SENT_GLOBAL:
        case SENT_NOLOCAL:
            /* En el scope actual, marcar estos nombres como externos para
             * que asignaciones posteriores no se traten como nuevos
             * locales no usados. */
            if (ctx->scope_actual) {
                for (int i = 0; i < s->como.global_o_nolocal.n_nombres; i++) {
                    Nombre *n = &s->como.global_o_nolocal.nombres[i];
                    scope_declarar(ctx->scope_actual,
                                    n->texto, n->longitud,
                                    DECL_VAR, s->linea, s->columna,
                                    /*es_extern=*/true);
                }
            }
            break;

        case SENT_COINCIDIR:
            visitar_expr_quizas(s->como.coincidir.sujeto, ctx);
            for (int i = 0; i < s->como.coincidir.n_clausulas; i++) {
                ClausulaCuando *c = &s->como.coincidir.clausulas[i];
                visitar_patron(c->patron, ctx);
                visitar_expr_quizas(c->guarda, ctx);
                visitar_bloque(c->cuerpo, ctx);
            }
            break;

        default:
            break;
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Sort y API publica.
 * ────────────────────────────────────────────────────────────────── */

static int comparar_warning(const void *a, const void *b) {
    const Warning *wa = (const Warning *)a;
    const Warning *wb = (const Warning *)b;
    if (wa->linea != wb->linea) return wa->linea - wb->linea;
    return wa->columna - wb->columna;
}

LinterResultado linter_analizar(Sent **sents, int n) {
    Ctx ctx;
    memset(&ctx, 0, sizeof(ctx));

    for (int i = 0; i < n; i++) {
        visitar_sent_quizas(sents[i], &ctx);
    }

    /* Emitir warnings de imports no usados. */
    for (int i = 0; i < ctx.n_imports; i++) {
        ImportEntry *e = &ctx.imports[i];
        if (!e->usado) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                "modulo importado pero no usado: '%.*s'",
                e->longitud, e->texto);
            emitir(&ctx, LINT_UNUSED_IMPORT, e->linea, e->columna, "%s", buf);
        }
    }

    /* Orden estable por linea/columna para output determinista. */
    if (ctx.n > 1) {
        qsort(ctx.avisos, (size_t)ctx.n, sizeof(Warning), comparar_warning);
    }

    LinterResultado r;
    r.avisos = ctx.avisos;
    r.n = ctx.n;
    r.capacidad = ctx.capacidad;
    return r;
}

void linter_resultado_destruir(LinterResultado *r) {
    if (!r) return;
    for (int i = 0; i < r->n; i++) {
        free(r->avisos[i].mensaje);
    }
    free(r->avisos);
    r->avisos = NULL;
    r->n = 0;
    r->capacidad = 0;
}

const char *linter_tipo_nombre(TipoWarning t) {
    switch (t) {
        case LINT_UNREACHABLE:     return "unreachable";
        case LINT_REDUNDANT_PASAR: return "redundant-pasar";
        case LINT_EQ_NULO:         return "eq-nulo";
        case LINT_UNUSED_IMPORT:   return "unused-import";
        case LINT_UNUSED_LOCAL:    return "unused-local";
        case LINT_UNUSED_PARAM:    return "unused-param";
        case LINT_SHADOW:          return "shadow";
        case LINT_UNUSED_LOOP_VAR: return "unused-loop-var";
        case LINT_MUTABLE_DEFAULT: return "mutable-default";
        default:                   return "warning";
    }
}
