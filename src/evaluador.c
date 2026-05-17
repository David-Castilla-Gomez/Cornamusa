#include "evaluador.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tommath.h"

/* ──────────────────────────────────────────────────────────────────
 * Errores
 *
 * El evaluador no usa setjmp/longjmp. En su lugar cada función pone el
 * flag de error y devuelve `valor_nulo()`. Las funciones llamadoras
 * deben comprobar `ev->error.tuvo_error` tras evaluar sub-expresiones
 * para no continuar tras un fallo.
 * ────────────────────────────────────────────────────────────────── */

/*
 * Pone el error en `err` con linea/columna. Preserva el primer error
 * si ya hay uno activo. Devuelve nulo para uso encadenado.
 */
static Valor error_pos(EvalError *err, int linea, int columna,
                        const char *fmt, ...) {
    if (!err->tuvo_error) {
        err->tuvo_error = true;
        err->linea = linea;
        err->columna = columna;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(err->mensaje, sizeof(err->mensaje), fmt, ap);
        va_end(ap);
    }
    return valor_nulo();
}

/* Wrapper compatible con los call-sites antiguos (Evaluador + Expr). */
static Valor error_en(Evaluador *ev, const Expr *e, const char *fmt, ...) {
    if (!ev->error.tuvo_error) {
        ev->error.tuvo_error = true;
        ev->error.linea = e->linea;
        ev->error.columna = e->columna;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(ev->error.mensaje, sizeof(ev->error.mensaje), fmt, ap);
        va_end(ap);
    }
    return valor_nulo();
}

/* ──────────────────────────────────────────────────────────────────
 * API pública
 * ────────────────────────────────────────────────────────────────── */

void evaluador_iniciar(Evaluador *ev, Entorno *globales) {
    ev->globales = globales;
    ev->entorno_actual = globales;
    ev->error.tuvo_error = false;
    ev->error.mensaje[0] = '\0';
    ev->error.linea = 0;
    ev->error.columna = 0;
    ev->control = EJEC_NORMAL;
    ev->valor_retorno = valor_nulo();
}

void evaluador_limpiar_error(Evaluador *ev) {
    ev->error.tuvo_error = false;
    ev->error.mensaje[0] = '\0';
    ev->control = EJEC_NORMAL;
    valor_destruir(&ev->valor_retorno);
}

bool evaluador_tiene_error(const Evaluador *ev) {
    return ev->error.tuvo_error;
}

/* ──────────────────────────────────────────────────────────────────
 * Helpers numéricos
 * ────────────────────────────────────────────────────────────────── */

/* Convierte un VAL_ENTERO/_SMALL/BOOLEANO/DECIMAL a double. */
static double valor_a_doble(const Valor *v) {
    if (v->tipo == VAL_ENTERO_SMALL) return (double)v->como.entero_small;
    if (v->tipo == VAL_ENTERO) return mp_get_double(v->como.entero);
    if (v->tipo == VAL_BOOLEANO) return v->como.booleano ? 1.0 : 0.0;
    if (v->tipo == VAL_DECIMAL) return v->como.decimal;
    return 0.0;
}

/*
 * `True` y `False` son enteros 1 y 0 en Python para fines aritméticos.
 * En Cornamusa adoptamos la misma semántica: si una operación requiere
 * entero, un booleano se promueve a 1/0. Esta función crea un mp_int
 * temporal cuando es necesario; el llamador debe destruir con mp_clear+free.
 *
 * Devuelve NULL si OOM. Si el valor ya es entero, devuelve directamente
 * v->como.entero (sin transferir propiedad: NO destruir).
 *
 * La bandera `propio_out` indica si el llamador debe liberar.
 */
static mp_int *como_mp_int(const Valor *v, bool *propio_out) {
    *propio_out = false;
    if (v->tipo == VAL_ENTERO) {
        return v->como.entero;
    }
    if (v->tipo == VAL_ENTERO_SMALL) {
        mp_int *m = (mp_int *)malloc(sizeof(mp_int));
        if (!m) return NULL;
        if (mp_init(m) != MP_OKAY) { free(m); return NULL; }
        mp_set_i64(m, v->como.entero_small);
        *propio_out = true;
        return m;
    }
    if (v->tipo == VAL_BOOLEANO) {
        mp_int *m = (mp_int *)malloc(sizeof(mp_int));
        if (!m) return NULL;
        if (mp_init(m) != MP_OKAY) { free(m); return NULL; }
        mp_set_l(m, v->como.booleano ? 1 : 0);
        *propio_out = true;
        return m;
    }
    return NULL;
}

/* (eliminado en v0.11.1: valor_entero_de_mp — todos los call sites
   migraron a valor_entero_de_mp_normalizado en v0.11.0. Reportado
   en code review post-release como warning -Wunused-function.) */

/* Aloca un mp_int inicializado. NULL si OOM. */
static mp_int *nuevo_mp(void) {
    mp_int *m = (mp_int *)malloc(sizeof(mp_int));
    if (!m) return NULL;
    if (mp_init(m) != MP_OKAY) { free(m); return NULL; }
    return m;
}

static void liberar_mp(mp_int *m) {
    if (!m) return;
    mp_clear(m);
    free(m);
}

/* Wrappers públicos para los inline caches de F10 (vm.c). */
mp_int *evaluador_nuevo_mp(void) { return nuevo_mp(); }
void evaluador_liberar_mp(mp_int *m) { liberar_mp(m); }

/* ──────────────────────────────────────────────────────────────────
 * Aritmética entero ⊕ entero
 *
 * Devuelve un Valor nuevo. Si hay error (OOM, división por cero, etc.)
 * pone el error en el evaluador y devuelve nulo.
 * ────────────────────────────────────────────────────────────────── */

/*
 * v0.11 (B9): camino rápido SMALL+SMALL.
 *
 * Si ambos operandos son VAL_ENTERO_SMALL, ejecuta la operación con
 * int64_t en stack sin tocar libtommath. Si el resultado cabe en SMALL,
 * se devuelve inline. Si no (overflow detectado), `*aplicable = false`
 * y el llamador cae al path BIG.
 *
 * Operaciones cubiertas: +, -, *, //, %.  Para potencia/bitwise/despl
 * el llamador siempre va al path BIG (raras de aplicar a small int en
 * programas normales).
 *
 * Nota: usamos __builtin_*_overflow cuando está disponible (GCC/Clang).
 * Para MSVC fallback con check explícito vía rango.
 */
/* Forward decl porque también lo invoca el wrapper público abajo. */
static Valor small_op_small(EvalError *err, TipoToken op,
                              int64_t a, int64_t b,
                              int linea, int columna,
                              bool *aplicable);

Valor evaluador_small_op_small(EvalError *err, int op_token,
                                int64_t a, int64_t b,
                                int linea, int columna,
                                bool *aplicable) {
    return small_op_small(err, (TipoToken)op_token, a, b, linea, columna,
                           aplicable);
}

static Valor small_op_small(EvalError *err, TipoToken op,
                              int64_t a, int64_t b,
                              int linea, int columna,
                              bool *aplicable) {
    *aplicable = true;
    int64_t r;
    switch (op) {
#if defined(__GNUC__) || defined(__clang__)
        case TT_MAS:
            if (__builtin_add_overflow(a, b, &r)) goto overflow_a_big;
            return valor_entero_de_i64(r);
        case TT_MENOS:
            if (__builtin_sub_overflow(a, b, &r)) goto overflow_a_big;
            return valor_entero_de_i64(r);
        case TT_ASTERISCO:
            if (__builtin_mul_overflow(a, b, &r)) goto overflow_a_big;
            return valor_entero_de_i64(r);
#else
        case TT_MAS: {
            /* Margen 2^62: a+b cabe en int64_t sin UB, pero puede no
               caber en SMALL. valor_entero_de_i64 promueve si hace falta. */
            r = a + b;
            return valor_entero_de_i64(r);
        }
        case TT_MENOS: {
            r = a - b;
            return valor_entero_de_i64(r);
        }
        case TT_ASTERISCO: {
            /* Detección manual para MSVC: si ambos caben en 31 bits, la
               multiplicación cabe en 62 bits. Si no, pasar al path BIG. */
            if (a >= INT32_MIN && a <= INT32_MAX
                && b >= INT32_MIN && b <= INT32_MAX) {
                r = a * b;
                return valor_entero_de_i64(r);
            }
            goto overflow_a_big;
        }
#endif
        case TT_DOBLE_BARRA: {  /* división entera floor */
            if (b == 0) {
                return error_pos(err, linea, columna,
                    "ErrorAritmetico: division por cero");
            }
            /* Caso UB en C: SMALL_MIN / -1 desbordaria. Promote a BIG. */
            if (a == CORNAMUSA_SMALL_INT_MIN && b == -1) goto overflow_a_big;
            int64_t q = a / b;
            int64_t rem = a - q * b;
            /* Floor division: si signos difieren y hay resto, restar 1. */
            if (rem != 0 && ((a < 0) != (b < 0))) q -= 1;
            return valor_entero_de_i64(q);
        }
        case TT_PORCENTAJE: {  /* módulo (Python-style: signo del divisor) */
            if (b == 0) {
                return error_pos(err, linea, columna,
                    "ErrorAritmetico: modulo por cero");
            }
            if (a == CORNAMUSA_SMALL_INT_MIN && b == -1) {
                return valor_entero_de_i64(0);
            }
            int64_t rem = a % b;
            if (rem != 0 && ((rem < 0) != (b < 0))) rem += b;
            return valor_entero_de_i64(rem);
        }
        default:
            *aplicable = false;
            return valor_nulo();
    }
overflow_a_big:
    *aplicable = false;
    return valor_nulo();
}

static Valor entero_op_entero(EvalError *err, TipoToken op,
                              mp_int *a, mp_int *b,
                              int linea, int columna) {
    mp_int *r = nuevo_mp();
    if (!r) return error_pos(err, linea, columna, "memoria insuficiente");

    int rc = MP_OKAY;
    switch (op) {
        case TT_MAS:        rc = mp_add(a, b, r); break;
        case TT_MENOS:      rc = mp_sub(a, b, r); break;
        case TT_ASTERISCO:  rc = mp_mul(a, b, r); break;

        case TT_DOBLE_BARRA: {
            if (mp_iszero(b) == MP_YES) {
                liberar_mp(r);
                return error_pos(err, linea, columna,
                    "ErrorAritmetico: division por cero");
            }
            mp_int rem;
            if (mp_init(&rem) != MP_OKAY) {
                liberar_mp(r);
                return error_pos(err, linea, columna, "memoria insuficiente");
            }
            rc = mp_div(a, b, r, &rem);
            if (rc == MP_OKAY && mp_iszero(&rem) == MP_NO) {
                bool a_neg = (mp_isneg(a) == MP_YES);
                bool b_neg = (mp_isneg(b) == MP_YES);
                if (a_neg != b_neg) {
                    rc = mp_sub_d(r, 1, r);
                }
            }
            mp_clear(&rem);
            break;
        }

        case TT_PORCENTAJE: {
            if (mp_iszero(b) == MP_YES) {
                liberar_mp(r);
                return error_pos(err, linea, columna,
                    "ErrorAritmetico: modulo por cero");
            }
            rc = mp_mod(a, b, r);
            break;
        }

        case TT_DOBLE_ASTERISCO: {
            if (mp_isneg(b) == MP_YES) {
                liberar_mp(r);
                double ad = mp_get_double(a);
                double bd = mp_get_double(b);
                return valor_decimal(pow(ad, bd));
            }
            uint64_t exp = 0;
            if (mp_count_bits(b) > 32) {
                liberar_mp(r);
                return error_pos(err, linea, columna,
                    "ErrorDeValor: exponente demasiado grande para potencia entera");
            }
            exp = mp_get_u64(b);
            rc = mp_expt_n(a, (int)exp, r);
            break;
        }

        case TT_AMPERSAND:  rc = mp_and(a, b, r); break;
        case TT_BARRA_VERT: rc = mp_or(a, b, r); break;
        case TT_CIRCUNFLEJO: rc = mp_xor(a, b, r); break;

        case TT_DESPL_IZQ: {
            if (mp_isneg(b) == MP_YES) {
                liberar_mp(r);
                return error_pos(err, linea, columna,
                    "ErrorDeValor: desplazamiento negativo");
            }
            if (mp_count_bits(b) > 31) {
                liberar_mp(r);
                return error_pos(err, linea, columna,
                    "ErrorDeValor: desplazamiento demasiado grande");
            }
            int n = (int)mp_get_u64(b);
            rc = mp_mul_2d(a, n, r);
            break;
        }
        case TT_DESPL_DER: {
            if (mp_isneg(b) == MP_YES) {
                liberar_mp(r);
                return error_pos(err, linea, columna,
                    "ErrorDeValor: desplazamiento negativo");
            }
            if (mp_count_bits(b) > 31) {
                liberar_mp(r);
                return error_pos(err, linea, columna,
                    "ErrorDeValor: desplazamiento demasiado grande");
            }
            int n = (int)mp_get_u64(b);
            mp_int rem;
            if (mp_init(&rem) != MP_OKAY) {
                liberar_mp(r);
                return error_pos(err, linea, columna, "memoria insuficiente");
            }
            rc = mp_div_2d(a, n, r, &rem);
            if (rc == MP_OKAY && mp_isneg(a) == MP_YES
                && mp_iszero(&rem) == MP_NO) {
                rc = mp_sub_d(r, 1, r);
            }
            mp_clear(&rem);
            break;
        }

        default:
            liberar_mp(r);
            return error_pos(err, linea, columna,
                "operador no soportado entre enteros");
    }

    if (rc != MP_OKAY) {
        liberar_mp(r);
        return error_pos(err, linea, columna, "fallo en operacion entera");
    }
    /* v0.11 (B9): si el resultado cabe en SMALL, demote inline. */
    return valor_entero_de_mp_normalizado(r);
}

/* ──────────────────────────────────────────────────────────────────
 * Aritmética decimal ⊕ decimal (con promociones desde entero/booleano)
 * ────────────────────────────────────────────────────────────────── */

static Valor decimal_op_decimal(EvalError *err, TipoToken op,
                                double a, double b,
                                int linea, int columna) {
    switch (op) {
        case TT_MAS:       return valor_decimal(a + b);
        case TT_MENOS:     return valor_decimal(a - b);
        case TT_ASTERISCO: return valor_decimal(a * b);
        case TT_BARRA:
            if (b == 0.0) {
                return error_pos(err, linea, columna,
                    "ErrorAritmetico: division por cero");
            }
            return valor_decimal(a / b);
        case TT_DOBLE_BARRA:
            if (b == 0.0) {
                return error_pos(err, linea, columna,
                    "ErrorAritmetico: division por cero");
            }
            return valor_decimal(floor(a / b));
        case TT_PORCENTAJE: {
            if (b == 0.0) {
                return error_pos(err, linea, columna,
                    "ErrorAritmetico: modulo por cero");
            }
            double r = a - floor(a / b) * b;
            return valor_decimal(r);
        }
        case TT_DOBLE_ASTERISCO:
            return valor_decimal(pow(a, b));
        default:
            return error_pos(err, linea, columna,
                "operador no soportado entre decimales");
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Aritmética y operadores con cadenas
 * ────────────────────────────────────────────────────────────────── */

static Valor cadena_concatenar(EvalError *err, const Valor *a, const Valor *b,
                                int linea, int columna) {
    int la = a->como.cadena.longitud;
    int lb = b->como.cadena.longitud;
    char *buf = (char *)malloc((size_t)la + (size_t)lb + 1);
    if (!buf) return error_pos(err, linea, columna, "memoria insuficiente");
    if (la > 0) memcpy(buf, a->como.cadena.texto, (size_t)la);
    if (lb > 0) memcpy(buf + la, b->como.cadena.texto, (size_t)lb);
    buf[la + lb] = '\0';
    Valor v;
    v.tipo = VAL_CADENA;
    v.dueno_cadena = true;
    v.como.cadena.texto = buf;
    v.como.cadena.longitud = la + lb;
    return v;
}

/* Repetición de cadena por entero: "ab" * 3 → "ababab".
   Solo se admite multiplicación con entero no negativo. */
static Valor cadena_repetir(EvalError *err, const Valor *cad, mp_int *veces,
                             int linea, int columna) {
    if (mp_isneg(veces) == MP_YES) {
        return valor_cadena_duplicar("", 0);  /* Python: "x" * -1 == "" */
    }
    if (mp_count_bits(veces) > 31) {
        return error_pos(err, linea, columna,
            "ErrorDeValor: repeticion de cadena demasiado grande");
    }
    uint64_t n = mp_get_u64(veces);
    size_t la = (size_t)cad->como.cadena.longitud;
    if (la > 0 && n > 0 && n > ((size_t)-1) / la) {
        return error_pos(err, linea, columna,
            "ErrorDeValor: repeticion de cadena produce tamaño excesivo");
    }
    size_t total = la * (size_t)n;
    char *buf = (char *)malloc(total + 1);
    if (!buf) return error_pos(err, linea, columna, "memoria insuficiente");
    for (uint64_t i = 0; i < n; i++) {
        if (la > 0) memcpy(buf + (size_t)i * la,
                            cad->como.cadena.texto, la);
    }
    buf[total] = '\0';
    Valor v;
    v.tipo = VAL_CADENA;
    v.dueno_cadena = true;
    v.como.cadena.texto = buf;
    v.como.cadena.longitud = (int)total;
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Comparaciones (devuelven booleano)
 *
 * Para tipos numéricos hacen comparación matemática (entero<->decimal
 * via doble). Para cadenas, lexicográfica byte a byte. Para booleanos,
 * se promueven a entero (false=0, true=1) si la otra parte es numérica.
 * ────────────────────────────────────────────────────────────────── */

typedef enum { ORD_LT = -1, ORD_EQ = 0, ORD_GT = 1, ORD_INCOMP = 2 } Orden;

static Orden comparar_valores(const Valor *a, const Valor *b) {
    /* Cadena vs cadena: lexicográfico. */
    if (a->tipo == VAL_CADENA && b->tipo == VAL_CADENA) {
        int la = a->como.cadena.longitud;
        int lb = b->como.cadena.longitud;
        int min = la < lb ? la : lb;
        int c = (min > 0) ? memcmp(a->como.cadena.texto,
                                    b->como.cadena.texto, (size_t)min) : 0;
        if (c < 0) return ORD_LT;
        if (c > 0) return ORD_GT;
        if (la == lb) return ORD_EQ;
        return la < lb ? ORD_LT : ORD_GT;
    }

    /* Numéricos (incluyendo booleano). */
    bool an_num = (valor_es_entero(a) || a->tipo == VAL_DECIMAL
                   || a->tipo == VAL_BOOLEANO);
    bool bn_num = (valor_es_entero(b) || b->tipo == VAL_DECIMAL
                   || b->tipo == VAL_BOOLEANO);
    if (!an_num || !bn_num) return ORD_INCOMP;

    /* Si ambos son enteros (o booleanos), comparar como bignum. */
    bool a_entero = (valor_es_entero(a) || a->tipo == VAL_BOOLEANO);
    bool b_entero = (valor_es_entero(b) || b->tipo == VAL_BOOLEANO);
    if (a_entero && b_entero) {
        /* v0.11 (B9): camino rápido — si ambos caben en i64 (incluyendo
           SMALL y BOOLEANO), comparar inline sin alocar mp_int. */
        int64_t ai = 0, bi = 0;
        bool a_fits = (a->tipo == VAL_BOOLEANO)
                      ? (ai = a->como.booleano ? 1 : 0, true)
                      : valor_entero_a_i64(a, &ai);
        bool b_fits = (b->tipo == VAL_BOOLEANO)
                      ? (bi = b->como.booleano ? 1 : 0, true)
                      : valor_entero_a_i64(b, &bi);
        if (a_fits && b_fits) {
            if (ai < bi) return ORD_LT;
            if (ai > bi) return ORD_GT;
            return ORD_EQ;
        }
        /* Al menos uno es BIG fuera de i64 — fallback bignum. */
        bool propio_a, propio_b;
        mp_int *ma = como_mp_int(a, &propio_a);
        mp_int *mb = como_mp_int(b, &propio_b);
        if (!ma || !mb) {
            if (propio_a) liberar_mp(ma);
            if (propio_b) liberar_mp(mb);
            return ORD_INCOMP;
        }
        int c = mp_cmp(ma, mb);
        if (propio_a) liberar_mp(ma);
        if (propio_b) liberar_mp(mb);
        if (c == MP_LT) return ORD_LT;
        if (c == MP_GT) return ORD_GT;
        return ORD_EQ;
    }

    /* Caso mixto entero/decimal: convertir a doble. */
    double ad = valor_a_doble(a);
    double bd = valor_a_doble(b);
    if (ad < bd) return ORD_LT;
    if (ad > bd) return ORD_GT;
    return ORD_EQ;
}

static Valor evaluar_comparacion(EvalError *err, TipoToken op,
                                  const Valor *a, const Valor *b,
                                  int linea, int columna) {
    if (op == TT_IGUAL)    return valor_booleano(valor_iguales(a, b));
    if (op == TT_DISTINTO) return valor_booleano(!valor_iguales(a, b));

    Orden o = comparar_valores(a, b);
    if (o == ORD_INCOMP) {
        return error_pos(err, linea, columna,
            "ErrorDeTipo: no se puede comparar '%s' con '%s'",
            valor_nombre_tipo(a), valor_nombre_tipo(b));
    }
    switch (op) {
        case TT_MENOR:        return valor_booleano(o == ORD_LT);
        case TT_MENOR_IGUAL:  return valor_booleano(o != ORD_GT);
        case TT_MAYOR:        return valor_booleano(o == ORD_GT);
        case TT_MAYOR_IGUAL:  return valor_booleano(o != ORD_LT);
        default:
            return error_pos(err, linea, columna, "comparador interno desconocido");
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Operador `es` (identidad) y `en` (pertenencia)
 *
 * Sesión 2: `es` aplica a cualquier par de valores. Para enteros
 * pequeños y booleanos coincide con igualdad estructural; para tipos
 * con identidad de objeto (cadenas, futuras colecciones) pueden diver-
 * gir, pero sin objetos heap interned todavía adoptamos la regla
 * conservadora `a es b ↔ valor_iguales(a, b)` cuando ambos son
 * inmutables. Esto se refinará en F4 S5 con identidad real para
 * funciones y futuras instancias.
 *
 * `en`: para esta sesión solo soporta `subcadena en cadena`. Listas y
 * diccionarios llegan en F5.
 * ────────────────────────────────────────────────────────────────── */

static Valor evaluar_es(const Valor *a, const Valor *b) {
    /* Para funciones (y futura clase/instancia) basta comparar la
       referencia subyacente: dos VAL_FUNCION son la misma si apuntan
       al mismo nodo SENT_FUNCION; dos VAL_NATIVA si comparten el
       mismo puntero a función C. */
    if (a->tipo == VAL_FUNCION && b->tipo == VAL_FUNCION) {
        return valor_booleano(a->como.funcion.def == b->como.funcion.def);
    }
    if (a->tipo == VAL_NATIVA && b->tipo == VAL_NATIVA) {
        return valor_booleano(a->como.nativa.fn == b->como.nativa.fn);
    }
    /* nulo es nulo, verdadero es verdadero, etc. — coincide con igualdad. */
    return valor_booleano(valor_iguales(a, b));
}

static Valor evaluar_en(EvalError *err, const Valor *a, const Valor *b,
                        int linea, int columna) {
    /* `subcadena en cadena` */
    if (b->tipo == VAL_CADENA) {
        if (a->tipo != VAL_CADENA) {
            return error_pos(err, linea, columna,
                "ErrorDeTipo: subcadena debe ser cadena, no '%s'",
                valor_nombre_tipo(a));
        }
        int la = a->como.cadena.longitud;
        int lb = b->como.cadena.longitud;
        if (la == 0) return valor_booleano(true);
        if (la > lb) return valor_booleano(false);
        const char *ta = a->como.cadena.texto;
        const char *tb = b->como.cadena.texto;
        for (int i = 0; i + la <= lb; i++) {
            if (memcmp(tb + i, ta, (size_t)la) == 0) return valor_booleano(true);
        }
        return valor_booleano(false);
    }
    /* `valor en lista` con búsqueda lineal usando igualdad estructural. */
    if (b->tipo == VAL_LISTA) {
        Lista *l = b->como.lista;
        for (int i = 0; i < l->cuenta; i++) {
            if (valor_iguales(a, &l->elementos[i])) return valor_booleano(true);
        }
        return valor_booleano(false);
    }
    /* `clave en diccionario`: búsqueda por hash O(1) amortizado. */
    if (b->tipo == VAL_DICCIONARIO) {
        if (!valor_es_hashable(a)) return valor_booleano(false);
        return valor_booleano(dicc_contiene(b->como.dicc, a));
    }
    /* `elemento en conjunto`: hash O(1). */
    if (b->tipo == VAL_CONJUNTO) {
        if (!valor_es_hashable(a)) return valor_booleano(false);
        return valor_booleano(conj_contiene(b->como.conjunto, a));
    }
    /* `valor en tupla`: búsqueda lineal igual que lista. */
    if (b->tipo == VAL_TUPLA) {
        Tupla *t = b->como.tupla;
        for (int i = 0; i < t->cuenta; i++) {
            if (valor_iguales(a, &t->elementos[i])) return valor_booleano(true);
        }
        return valor_booleano(false);
    }
    return error_pos(err, linea, columna,
        "ErrorDeTipo: el operador 'en' no soporta '%s' a la derecha",
        valor_nombre_tipo(b));
}

/* ──────────────────────────────────────────────────────────────────
 * Despachadores
 * ────────────────────────────────────────────────────────────────── */

static Valor eval_binario(Evaluador *ev, const Expr *e);
static Valor eval_unario(Evaluador *ev, const Expr *e);
static Valor eval_logica(Evaluador *ev, const Expr *e);
static Valor eval_llamada(Evaluador *ev, const Expr *e);
static Valor eval_lista(Evaluador *ev, const Expr *e);
static Valor eval_indice(Evaluador *ev, const Expr *e);
static Valor eval_rebanada(Evaluador *ev, const Expr *e);
static Valor eval_diccionario(Evaluador *ev, const Expr *e);
static Valor eval_conjunto(Evaluador *ev, const Expr *e);
static Valor eval_tupla(Evaluador *ev, const Expr *e);

Valor evaluador_evaluar_expr(Evaluador *ev, const Expr *e) {
    if (ev->error.tuvo_error) return valor_nulo();

    switch (e->tipo) {
        case EXPR_LITERAL_NULO:
            return valor_nulo();
        case EXPR_LITERAL_BOOLEANO:
            return valor_booleano(e->como.booleano.valor);
        case EXPR_LITERAL_ENTERO:
            return valor_entero_de_lexema(e->como.literal.lexema,
                                           e->como.literal.longitud);
        case EXPR_LITERAL_DECIMAL:
            return valor_decimal_de_lexema(e->como.literal.lexema,
                                            e->como.literal.longitud);
        case EXPR_LITERAL_CADENA: {
            /* El lexema incluye comillas: las quitamos y delegamos al
             * helper que procesa los escapes mínimos. v1.14: triples
             * (`"""..."""` o `'''...'''`) detectadas como prefijo de
             * tres delimitadores iguales. */
            const char *lex = e->como.literal.lexema;
            int len = e->como.literal.longitud;
            if (len < 2) return valor_cadena_referencia("", 0);
            int comillas = 1;
            if (len >= 6 && (lex[0] == '"' || lex[0] == '\'')
                          && lex[1] == lex[0] && lex[2] == lex[0]) {
                comillas = 3;
            }
            Valor v = valor_cadena_desde_escapes(lex + comillas,
                                                   len - 2 * comillas);
            if (v.tipo == VAL_NULO) return error_en(ev, e, "memoria insuficiente");
            return v;
        }
        case EXPR_LITERAL_F_CADENA: {
            /* Itera las partes y concatena. Las partes literales pasan
             * por el helper de escapes; las expresión por evaluar +
             * coerción a cadena. */
            Valor acc = valor_cadena_duplicar("", 0);
            if (acc.tipo == VAL_NULO) return error_en(ev, e, "memoria insuficiente");
            int n = e->como.f_cadena.n_partes;
            const ParteFCadena *partes = e->como.f_cadena.partes;
            for (int i = 0; i < n; i++) {
                const ParteFCadena *p = &partes[i];
                Valor pieza;
                if (p->expr) {
                    Valor v = evaluador_evaluar_expr(ev, p->expr);
                    if (ev->error.tuvo_error) {
                        valor_destruir(&v); valor_destruir(&acc);
                        return valor_nulo();
                    }
                    pieza = valor_a_cadena_alocada(&v);
                    valor_destruir(&v);
                } else {
                    pieza = valor_cadena_desde_escapes(p->literal, p->longitud);
                }
                if (pieza.tipo == VAL_NULO) {
                    valor_destruir(&acc);
                    return error_en(ev, e, "memoria insuficiente");
                }
                /* Concatenar acc + pieza in-place. */
                int la = acc.como.cadena.longitud;
                int lp = pieza.como.cadena.longitud;
                char *combinado = (char *)malloc((size_t)(la + lp + 1));
                if (!combinado) {
                    valor_destruir(&acc); valor_destruir(&pieza);
                    return error_en(ev, e, "memoria insuficiente");
                }
                if (la > 0) memcpy(combinado, acc.como.cadena.texto, (size_t)la);
                if (lp > 0) memcpy(combinado + la, pieza.como.cadena.texto, (size_t)lp);
                combinado[la + lp] = '\0';
                valor_destruir(&acc);
                valor_destruir(&pieza);
                acc.tipo = VAL_CADENA;
                acc.dueno_cadena = true;
                acc.como.cadena.texto = combinado;
                acc.como.cadena.longitud = la + lp;
            }
            return acc;
        }

        case EXPR_IDENT: {
            Valor v;
            if (!entorno_obtener(ev->entorno_actual,
                                  e->como.ident.nombre,
                                  e->como.ident.longitud, &v)) {
                return error_en(ev, e,
                    "ErrorDeNombre: nombre '%.*s' no esta definido",
                    e->como.ident.longitud, e->como.ident.nombre);
            }
            return v;
        }

        case EXPR_GRUPO:
            return evaluador_evaluar_expr(ev, e->como.grupo.interna);

        case EXPR_BINARIO:  return eval_binario(ev, e);
        case EXPR_UNARIO:   return eval_unario(ev, e);
        case EXPR_LOGICA:   return eval_logica(ev, e);

        case EXPR_LLAMADA:     return eval_llamada(ev, e);
        case EXPR_LISTA:       return eval_lista(ev, e);
        case EXPR_INDICE:      return eval_indice(ev, e);
        case EXPR_REBANADA:    return eval_rebanada(ev, e);
        case EXPR_DICCIONARIO: return eval_diccionario(ev, e);
        case EXPR_CONJUNTO:    return eval_conjunto(ev, e);
        case EXPR_TUPLA:       return eval_tupla(ev, e);

        case EXPR_ATRIBUTO:
        case EXPR_LAMBDA:
        case EXPR_SUPER:
        case EXPR_TERNARIA:
            return error_en(ev, e,
                "esta forma de expresion aun no esta implementada en v0.5");
    }
    return error_en(ev, e, "tipo de expresion desconocido");
}

/* ──────────────────────────────────────────────────────────────────
 * Binario
 * ────────────────────────────────────────────────────────────────── */

static bool es_numerico(const Valor *v) {
    return valor_es_entero(v) || v->tipo == VAL_DECIMAL
        || v->tipo == VAL_BOOLEANO;
}

static bool es_aritmetico(TipoToken op) {
    switch (op) {
        case TT_MAS: case TT_MENOS: case TT_ASTERISCO:
        case TT_BARRA: case TT_DOBLE_BARRA:
        case TT_PORCENTAJE: case TT_DOBLE_ASTERISCO:
            return true;
        default: return false;
    }
}

static bool es_bitwise(TipoToken op) {
    switch (op) {
        case TT_AMPERSAND: case TT_BARRA_VERT: case TT_CIRCUNFLEJO:
        case TT_DESPL_IZQ: case TT_DESPL_DER:
            return true;
        default: return false;
    }
}

static bool es_comparacion(TipoToken op) {
    switch (op) {
        case TT_IGUAL: case TT_DISTINTO:
        case TT_MENOR: case TT_MENOR_IGUAL:
        case TT_MAYOR: case TT_MAYOR_IGUAL:
            return true;
        default: return false;
    }
}

/*
 * Aplica un operador binario sobre dos valores ya evaluados, tomando
 * posesión de ambos (los destruye antes de devolver). Útil tanto para
 * EXPR_BINARIO como para SENT_ASIGNAR_AUG (`x += expr` → x = x op expr).
 */
static Valor aplicar_binario_pos(EvalError *err, TipoToken op,
                                  Valor a, Valor b,
                                  int linea, int columna) {
    Valor resultado = valor_nulo();

    /* Identidad y pertenencia. */
    if (op == TT_ES) {
        resultado = evaluar_es(&a, &b);
        valor_destruir(&a); valor_destruir(&b);
        return resultado;
    }
    if (op == TT_EN) {
        resultado = evaluar_en(err, &a, &b, linea, columna);
        valor_destruir(&a); valor_destruir(&b);
        return resultado;
    }

    /* Comparaciones (devuelven booleano). */
    if (es_comparacion(op)) {
        resultado = evaluar_comparacion(err, op, &a, &b, linea, columna);
        valor_destruir(&a); valor_destruir(&b);
        return resultado;
    }

    /* Concatenación y repetición de cadena. */
    if (op == TT_MAS && a.tipo == VAL_CADENA && b.tipo == VAL_CADENA) {
        resultado = cadena_concatenar(err, &a, &b, linea, columna);
        valor_destruir(&a); valor_destruir(&b);
        return resultado;
    }
    if (op == TT_ASTERISCO
        && ((a.tipo == VAL_CADENA && (valor_es_entero(&b) || b.tipo == VAL_BOOLEANO))
         || (b.tipo == VAL_CADENA && (valor_es_entero(&a) || a.tipo == VAL_BOOLEANO)))) {
        const Valor *cad = (a.tipo == VAL_CADENA) ? &a : &b;
        const Valor *otr = (a.tipo == VAL_CADENA) ? &b : &a;
        bool propio;
        mp_int *m = como_mp_int(otr, &propio);
        if (!m) {
            valor_destruir(&a); valor_destruir(&b);
            return error_pos(err, linea, columna, "memoria insuficiente");
        }
        resultado = cadena_repetir(err, cad, m, linea, columna);
        if (propio) liberar_mp(m);
        valor_destruir(&a); valor_destruir(&b);
        return resultado;
    }

    /* Concatenación de listas. */
    if (op == TT_MAS && a.tipo == VAL_LISTA && b.tipo == VAL_LISTA) {
        Lista *la = a.como.lista;
        Lista *lb = b.como.lista;
        Lista *nueva = lista_nueva(la->cuenta + lb->cuenta);
        if (!nueva) {
            valor_destruir(&a); valor_destruir(&b);
            return error_pos(err, linea, columna, "memoria insuficiente");
        }
        for (int i = 0; i < la->cuenta; i++) {
            lista_agregar(nueva, valor_clonar(&la->elementos[i]));
        }
        for (int i = 0; i < lb->cuenta; i++) {
            lista_agregar(nueva, valor_clonar(&lb->elementos[i]));
        }
        valor_destruir(&a); valor_destruir(&b);
        return valor_lista(nueva);
    }

    /* Repetición de lista. */
    if (op == TT_ASTERISCO
        && ((a.tipo == VAL_LISTA && (valor_es_entero(&b) || b.tipo == VAL_BOOLEANO))
         || (b.tipo == VAL_LISTA && (valor_es_entero(&a) || a.tipo == VAL_BOOLEANO)))) {
        const Valor *vlst = (a.tipo == VAL_LISTA) ? &a : &b;
        const Valor *vnum = (a.tipo == VAL_LISTA) ? &b : &a;
        bool propio;
        mp_int *m = como_mp_int(vnum, &propio);
        if (!m) {
            valor_destruir(&a); valor_destruir(&b);
            return error_pos(err, linea, columna, "memoria insuficiente");
        }
        if (mp_isneg(m) == MP_YES) {
            if (propio) liberar_mp(m);
            valor_destruir(&a); valor_destruir(&b);
            return valor_lista(lista_nueva(0));
        }
        if (mp_count_bits(m) > 31) {
            if (propio) liberar_mp(m);
            valor_destruir(&a); valor_destruir(&b);
            return error_pos(err, linea, columna,
                "ErrorDeValor: repeticion de lista demasiado grande");
        }
        long n_rep = (long)mp_get_u64(m);
        if (propio) liberar_mp(m);

        Lista *src = vlst->como.lista;
        Lista *nueva = lista_nueva((int)(n_rep * src->cuenta));
        if (!nueva) {
            valor_destruir(&a); valor_destruir(&b);
            return error_pos(err, linea, columna, "memoria insuficiente");
        }
        for (long k = 0; k < n_rep; k++) {
            for (int i = 0; i < src->cuenta; i++) {
                lista_agregar(nueva, valor_clonar(&src->elementos[i]));
            }
        }
        valor_destruir(&a); valor_destruir(&b);
        return valor_lista(nueva);
    }

    /* Bitwise. */
    if (es_bitwise(op)) {
        if (!(valor_es_entero(&a) || a.tipo == VAL_BOOLEANO)
         || !(valor_es_entero(&b) || b.tipo == VAL_BOOLEANO)) {
            resultado = error_pos(err, linea, columna,
                "ErrorDeTipo: operador bitwise requiere enteros, no '%s' y '%s'",
                valor_nombre_tipo(&a), valor_nombre_tipo(&b));
            valor_destruir(&a); valor_destruir(&b);
            return resultado;
        }
        bool pa, pb;
        mp_int *ma = como_mp_int(&a, &pa);
        mp_int *mb = como_mp_int(&b, &pb);
        resultado = entero_op_entero(err, op, ma, mb, linea, columna);
        if (pa) liberar_mp(ma);
        if (pb) liberar_mp(mb);
        valor_destruir(&a); valor_destruir(&b);
        return resultado;
    }

    /* Aritmética. */
    if (es_aritmetico(op)) {
        if (!es_numerico(&a) || !es_numerico(&b)) {
            resultado = error_pos(err, linea, columna,
                "ErrorDeTipo: operador '%s' no aplica entre '%s' y '%s'",
                op == TT_MAS ? "+" :
                op == TT_MENOS ? "-" :
                op == TT_ASTERISCO ? "*" :
                op == TT_BARRA ? "/" :
                op == TT_DOBLE_BARRA ? "//" :
                op == TT_PORCENTAJE ? "%" :
                op == TT_DOBLE_ASTERISCO ? "**" : "?",
                valor_nombre_tipo(&a), valor_nombre_tipo(&b));
            valor_destruir(&a); valor_destruir(&b);
            return resultado;
        }

        if (op == TT_BARRA) {
            double ad = valor_a_doble(&a);
            double bd = valor_a_doble(&b);
            valor_destruir(&a); valor_destruir(&b);
            if (bd == 0.0) {
                return error_pos(err, linea, columna,
                    "ErrorAritmetico: division por cero");
            }
            return valor_decimal(ad / bd);
        }

        bool a_dec = (a.tipo == VAL_DECIMAL);
        bool b_dec = (b.tipo == VAL_DECIMAL);
        if (a_dec || b_dec) {
            double ad = valor_a_doble(&a);
            double bd = valor_a_doble(&b);
            resultado = decimal_op_decimal(err, op, ad, bd, linea, columna);
            valor_destruir(&a); valor_destruir(&b);
            return resultado;
        }

        /* v0.11 (B9): camino rápido SMALL+SMALL sin tocar libtommath. */
        if (a.tipo == VAL_ENTERO_SMALL && b.tipo == VAL_ENTERO_SMALL) {
            bool aplicable;
            resultado = small_op_small(err, op,
                a.como.entero_small, b.como.entero_small,
                linea, columna, &aplicable);
            if (aplicable) {
                valor_destruir(&a); valor_destruir(&b);
                return resultado;
            }
            /* Overflow o op no especializada: caer al path BIG. */
        }

        bool pa, pb;
        mp_int *ma = como_mp_int(&a, &pa);
        mp_int *mb = como_mp_int(&b, &pb);
        if (!ma || !mb) {
            if (pa) liberar_mp(ma);
            if (pb) liberar_mp(mb);
            valor_destruir(&a); valor_destruir(&b);
            return error_pos(err, linea, columna, "memoria insuficiente");
        }
        resultado = entero_op_entero(err, op, ma, mb, linea, columna);
        if (pa) liberar_mp(ma);
        if (pb) liberar_mp(mb);
        valor_destruir(&a); valor_destruir(&b);
        return resultado;
    }

    valor_destruir(&a); valor_destruir(&b);
    return error_pos(err, linea, columna, "operador binario desconocido");
}

/* Wrapper compatible con call-sites antiguos (pasar Evaluador + Expr). */
static Valor aplicar_binario(Evaluador *ev, TipoToken op,
                             Valor a, Valor b, const Expr *e) {
    return aplicar_binario_pos(&ev->error, op, a, b, e->linea, e->columna);
}

/* API pública: idéntica a aplicar_binario_pos pero con prefijo. */
Valor evaluador_aplicar_binario(EvalError *err, int op_token,
                                 Valor a, Valor b,
                                 int linea, int columna) {
    return aplicar_binario_pos(err, (TipoToken)op_token, a, b, linea, columna);
}

static Valor eval_binario(Evaluador *ev, const Expr *e) {
    Valor a = evaluador_evaluar_expr(ev, e->como.binario.izq);
    if (ev->error.tuvo_error) { valor_destruir(&a); return valor_nulo(); }
    Valor b = evaluador_evaluar_expr(ev, e->como.binario.der);
    if (ev->error.tuvo_error) {
        valor_destruir(&a); valor_destruir(&b); return valor_nulo();
    }
    return aplicar_binario(ev, e->como.binario.op, a, b, e);
}

/* ──────────────────────────────────────────────────────────────────
 * Unario
 * ────────────────────────────────────────────────────────────────── */

/*
 * Aplica un operador unario sobre un valor ya evaluado, tomando
 * posesión. Reutilizable desde la VM bytecode.
 */
static Valor aplicar_unario_pos(EvalError *err, TipoToken op,
                                 Valor v,
                                 int linea, int columna) {
    switch (op) {
        case TT_NO:
            return (Valor){
                .tipo = VAL_BOOLEANO,
                .dueno_cadena = false,
                .como.booleano = !valor_es_verdadero(&v),
            };

        case TT_MAS:
            if (valor_es_entero(&v) || v.tipo == VAL_DECIMAL) return v;
            if (v.tipo == VAL_BOOLEANO) {
                bool b = v.como.booleano;
                valor_destruir(&v);
                return valor_entero_de_long(b ? 1 : 0);
            }
            valor_destruir(&v);
            return error_pos(err, linea, columna,
                "ErrorDeTipo: el operador '+' unario requiere numerico");

        case TT_MENOS: {
            if (v.tipo == VAL_DECIMAL) {
                double d = -v.como.decimal;
                valor_destruir(&v);
                return valor_decimal(d);
            }
            if (v.tipo == VAL_ENTERO_SMALL) {
                int64_t n = v.como.entero_small;
                /* -SMALL_INT_MIN no cabe en SMALL — promote a BIG. */
                if (n == CORNAMUSA_SMALL_INT_MIN) {
                    mp_int *r = nuevo_mp();
                    if (!r) return error_pos(err, linea, columna, "memoria insuficiente");
                    mp_set_i64(r, n);
                    if (mp_neg(r, r) != MP_OKAY) {
                        liberar_mp(r);
                        return error_pos(err, linea, columna, "fallo en negacion entera");
                    }
                    return valor_entero_de_mp_normalizado(r);
                }
                return valor_entero_de_i64(-n);
            }
            if (v.tipo == VAL_ENTERO) {
                mp_int *r = nuevo_mp();
                if (!r) {
                    valor_destruir(&v);
                    return error_pos(err, linea, columna, "memoria insuficiente");
                }
                if (mp_neg(v.como.entero, r) != MP_OKAY) {
                    liberar_mp(r); valor_destruir(&v);
                    return error_pos(err, linea, columna, "fallo en negacion entera");
                }
                valor_destruir(&v);
                return valor_entero_de_mp_normalizado(r);
            }
            if (v.tipo == VAL_BOOLEANO) {
                long n = v.como.booleano ? -1 : 0;
                valor_destruir(&v);
                return valor_entero_de_long(n);
            }
            valor_destruir(&v);
            return error_pos(err, linea, columna,
                "ErrorDeTipo: el operador '-' unario requiere numerico");
        }

        case TT_TILDE_BIT: {
            if (valor_es_entero(&v) || v.tipo == VAL_BOOLEANO) {
                bool propio;
                mp_int *m = como_mp_int(&v, &propio);
                if (!m) {
                    valor_destruir(&v);
                    return error_pos(err, linea, columna, "memoria insuficiente");
                }
                mp_int *r = nuevo_mp();
                if (!r) {
                    if (propio) liberar_mp(m);
                    valor_destruir(&v);
                    return error_pos(err, linea, columna, "memoria insuficiente");
                }
                if (mp_complement(m, r) != MP_OKAY) {
                    liberar_mp(r);
                    if (propio) liberar_mp(m);
                    valor_destruir(&v);
                    return error_pos(err, linea, columna, "fallo en complemento bit a bit");
                }
                if (propio) liberar_mp(m);
                valor_destruir(&v);
                return valor_entero_de_mp_normalizado(r);
            }
            valor_destruir(&v);
            return error_pos(err, linea, columna,
                "ErrorDeTipo: el operador '~' requiere entero");
        }

        default:
            valor_destruir(&v);
            return error_pos(err, linea, columna, "operador unario desconocido");
    }
}

/* API pública para reutilización desde la VM bytecode. */
Valor evaluador_aplicar_unario(EvalError *err, int op_token,
                                Valor v,
                                int linea, int columna) {
    return aplicar_unario_pos(err, (TipoToken)op_token, v, linea, columna);
}

static Valor eval_unario(Evaluador *ev, const Expr *e) {
    Valor v = evaluador_evaluar_expr(ev, e->como.unario.operando);
    if (ev->error.tuvo_error) { valor_destruir(&v); return valor_nulo(); }
    return aplicar_unario_pos(&ev->error, e->como.unario.op, v,
                               e->linea, e->columna);
}

/* ──────────────────────────────────────────────────────────────────
 * Lógica con cortocircuito
 *
 * `a y b`: si `a` es falso, devuelve `a` sin evaluar `b`.
 * `a o b`: si `a` es verdadero, devuelve `a` sin evaluar `b`.
 * Devuelve el valor "decisor" original (no booleano), igual que Python.
 * ────────────────────────────────────────────────────────────────── */

static Valor eval_logica(Evaluador *ev, const Expr *e) {
    Valor izq = evaluador_evaluar_expr(ev, e->como.logica.izq);
    if (ev->error.tuvo_error) { valor_destruir(&izq); return valor_nulo(); }

    bool es_y = e->como.logica.es_y;
    bool izq_v = valor_es_verdadero(&izq);

    if (es_y) {
        if (!izq_v) return izq;     /* `falso y X` → `falso` (sin evaluar X) */
    } else {
        if (izq_v)  return izq;     /* `verdadero o X` → `verdadero` */
    }

    valor_destruir(&izq);
    return evaluador_evaluar_expr(ev, e->como.logica.der);
}

/* ──────────────────────────────────────────────────────────────────
 * Sentencias
 *
 * Sesión 3: ejecuta sentencias simples y de control de flujo.
 *   - SENT_EXPR / SENT_PASAR
 *   - SENT_ASIGNAR (solo destino IDENT en esta versión)
 *   - SENT_ASIGNAR_AUG (`x += expr` y similares)
 *   - SENT_ROMPER / SENT_CONTINUAR
 *   - SENT_SI con cadenas `sino si`/`sino`
 *   - SENT_MIENTRAS con cláusula `sino` opcional (Python-style)
 *   - SENT_PARA iterando sobre cadenas (un Valor cadena por code-point UTF-8)
 *   - SENT_BLOQUE
 *
 * Aplazadas (devuelven error explícito o ya errado por el parser):
 *   - SENT_FUNCION / SENT_RETORNAR / SENT_CLASE → S4
 *   - SENT_INTENTAR / SENT_LANZAR / SENT_IMPORTAR / SENT_DESDE_IMPORTAR
 *     / SENT_GLOBAL / SENT_NOLOCAL → F5+
 * ────────────────────────────────────────────────────────────────── */

#include "utf8proc.h"

static void sent_set_error(Evaluador *ev, const Sent *s, const char *fmt, ...) {
    if (ev->error.tuvo_error) return;
    ev->error.tuvo_error = true;
    ev->error.linea = s->linea;
    ev->error.columna = s->columna;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ev->error.mensaje, sizeof(ev->error.mensaje), fmt, ap);
    va_end(ap);
}

/* Mapea token de asignación aumentada al operador binario equivalente. */
static TipoToken aug_a_binario(TipoToken aug) {
    switch (aug) {
        case TT_ASIGNAR_MAS:        return TT_MAS;
        case TT_ASIGNAR_MENOS:      return TT_MENOS;
        case TT_ASIGNAR_ASTERISCO:  return TT_ASTERISCO;
        case TT_ASIGNAR_BARRA:      return TT_BARRA;
        case TT_ASIGNAR_DOBLE_BARRA: return TT_DOBLE_BARRA;
        case TT_ASIGNAR_PORCENTAJE: return TT_PORCENTAJE;
        case TT_ASIGNAR_DOBLE_ASTER: return TT_DOBLE_ASTERISCO;
        default: return aug;
    }
}

/* Ejecuta una sentencia de bloque. */
static void ejec_bloque(Evaluador *ev, const Sent *s) {
    int n = s->como.bloque.n_sentencias;
    Sent **sentencias = s->como.bloque.sentencias;
    for (int i = 0; i < n; i++) {
        evaluador_ejecutar_sent(ev, sentencias[i]);
        if (ev->error.tuvo_error) return;
        if (ev->control != EJEC_NORMAL) return;
    }
}

/*
 * Helper: convierte un Valor entero/booleano en `long` y normaliza
 * un índice negativo a positivo respecto a `total`. Devuelve true si
 * OK; false si tipo incorrecto o fuera de rango.
 */
static bool indice_normalizar(const Valor *idx, int total, long *out) {
    int64_t i64;
    if (idx->tipo == VAL_BOOLEANO) { i64 = idx->como.booleano ? 1 : 0; }
    else if (!valor_entero_a_i64(idx, &i64)) return false;
    long i = (long)i64;
    if ((int64_t)i != i64) return false;  /* no cabe en long */
    if (i < 0) i += total;
    if (i < 0 || i >= total) return false;
    *out = i;
    return true;
}

/*
 * Asignación a `lista[i] = valor`. Destruye el valor anterior y
 * almacena el nuevo. Reporta `ErrorDeIndice` si fuera de rango.
 */
static void asignar_indice(Evaluador *ev, const Sent *s, Expr *destino,
                            Valor nuevo) {
    Valor obj = evaluador_evaluar_expr(ev, destino->como.indice.objeto);
    if (ev->error.tuvo_error) {
        valor_destruir(&obj); valor_destruir(&nuevo);
        return;
    }
    Valor idx = evaluador_evaluar_expr(ev, destino->como.indice.indice);
    if (ev->error.tuvo_error) {
        valor_destruir(&obj); valor_destruir(&idx); valor_destruir(&nuevo);
        return;
    }

    if (obj.tipo == VAL_LISTA) {
        Lista *l = obj.como.lista;
        long i;
        if (!indice_normalizar(&idx, l->cuenta, &i)) {
            sent_set_error(ev, s,
                "ErrorDeIndice: indice fuera de rango (lista de %d)", l->cuenta);
            valor_destruir(&obj); valor_destruir(&idx); valor_destruir(&nuevo);
            return;
        }
        lista_asignar(l, (int)i, nuevo);
        valor_destruir(&obj); valor_destruir(&idx);
        return;
    }

    if (obj.tipo == VAL_DICCIONARIO) {
        if (!valor_es_hashable(&idx)) {
            sent_set_error(ev, s,
                "ErrorDeTipo: '%s' no se puede usar como clave de diccionario",
                valor_nombre_tipo(&idx));
            valor_destruir(&obj); valor_destruir(&idx); valor_destruir(&nuevo);
            return;
        }
        if (!dicc_asignar(obj.como.dicc, idx, nuevo)) {
            sent_set_error(ev, s, "memoria insuficiente al asignar al diccionario");
        }
        /* `idx` y `nuevo` transferidos a la entrada (o destruidos en error). */
        valor_destruir(&obj);
        return;
    }

    sent_set_error(ev, s,
        "ErrorDeTipo: '%s' no soporta asignacion por indice",
        valor_nombre_tipo(&obj));
    valor_destruir(&obj); valor_destruir(&idx); valor_destruir(&nuevo);
}

/*
 * SENT_ASIGNAR: dos destinos soportados en v0.5:
 *   - IDENT: define o sobrescribe en el entorno actual.
 *   - INDICE (`lista[i] = v`): asigna en la lista referenciada.
 * Tuple destructuring y atributos (`obj.x = v`) llegan después.
 */
/*
 * v1.21: destructuring recursivo. `valor` se consume.
 * Soporta RHS de tipo VAL_TUPLA, VAL_LISTA o VAL_CADENA (iterables
 * indexables con longitud conocida). Aridad mismatch → ErrorDeValor.
 * Tipo no soportado → ErrorDeTipo.
 */
static void asignar_destructuring(Evaluador *ev, const Sent *s,
                                    const Expr *patron, Valor valor) {
    int n_destino = patron->como.secuencia.n_elementos;
    Expr **elementos = patron->como.secuencia.elementos;

    /* Determinar n_origen según tipo. */
    int n_origen = -1;
    if (valor.tipo == VAL_TUPLA) {
        n_origen = valor.como.tupla->cuenta;
    } else if (valor.tipo == VAL_LISTA) {
        n_origen = valor.como.lista->cuenta;
    } else if (valor.tipo == VAL_CADENA) {
        /* Longitud en code points; iteramos por code points abajo. */
        const char *t = valor.como.cadena.texto;
        int len = valor.como.cadena.longitud;
        int cp = 0;
        for (int i = 0; i < len; ) {
            unsigned char b = (unsigned char)t[i];
            if      (b < 0x80) i += 1;
            else if ((b >> 5) == 0x6) i += 2;
            else if ((b >> 4) == 0xE) i += 3;
            else if ((b >> 3) == 0x1E) i += 4;
            else i += 1;
            cp++;
        }
        n_origen = cp;
    } else {
        sent_set_error(ev, s,
            "ErrorDeTipo: '%s' no soporta destructuring",
            valor_nombre_tipo(&valor));
        valor_destruir(&valor);
        return;
    }

    if (n_origen != n_destino) {
        sent_set_error(ev, s,
            "ErrorDeValor: aridad incorrecta en destructuring "
            "(esperaba %d, recibió %d)", n_destino, n_origen);
        valor_destruir(&valor);
        return;
    }

    for (int i = 0; i < n_destino; i++) {
        Valor elem;
        if (valor.tipo == VAL_TUPLA) {
            elem = valor_clonar(&valor.como.tupla->elementos[i]);
        } else if (valor.tipo == VAL_LISTA) {
            elem = valor_clonar(&valor.como.lista->elementos[i]);
        } else { /* VAL_CADENA: extraer code point i como cadena */
            const char *t = valor.como.cadena.texto;
            int len = valor.como.cadena.longitud;
            int idx = 0, byte_inicio = 0;
            int j = 0;
            while (j < len && idx <= i) {
                if (idx == i) { byte_inicio = j; }
                unsigned char b = (unsigned char)t[j];
                int adv;
                if      (b < 0x80) adv = 1;
                else if ((b >> 5) == 0x6) adv = 2;
                else if ((b >> 4) == 0xE) adv = 3;
                else if ((b >> 3) == 0x1E) adv = 4;
                else adv = 1;
                if (idx == i) {
                    elem = valor_cadena_duplicar(t + byte_inicio, adv);
                    j += adv; idx++; goto extraido;
                }
                j += adv; idx++;
            }
            elem = valor_nulo();
        extraido: ;
        }

        const Expr *dst_i = elementos[i];
        if (dst_i->tipo == EXPR_IDENT) {
            if (!entorno_definir(ev->entorno_actual,
                                  dst_i->como.ident.nombre,
                                  dst_i->como.ident.longitud, elem)) {
                sent_set_error(ev, s, "memoria insuficiente al asignar");
                valor_destruir(&valor);
                return;
            }
        } else if (dst_i->tipo == EXPR_TUPLA
                   || dst_i->tipo == EXPR_LISTA) {
            asignar_destructuring(ev, s, dst_i, elem);
            if (ev->error.tuvo_error) {
                valor_destruir(&valor);
                return;
            }
        } else {
            sent_set_error(ev, s,
                "ErrorDeSintaxis: destino de destructuring debe ser "
                "identificador o tupla/lista anidada");
            valor_destruir(&elem);
            valor_destruir(&valor);
            return;
        }
    }
    valor_destruir(&valor);
}

static void ejec_asignar(Evaluador *ev, const Sent *s) {
    Expr *destino = s->como.asignar.destino;

    if (destino->tipo == EXPR_IDENT) {
        Valor v = evaluador_evaluar_expr(ev, s->como.asignar.valor);
        if (ev->error.tuvo_error) { valor_destruir(&v); return; }
        if (!entorno_definir(ev->entorno_actual,
                              destino->como.ident.nombre,
                              destino->como.ident.longitud, v)) {
            sent_set_error(ev, s, "memoria insuficiente al asignar");
        }
        return;
    }

    if (destino->tipo == EXPR_INDICE) {
        Valor v = evaluador_evaluar_expr(ev, s->como.asignar.valor);
        if (ev->error.tuvo_error) { valor_destruir(&v); return; }
        asignar_indice(ev, s, destino, v);
        return;
    }

    /* v1.21: destructuring `a, b = par` o `[a, b] = lista`. */
    if (destino->tipo == EXPR_TUPLA || destino->tipo == EXPR_LISTA) {
        Valor v = evaluador_evaluar_expr(ev, s->como.asignar.valor);
        if (ev->error.tuvo_error) { valor_destruir(&v); return; }
        asignar_destructuring(ev, s, destino, v);
        return;
    }

    sent_set_error(ev, s,
        "ErrorDeSintaxis: destino de asignacion no soportado en v0.5");
}

/*
 * SENT_ASIGNAR_AUG: `x op= expr`. El destino debe estar previamente
 * definido (semántica Python: NameError si x no existe). Soporta:
 *   - IDENT: lee del entorno, computa, escribe.
 *   - INDICE: lee de lista[i], computa, escribe.
 */
static void ejec_asignar_aug(Evaluador *ev, const Sent *s) {
    Expr *destino = s->como.asignar_aug.destino;
    TipoToken op = aug_a_binario(s->como.asignar_aug.op);

    if (destino->tipo == EXPR_IDENT) {
        Valor actual;
        if (!entorno_obtener(ev->entorno_actual,
                              destino->como.ident.nombre,
                              destino->como.ident.longitud, &actual)) {
            sent_set_error(ev, s,
                "ErrorDeNombre: nombre '%.*s' no esta definido",
                destino->como.ident.longitud, destino->como.ident.nombre);
            return;
        }
        Valor incremento = evaluador_evaluar_expr(ev, s->como.asignar_aug.valor);
        if (ev->error.tuvo_error) {
            valor_destruir(&actual); valor_destruir(&incremento);
            return;
        }
        Valor resultado = aplicar_binario(ev, op, actual, incremento, destino);
        if (ev->error.tuvo_error) { valor_destruir(&resultado); return; }
        if (!entorno_definir(ev->entorno_actual,
                              destino->como.ident.nombre,
                              destino->como.ident.longitud, resultado)) {
            sent_set_error(ev, s, "memoria insuficiente al asignar");
        }
        return;
    }

    if (destino->tipo == EXPR_INDICE) {
        /* Evaluar objeto e índice una sola vez (idéntico a Python). */
        Valor obj = evaluador_evaluar_expr(ev, destino->como.indice.objeto);
        if (ev->error.tuvo_error) { valor_destruir(&obj); return; }
        Valor idx = evaluador_evaluar_expr(ev, destino->como.indice.indice);
        if (ev->error.tuvo_error) {
            valor_destruir(&obj); valor_destruir(&idx); return;
        }

        if (obj.tipo == VAL_LISTA) {
            Lista *l = obj.como.lista;
            long i;
            if (!indice_normalizar(&idx, l->cuenta, &i)) {
                sent_set_error(ev, s,
                    "ErrorDeIndice: indice fuera de rango (lista de %d)", l->cuenta);
                valor_destruir(&obj); valor_destruir(&idx); return;
            }
            Valor actual = valor_clonar(&l->elementos[i]);
            Valor incremento = evaluador_evaluar_expr(ev, s->como.asignar_aug.valor);
            if (ev->error.tuvo_error) {
                valor_destruir(&actual); valor_destruir(&incremento);
                valor_destruir(&obj); valor_destruir(&idx); return;
            }
            Valor resultado = aplicar_binario(ev, op, actual, incremento, destino);
            if (ev->error.tuvo_error) {
                valor_destruir(&resultado);
                valor_destruir(&obj); valor_destruir(&idx); return;
            }
            lista_asignar(l, (int)i, resultado);
            valor_destruir(&obj); valor_destruir(&idx);
            return;
        }

        if (obj.tipo == VAL_DICCIONARIO) {
            if (!valor_es_hashable(&idx)) {
                sent_set_error(ev, s,
                    "ErrorDeTipo: '%s' no es hashable", valor_nombre_tipo(&idx));
                valor_destruir(&obj); valor_destruir(&idx); return;
            }
            Valor actual;
            if (!dicc_obtener(obj.como.dicc, &idx, &actual)) {
                char buf[128];
                valor_a_repr(&idx, buf, sizeof(buf));
                sent_set_error(ev, s, "ErrorDeClave: %s", buf);
                valor_destruir(&obj); valor_destruir(&idx); return;
            }
            Valor incremento = evaluador_evaluar_expr(ev, s->como.asignar_aug.valor);
            if (ev->error.tuvo_error) {
                valor_destruir(&actual); valor_destruir(&incremento);
                valor_destruir(&obj); valor_destruir(&idx); return;
            }
            Valor resultado = aplicar_binario(ev, op, actual, incremento, destino);
            if (ev->error.tuvo_error) {
                valor_destruir(&resultado);
                valor_destruir(&obj); valor_destruir(&idx); return;
            }
            /* Asignar reusa la clave clonada — necesitamos clonarla
               porque idx aún la posee y la destruiremos al final. */
            Valor clave_copia = valor_clonar(&idx);
            dicc_asignar(obj.como.dicc, clave_copia, resultado);
            valor_destruir(&obj); valor_destruir(&idx);
            return;
        }

        sent_set_error(ev, s,
            "ErrorDeTipo: '%s' no soporta asignacion aumentada por indice",
            valor_nombre_tipo(&obj));
        valor_destruir(&obj); valor_destruir(&idx);
        return;
    }

    sent_set_error(ev, s,
        "ErrorDeSintaxis: destino de asignacion aumentada no soportado en v0.5");
}

static void ejec_si(Evaluador *ev, const Sent *s) {
    int n = s->como.si.n_ramas;
    RamaSi *ramas = s->como.si.ramas;
    for (int i = 0; i < n; i++) {
        Expr *cond = ramas[i].condicion;
        bool tomar;
        if (cond == NULL) {
            /* rama 'sino' final: siempre se toma si llegamos aquí. */
            tomar = true;
        } else {
            Valor cv = evaluador_evaluar_expr(ev, cond);
            if (ev->error.tuvo_error) { valor_destruir(&cv); return; }
            tomar = valor_es_verdadero(&cv);
            valor_destruir(&cv);
        }
        if (tomar) {
            evaluador_ejecutar_sent(ev, ramas[i].cuerpo);
            return;
        }
    }
}

static void ejec_mientras(Evaluador *ev, const Sent *s) {
    bool rompio = false;
    while (true) {
        Valor cv = evaluador_evaluar_expr(ev, s->como.mientras.condicion);
        if (ev->error.tuvo_error) { valor_destruir(&cv); return; }
        bool seguir = valor_es_verdadero(&cv);
        valor_destruir(&cv);
        if (!seguir) break;

        evaluador_ejecutar_sent(ev, s->como.mientras.cuerpo);
        if (ev->error.tuvo_error) return;

        if (ev->control == EJEC_ROMPER) {
            ev->control = EJEC_NORMAL;
            rompio = true;
            break;
        }
        if (ev->control == EJEC_CONTINUAR) {
            ev->control = EJEC_NORMAL;
            continue;
        }
        if (ev->control == EJEC_RETORNAR) {
            return;  /* propagar al frame de la función (S4) */
        }
    }
    /* Cláusula `sino`: solo si el bucle terminó por condición falsa. */
    if (!rompio && s->como.mientras.sino != NULL) {
        evaluador_ejecutar_sent(ev, s->como.mientras.sino);
    }
}

/*
 * SENT_PARA en v0.4 S3: iterable es una cadena. Cada iteración liga
 * el objetivo a un Valor cadena de un code-point UTF-8 (lo que permite
 * recorrer correctamente "niño" como ['n','i','ñ','o']).
 *
 * Otros iterables (rango, lista, diccionario) llegarán cuando lleguen
 * los built-ins (S4) y las colecciones (F5).
 */
static void ejec_para(Evaluador *ev, const Sent *s) {
    Expr *objetivo = s->como.para.objetivo;
    if (objetivo->tipo != EXPR_IDENT) {
        sent_set_error(ev, s,
            "ErrorDeSintaxis: objetivo de 'para' debe ser un identificador en v0.4");
        return;
    }

    Valor iter = evaluador_evaluar_expr(ev, s->como.para.iterable);
    if (ev->error.tuvo_error) { valor_destruir(&iter); return; }

    if (iter.tipo != VAL_CADENA && iter.tipo != VAL_RANGO
        && iter.tipo != VAL_LISTA && iter.tipo != VAL_DICCIONARIO
        && iter.tipo != VAL_CONJUNTO && iter.tipo != VAL_TUPLA) {
        sent_set_error(ev, s,
            "ErrorDeTipo: 'para' no soporta iterar sobre '%s' en v0.5",
            valor_nombre_tipo(&iter));
        valor_destruir(&iter);
        return;
    }

    /* Iteración sobre tupla — análoga a lista. */
    if (iter.tipo == VAL_TUPLA) {
        Tupla *t = iter.como.tupla;
        bool rompio_t = false;
        for (int i = 0; i < t->cuenta; i++) {
            Valor v = valor_clonar(&t->elementos[i]);
            if (!entorno_definir(ev->entorno_actual,
                                  objetivo->como.ident.nombre,
                                  objetivo->como.ident.longitud, v)) {
                sent_set_error(ev, s, "memoria insuficiente en 'para'");
                break;
            }
            evaluador_ejecutar_sent(ev, s->como.para.cuerpo);
            if (ev->error.tuvo_error) break;
            if (ev->control == EJEC_ROMPER) {
                ev->control = EJEC_NORMAL; rompio_t = true; break;
            }
            if (ev->control == EJEC_CONTINUAR) ev->control = EJEC_NORMAL;
            if (ev->control == EJEC_RETORNAR) {
                valor_destruir(&iter); return;
            }
        }
        valor_destruir(&iter);
        if (!rompio_t && !ev->error.tuvo_error && s->como.para.sino != NULL) {
            evaluador_ejecutar_sent(ev, s->como.para.sino);
        }
        return;
    }

    /* Iteración sobre conjunto — produce los elementos en orden de slot. */
    if (iter.tipo == VAL_CONJUNTO) {
        Conjunto *c = iter.como.conjunto;
        bool rompio_c = false;
        for (int i = 0; i < c->capacidad; i++) {
            if (!c->entradas[i].ocupada) continue;
            Valor v = valor_clonar(&c->entradas[i].elemento);
            if (!entorno_definir(ev->entorno_actual,
                                  objetivo->como.ident.nombre,
                                  objetivo->como.ident.longitud, v)) {
                sent_set_error(ev, s, "memoria insuficiente en 'para'");
                break;
            }
            evaluador_ejecutar_sent(ev, s->como.para.cuerpo);
            if (ev->error.tuvo_error) break;
            if (ev->control == EJEC_ROMPER) {
                ev->control = EJEC_NORMAL; rompio_c = true; break;
            }
            if (ev->control == EJEC_CONTINUAR) ev->control = EJEC_NORMAL;
            if (ev->control == EJEC_RETORNAR) {
                valor_destruir(&iter); return;
            }
        }
        valor_destruir(&iter);
        if (!rompio_c && !ev->error.tuvo_error && s->como.para.sino != NULL) {
            evaluador_ejecutar_sent(ev, s->como.para.sino);
        }
        return;
    }

    /* Rama: iteración sobre diccionario — produce las claves (Python).
       v1.20: orden de inserción. */
    if (iter.tipo == VAL_DICCIONARIO) {
        Diccionario *d = iter.como.dicc;
        bool rompio_d = false;
        for (int idx = 0; idx < d->cuenta; idx++) {
            int i = d->orden_insercion[idx];
            Valor v = valor_clonar(&d->entradas[i].clave);
            if (!entorno_definir(ev->entorno_actual,
                                  objetivo->como.ident.nombre,
                                  objetivo->como.ident.longitud, v)) {
                sent_set_error(ev, s, "memoria insuficiente en 'para'");
                break;
            }
            evaluador_ejecutar_sent(ev, s->como.para.cuerpo);
            if (ev->error.tuvo_error) break;
            if (ev->control == EJEC_ROMPER) {
                ev->control = EJEC_NORMAL; rompio_d = true; break;
            }
            if (ev->control == EJEC_CONTINUAR) ev->control = EJEC_NORMAL;
            if (ev->control == EJEC_RETORNAR) {
                valor_destruir(&iter);
                return;
            }
        }
        valor_destruir(&iter);
        if (!rompio_d && !ev->error.tuvo_error && s->como.para.sino != NULL) {
            evaluador_ejecutar_sent(ev, s->como.para.sino);
        }
        return;
    }

    /* Rama: iteración sobre lista. */
    if (iter.tipo == VAL_LISTA) {
        Lista *l = iter.como.lista;
        bool rompio_l = false;
        for (int i = 0; i < l->cuenta; i++) {
            Valor v = valor_clonar(&l->elementos[i]);
            if (!entorno_definir(ev->entorno_actual,
                                  objetivo->como.ident.nombre,
                                  objetivo->como.ident.longitud, v)) {
                sent_set_error(ev, s, "memoria insuficiente en 'para'");
                break;
            }
            evaluador_ejecutar_sent(ev, s->como.para.cuerpo);
            if (ev->error.tuvo_error) break;
            if (ev->control == EJEC_ROMPER) {
                ev->control = EJEC_NORMAL;
                rompio_l = true;
                break;
            }
            if (ev->control == EJEC_CONTINUAR) {
                ev->control = EJEC_NORMAL;
            }
            if (ev->control == EJEC_RETORNAR) {
                valor_destruir(&iter);
                return;
            }
        }
        valor_destruir(&iter);
        if (!rompio_l && !ev->error.tuvo_error && s->como.para.sino != NULL) {
            evaluador_ejecutar_sent(ev, s->como.para.sino);
        }
        return;
    }

    /* Rama: iteración sobre rango (entero por iteración). */
    if (iter.tipo == VAL_RANGO) {
        mp_int actual;
        if (mp_init(&actual) != MP_OKAY) {
            sent_set_error(ev, s, "memoria insuficiente");
            valor_destruir(&iter);
            return;
        }
        if (mp_copy(iter.como.rango.inicio, &actual) != MP_OKAY) {
            mp_clear(&actual);
            sent_set_error(ev, s, "memoria insuficiente");
            valor_destruir(&iter);
            return;
        }
        bool paso_neg = (mp_isneg(iter.como.rango.paso) == MP_YES);
        bool rompio_r = false;

        while (true) {
            int cmp = mp_cmp(&actual, iter.como.rango.fin);
            bool sigue = paso_neg ? (cmp == MP_GT) : (cmp == MP_LT);
            if (!sigue) break;

            mp_int *clon = (mp_int *)malloc(sizeof(mp_int));
            if (!clon || mp_init(clon) != MP_OKAY) {
                free(clon);
                sent_set_error(ev, s, "memoria insuficiente en 'para'");
                break;
            }
            if (mp_copy(&actual, clon) != MP_OKAY) {
                mp_clear(clon); free(clon);
                sent_set_error(ev, s, "memoria insuficiente en 'para'");
                break;
            }
            /* v0.11 (B9): si la iteración cabe en SMALL, demote.
               Útil para `para i en rango(0, 1000)` donde cada i es
               pequeño — evita ~3000 mp_init/mp_clear extra. */
            Valor vi = valor_entero_de_mp_normalizado(clon);
            if (!entorno_definir(ev->entorno_actual,
                                  objetivo->como.ident.nombre,
                                  objetivo->como.ident.longitud, vi)) {
                sent_set_error(ev, s, "memoria insuficiente en 'para'");
                break;
            }

            evaluador_ejecutar_sent(ev, s->como.para.cuerpo);
            if (ev->error.tuvo_error) break;

            if (ev->control == EJEC_ROMPER) {
                ev->control = EJEC_NORMAL;
                rompio_r = true;
                break;
            }
            if (ev->control == EJEC_CONTINUAR) {
                ev->control = EJEC_NORMAL;
            }
            if (ev->control == EJEC_RETORNAR) {
                mp_clear(&actual);
                valor_destruir(&iter);
                return;
            }

            if (mp_add(&actual, iter.como.rango.paso, &actual) != MP_OKAY) {
                sent_set_error(ev, s, "fallo aritmetico en 'para'");
                break;
            }
        }

        mp_clear(&actual);
        valor_destruir(&iter);

        if (!rompio_r && !ev->error.tuvo_error && s->como.para.sino != NULL) {
            evaluador_ejecutar_sent(ev, s->como.para.sino);
        }
        return;
    }

    const char *texto = iter.como.cadena.texto;
    size_t total = (size_t)iter.como.cadena.longitud;
    size_t pos = 0;
    bool rompio = false;

    while (pos < total) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t consumido = utf8proc_iterate(
            (const utf8proc_uint8_t *)(texto + pos),
            (utf8proc_ssize_t)(total - pos), &cp);
        if (consumido <= 0) {
            sent_set_error(ev, s,
                "ErrorDeValor: byte UTF-8 invalido durante iteracion");
            break;
        }

        Valor letra = valor_cadena_duplicar(texto + pos, (int)consumido);
        if (!entorno_definir(ev->entorno_actual,
                              objetivo->como.ident.nombre,
                              objetivo->como.ident.longitud, letra)) {
            sent_set_error(ev, s, "memoria insuficiente en 'para'");
            break;
        }
        pos += (size_t)consumido;

        evaluador_ejecutar_sent(ev, s->como.para.cuerpo);
        if (ev->error.tuvo_error) break;

        if (ev->control == EJEC_ROMPER) {
            ev->control = EJEC_NORMAL;
            rompio = true;
            break;
        }
        if (ev->control == EJEC_CONTINUAR) {
            ev->control = EJEC_NORMAL;
            continue;
        }
        if (ev->control == EJEC_RETORNAR) {
            valor_destruir(&iter);
            return;
        }
    }

    valor_destruir(&iter);

    if (!rompio && !ev->error.tuvo_error && s->como.para.sino != NULL) {
        evaluador_ejecutar_sent(ev, s->como.para.sino);
    }
}

void evaluador_ejecutar_sent(Evaluador *ev, const Sent *s) {
    if (ev->error.tuvo_error) return;
    if (ev->control != EJEC_NORMAL) return;

    switch (s->tipo) {
        case SENT_PASAR:
            return;

        case SENT_EXPR: {
            Valor v = evaluador_evaluar_expr(ev, s->como.expr.expr);
            valor_destruir(&v);
            return;
        }

        case SENT_ASIGNAR:      ejec_asignar(ev, s);      return;
        case SENT_ASIGNAR_AUG:  ejec_asignar_aug(ev, s);  return;

        case SENT_ROMPER:
            ev->control = EJEC_ROMPER;
            return;
        case SENT_CONTINUAR:
            ev->control = EJEC_CONTINUAR;
            return;

        case SENT_SI:        ejec_si(ev, s);        return;
        case SENT_MIENTRAS:  ejec_mientras(ev, s);  return;
        case SENT_PARA:      ejec_para(ev, s);      return;
        case SENT_BLOQUE:    ejec_bloque(ev, s);    return;

        case SENT_FUNCION: {
            /* Crear un VAL_FUNCION que referencia el nodo SENT_FUNCION
             * del AST + el entorno actual de definición. Sin closures
             * (decisión B2): el entorno_definicion se usará en el
             * scope chain pero no captura variables locales. */
            /* v1.72: decoradores. El motor tree-walking esta congelado
             * en v0.5 (ADR B2) — clases/importar/intentar/etc. ya emiten
             * error claro. Decoradores siguen la misma politica para
             * evitar silent-ignore (donde el usuario veria su funcion
             * ejecutar SIN el decorador aplicado). */
            if (s->como.funcion.n_decoradores > 0) {
                sent_set_error(ev, s,
                    "decoradores '@...' requieren --bytecode (motor tree-walking congelado en v0.5)");
                return;
            }
            Valor fn = valor_funcion(s, ev->entorno_actual);
            if (!entorno_definir(ev->entorno_actual,
                                  s->como.funcion.nombre,
                                  s->como.funcion.longitud_nombre, fn)) {
                sent_set_error(ev, s, "memoria insuficiente al definir funcion");
            }
            return;
        }

        case SENT_RETORNAR: {
            Valor v = valor_nulo();
            if (s->como.retornar.valor != NULL) {
                v = evaluador_evaluar_expr(ev, s->como.retornar.valor);
                if (ev->error.tuvo_error) { valor_destruir(&v); return; }
            }
            valor_destruir(&ev->valor_retorno);
            ev->valor_retorno = v;
            ev->control = EJEC_RETORNAR;
            return;
        }

        case SENT_CLASE:
        case SENT_INTENTAR:
        case SENT_LANZAR:
        case SENT_IMPORTAR:
        case SENT_DESDE_IMPORTAR:
        case SENT_GLOBAL:
        case SENT_NOLOCAL:
        case SENT_COINCIDIR:
        case SENT_BORRAR:
            sent_set_error(ev, s,
                "esta forma de sentencia no esta implementada en v0.4");
            return;
    }
    sent_set_error(ev, s, "tipo de sentencia desconocido");
}

void evaluador_ejecutar_programa(Evaluador *ev, Sent **sentencias, int n) {
    for (int i = 0; i < n; i++) {
        evaluador_ejecutar_sent(ev, sentencias[i]);
        if (ev->error.tuvo_error) return;
        if (ev->control != EJEC_NORMAL) {
            sent_set_error(ev, sentencias[i],
                "control de flujo (romper/continuar/retornar) fuera de su contexto");
            return;
        }
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Llamadas a función
 * ────────────────────────────────────────────────────────────────── */

/*
 * Llama a una función definida por el usuario. Crea un nuevo entorno
 * hijo de `entorno_def` (sin closures: típicamente el global), liga
 * los parámetros, ejecuta el cuerpo y recoge el valor de retorno si
 * apareció `SENT_RETORNAR`. Restaura el entorno previo al volver.
 */
static Valor llamar_usuario(Evaluador *ev, const Sent *def,
                            Entorno *entorno_def,
                            Valor *args, int n_args, const Expr *call_site) {
    int n_params = def->como.funcion.n_parametros;
    Parametro *params = def->como.funcion.parametros;

    int min_req = 0;
    for (int i = 0; i < n_params; i++) {
        if (params[i].valor_defecto == NULL) min_req++;
    }
    if (n_args < min_req || n_args > n_params) {
        if (min_req == n_params) {
            return error_en(ev, call_site,
                "ErrorDeTipo: %.*s() esperaba %d argumentos, recibio %d",
                def->como.funcion.longitud_nombre,
                def->como.funcion.nombre,
                n_params, n_args);
        }
        return error_en(ev, call_site,
            "ErrorDeTipo: %.*s() esperaba entre %d y %d argumentos, recibio %d",
            def->como.funcion.longitud_nombre,
            def->como.funcion.nombre,
            min_req, n_params, n_args);
    }

    Entorno local;
    entorno_iniciar(&local, entorno_def);

    for (int i = 0; i < n_params; i++) {
        Valor v;
        if (i < n_args) {
            v = valor_clonar(&args[i]);
        } else {
            v = evaluador_evaluar_expr(ev, params[i].valor_defecto);
            if (ev->error.tuvo_error) {
                valor_destruir(&v);
                entorno_destruir(&local);
                return valor_nulo();
            }
        }
        if (!entorno_definir(&local, params[i].nombre,
                              params[i].longitud_nombre, v)) {
            entorno_destruir(&local);
            return error_en(ev, call_site, "memoria insuficiente");
        }
    }

    Entorno *guardar_env = ev->entorno_actual;
    ev->entorno_actual = &local;

    evaluador_ejecutar_sent(ev, def->como.funcion.cuerpo);

    ev->entorno_actual = guardar_env;

    Valor resultado = valor_nulo();
    if (!ev->error.tuvo_error) {
        if (ev->control == EJEC_RETORNAR) {
            resultado = ev->valor_retorno;
            ev->valor_retorno = valor_nulo();   /* transferimos ownership */
            ev->control = EJEC_NORMAL;
        }
        /* Función sin `retornar` explícito → resultado nulo. */
    }

    entorno_destruir(&local);
    return resultado;
}

static Valor eval_llamada(Evaluador *ev, const Expr *e) {
    Valor callee = evaluador_evaluar_expr(ev, e->como.llamada.callee);
    if (ev->error.tuvo_error) {
        valor_destruir(&callee);
        return valor_nulo();
    }

    int n = e->como.llamada.n_args;
    Valor *args = NULL;
    if (n > 0) {
        args = (Valor *)malloc(sizeof(Valor) * (size_t)n);
        if (!args) {
            valor_destruir(&callee);
            return error_en(ev, e, "memoria insuficiente");
        }
    }

    int evaluados = 0;
    for (int i = 0; i < n; i++) {
        args[i] = evaluador_evaluar_expr(ev, e->como.llamada.args[i]);
        evaluados = i + 1;
        if (ev->error.tuvo_error) {
            for (int j = 0; j < evaluados; j++) valor_destruir(&args[j]);
            free(args);
            valor_destruir(&callee);
            return valor_nulo();
        }
    }

    Valor resultado;
    if (callee.tipo == VAL_NATIVA) {
        resultado = callee.como.nativa.fn(&ev->error, n, args,
                                            e->linea, e->columna);
    } else if (callee.tipo == VAL_FUNCION) {
        resultado = llamar_usuario(ev, callee.como.funcion.def,
                                    callee.como.funcion.entorno_definicion,
                                    args, n, e);
    } else {
        resultado = error_en(ev, e,
            "ErrorDeTipo: '%s' no es invocable",
            valor_nombre_tipo(&callee));
    }

    for (int i = 0; i < n; i++) valor_destruir(&args[i]);
    free(args);
    valor_destruir(&callee);
    return resultado;
}

/* ──────────────────────────────────────────────────────────────────
 * Listas: construcción e indexación
 * ────────────────────────────────────────────────────────────────── */

static Valor eval_lista(Evaluador *ev, const Expr *e) {
    int n = e->como.secuencia.n_elementos;
    Lista *l = lista_nueva(n);
    if (!l) return error_en(ev, e, "memoria insuficiente");

    for (int i = 0; i < n; i++) {
        Valor v = evaluador_evaluar_expr(ev, e->como.secuencia.elementos[i]);
        if (ev->error.tuvo_error) {
            valor_destruir(&v);
            lista_liberar(l);
            return valor_nulo();
        }
        if (!lista_agregar(l, v)) {
            lista_liberar(l);
            return error_en(ev, e, "memoria insuficiente");
        }
    }
    return valor_lista(l);
}

/*
 * Convierte un Valor entero/booleano en `long` con bounds-check
 * adecuado para usarlo como índice. Devuelve true si OK; en caso de
 * overflow del rango entero, devuelve false (el llamador reporta).
 */
static bool indice_a_long(const Valor *v, long *out) {
    if (v->tipo == VAL_BOOLEANO) { *out = v->como.booleano ? 1 : 0; return true; }
    int64_t i64;
    if (!valor_entero_a_i64(v, &i64)) return false;
    long l = (long)i64;
    if ((int64_t)l != i64) return false;  /* no cabe en long */
    *out = l;
    return true;
}

static Valor eval_indice(Evaluador *ev, const Expr *e) {
    Valor obj = evaluador_evaluar_expr(ev, e->como.indice.objeto);
    if (ev->error.tuvo_error) { valor_destruir(&obj); return valor_nulo(); }
    Valor idx = evaluador_evaluar_expr(ev, e->como.indice.indice);
    if (ev->error.tuvo_error) {
        valor_destruir(&obj); valor_destruir(&idx);
        return valor_nulo();
    }

    if (obj.tipo == VAL_LISTA) {
        if (!valor_es_entero(&idx) && idx.tipo != VAL_BOOLEANO) {
            Valor err = error_en(ev, e,
                "ErrorDeTipo: indice de lista debe ser entero, no '%s'",
                valor_nombre_tipo(&idx));
            valor_destruir(&obj); valor_destruir(&idx);
            return err;
        }
        long i;
        if (!indice_a_long(&idx, &i)) {
            valor_destruir(&obj); valor_destruir(&idx);
            return error_en(ev, e,
                "ErrorDeIndice: indice fuera de rango para una lista");
        }
        Lista *l = obj.como.lista;
        if (i < 0) i += l->cuenta;     /* negativo cuenta desde el final */
        if (i < 0 || i >= l->cuenta) {
            valor_destruir(&obj); valor_destruir(&idx);
            return error_en(ev, e,
                "ErrorDeIndice: indice %ld fuera de rango (lista de %d)",
                i, l->cuenta);
        }
        Valor resultado = valor_clonar(&l->elementos[i]);
        valor_destruir(&obj); valor_destruir(&idx);
        return resultado;
    }

    if (obj.tipo == VAL_DICCIONARIO) {
        if (!valor_es_hashable(&idx)) {
            Valor err = error_en(ev, e,
                "ErrorDeTipo: '%s' no se puede usar como clave de diccionario",
                valor_nombre_tipo(&idx));
            valor_destruir(&obj); valor_destruir(&idx);
            return err;
        }
        Valor resultado;
        if (!dicc_obtener(obj.como.dicc, &idx, &resultado)) {
            char buf[128];
            valor_a_repr(&idx, buf, sizeof(buf));
            Valor err = error_en(ev, e,
                "ErrorDeClave: %s", buf);
            valor_destruir(&obj); valor_destruir(&idx);
            return err;
        }
        valor_destruir(&obj); valor_destruir(&idx);
        return resultado;
    }

    if (obj.tipo == VAL_TUPLA) {
        if (!valor_es_entero(&idx) && idx.tipo != VAL_BOOLEANO) {
            Valor err = error_en(ev, e,
                "ErrorDeTipo: indice de tupla debe ser entero, no '%s'",
                valor_nombre_tipo(&idx));
            valor_destruir(&obj); valor_destruir(&idx);
            return err;
        }
        long i;
        if (!indice_a_long(&idx, &i)) {
            valor_destruir(&obj); valor_destruir(&idx);
            return error_en(ev, e,
                "ErrorDeIndice: indice fuera de rango para una tupla");
        }
        Tupla *t = obj.como.tupla;
        if (i < 0) i += t->cuenta;
        if (i < 0 || i >= t->cuenta) {
            valor_destruir(&obj); valor_destruir(&idx);
            return error_en(ev, e,
                "ErrorDeIndice: indice %ld fuera de rango (tupla de %d)",
                i, t->cuenta);
        }
        Valor resultado = valor_clonar(&t->elementos[i]);
        valor_destruir(&obj); valor_destruir(&idx);
        return resultado;
    }

    Valor err = error_en(ev, e,
        "ErrorDeTipo: '%s' no es indexable en v0.5",
        valor_nombre_tipo(&obj));
    valor_destruir(&obj); valor_destruir(&idx);
    return err;
}

/*
 * Slicing `objeto[a:b:c]` con semántica Python:
 *   - Cualquier campo puede omitirse y se aplica el default.
 *   - Defaults dependen del signo de paso:
 *       paso > 0:  inicio=0,        fin=cuenta
 *       paso < 0:  inicio=cuenta-1, fin=-1 (uno antes de 0)
 *   - paso == 0 → error.
 *   - Índices negativos cuentan desde el final.
 *   - Índices fuera de rango se clamp-ean (NO da error, igual que Py).
 */
static Valor eval_rebanada(Evaluador *ev, const Expr *e) {
    Valor obj = evaluador_evaluar_expr(ev, e->como.rebanada.objeto);
    if (ev->error.tuvo_error) { valor_destruir(&obj); return valor_nulo(); }

    if (obj.tipo != VAL_LISTA) {
        Valor err = error_en(ev, e,
            "ErrorDeTipo: '%s' no soporta slicing en v0.5",
            valor_nombre_tipo(&obj));
        valor_destruir(&obj);
        return err;
    }

    long paso = 1;
    if (e->como.rebanada.paso != NULL) {
        Valor pv = evaluador_evaluar_expr(ev, e->como.rebanada.paso);
        if (ev->error.tuvo_error) {
            valor_destruir(&pv); valor_destruir(&obj); return valor_nulo();
        }
        if (!valor_es_entero(&pv) && pv.tipo != VAL_BOOLEANO) {
            Valor err = error_en(ev, e,
                "ErrorDeTipo: paso de rebanada debe ser entero, no '%s'",
                valor_nombre_tipo(&pv));
            valor_destruir(&pv); valor_destruir(&obj); return err;
        }
        if (pv.tipo == VAL_BOOLEANO) {
            paso = pv.como.booleano ? 1 : 0;
        } else {
            int64_t i64;
            if (!valor_entero_a_i64(&pv, &i64)) {
                valor_destruir(&pv); valor_destruir(&obj);
                return error_en(ev, e,
                    "ErrorDeValor: paso de rebanada demasiado grande");
            }
            paso = (long)i64;
        }
        valor_destruir(&pv);
        if (paso == 0) {
            valor_destruir(&obj);
            return error_en(ev, e,
                "ErrorDeValor: el paso de una rebanada no puede ser 0");
        }
    }

    int total = obj.como.lista->cuenta;

    /* Helper para resolver inicio/fin opcionales con defaults dependientes
       del signo de paso. Para slicing usamos el rango [-cuenta, cuenta]
       sin error de bounds: Python clamp-ea silenciosamente. */
    long inicio, fin;
    if (e->como.rebanada.inicio != NULL) {
        Valor iv = evaluador_evaluar_expr(ev, e->como.rebanada.inicio);
        if (ev->error.tuvo_error) {
            valor_destruir(&iv); valor_destruir(&obj); return valor_nulo();
        }
        if (iv.tipo == VAL_BOOLEANO) inicio = iv.como.booleano ? 1 : 0;
        else {
            int64_t i64;
            if (!valor_entero_a_i64(&iv, &i64)) {
                valor_destruir(&iv); valor_destruir(&obj);
                return error_en(ev, e,
                    "ErrorDeTipo: inicio de rebanada debe ser entero");
            }
            inicio = (long)i64;
        }
        valor_destruir(&iv);
        if (inicio < 0) inicio += total;
    } else {
        inicio = (paso > 0) ? 0 : total - 1;
    }

    if (e->como.rebanada.fin != NULL) {
        Valor fv = evaluador_evaluar_expr(ev, e->como.rebanada.fin);
        if (ev->error.tuvo_error) {
            valor_destruir(&fv); valor_destruir(&obj); return valor_nulo();
        }
        if (fv.tipo == VAL_BOOLEANO) fin = fv.como.booleano ? 1 : 0;
        else {
            int64_t i64;
            if (!valor_entero_a_i64(&fv, &i64)) {
                valor_destruir(&fv); valor_destruir(&obj);
                return error_en(ev, e,
                    "ErrorDeTipo: fin de rebanada debe ser entero");
            }
            fin = (long)i64;
        }
        valor_destruir(&fv);
        if (fin < 0) fin += total;
    } else {
        fin = (paso > 0) ? total : -1;
    }

    /* Clamping al rango válido (no error). */
    if (paso > 0) {
        if (inicio < 0) inicio = 0;
        if (inicio > total) inicio = total;
        if (fin < 0) fin = 0;
        if (fin > total) fin = total;
    } else {
        if (inicio < 0) inicio = -1;
        if (inicio >= total) inicio = total - 1;
        if (fin < -1) fin = -1;
        if (fin >= total) fin = total - 1;
    }

    /* Construir lista resultante. */
    Lista *resultado = lista_nueva(0);
    if (!resultado) {
        valor_destruir(&obj);
        return error_en(ev, e, "memoria insuficiente");
    }
    Lista *src = obj.como.lista;
    if (paso > 0) {
        for (long i = inicio; i < fin; i += paso) {
            lista_agregar(resultado, valor_clonar(&src->elementos[i]));
        }
    } else {
        for (long i = inicio; i > fin; i += paso) {
            lista_agregar(resultado, valor_clonar(&src->elementos[i]));
        }
    }
    valor_destruir(&obj);
    return valor_lista(resultado);
}

static Valor eval_diccionario(Evaluador *ev, const Expr *e) {
    Diccionario *d = dicc_nuevo();
    if (!d) return error_en(ev, e, "memoria insuficiente");

    int n = e->como.diccionario.n_pares;
    for (int i = 0; i < n; i++) {
        Valor k = evaluador_evaluar_expr(ev, e->como.diccionario.claves[i]);
        if (ev->error.tuvo_error) {
            valor_destruir(&k);
            dicc_liberar(d);
            return valor_nulo();
        }
        if (!valor_es_hashable(&k)) {
            Valor err = error_en(ev, e,
                "ErrorDeTipo: '%s' no se puede usar como clave de diccionario",
                valor_nombre_tipo(&k));
            valor_destruir(&k);
            dicc_liberar(d);
            return err;
        }
        Valor v = evaluador_evaluar_expr(ev, e->como.diccionario.valores[i]);
        if (ev->error.tuvo_error) {
            valor_destruir(&k); valor_destruir(&v);
            dicc_liberar(d);
            return valor_nulo();
        }
        if (!dicc_asignar(d, k, v)) {
            dicc_liberar(d);
            return error_en(ev, e, "memoria insuficiente al construir diccionario");
        }
    }
    return valor_diccionario(d);
}

static Valor eval_conjunto(Evaluador *ev, const Expr *e) {
    Conjunto *c = conj_nuevo();
    if (!c) return error_en(ev, e, "memoria insuficiente");

    int n = e->como.secuencia.n_elementos;
    for (int i = 0; i < n; i++) {
        Valor v = evaluador_evaluar_expr(ev, e->como.secuencia.elementos[i]);
        if (ev->error.tuvo_error) {
            valor_destruir(&v);
            conj_liberar(c);
            return valor_nulo();
        }
        if (!valor_es_hashable(&v)) {
            Valor err = error_en(ev, e,
                "ErrorDeTipo: '%s' no se puede usar como elemento de conjunto",
                valor_nombre_tipo(&v));
            valor_destruir(&v);
            conj_liberar(c);
            return err;
        }
        if (!conj_agregar(c, v)) {
            conj_liberar(c);
            return error_en(ev, e, "memoria insuficiente al construir conjunto");
        }
    }
    return valor_conjunto(c);
}

static Valor eval_tupla(Evaluador *ev, const Expr *e) {
    int n = e->como.secuencia.n_elementos;
    Tupla *t = tupla_nueva(n);
    if (!t) return error_en(ev, e, "memoria insuficiente");

    for (int i = 0; i < n; i++) {
        Valor v = evaluador_evaluar_expr(ev, e->como.secuencia.elementos[i]);
        if (ev->error.tuvo_error) {
            /* Limpiar elementos parcialmente inicializados. */
            for (int k = 0; k < i; k++) valor_destruir(&t->elementos[k]);
            free(t->elementos);
            free(t);
            valor_destruir(&v);
            return valor_nulo();
        }
        t->elementos[i] = v;
    }
    return valor_tupla(t);
}
