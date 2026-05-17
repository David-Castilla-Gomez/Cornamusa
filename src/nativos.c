#include "nativos.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "evaluador.h"
#include "memoria.h"
#include "tommath.h"
#include "utf8proc.h"
#include "valor.h"

/*
 * Helper: pone un error en el evaluador desde un built-in. Las
 * built-ins no tienen un Expr* sino sólo línea/columna del call-site.
 */
static Valor error_nativa(EvalError *err, int linea, int columna,
                          const char *fmt, ...) {
    if (err->tuvo_error) return valor_nulo();
    err->tuvo_error = true;
    err->linea = linea;
    err->columna = columna;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err->mensaje, sizeof(err->mensaje), fmt, ap);
    va_end(ap);
    return valor_nulo();
}

/* ──────────────────────────────────────────────────────────────────
 * imprimir(*args)
 *
 * Variádica. Imprime cada arg separado por un espacio, seguido de '\n'.
 * Sin kwargs (no hay sintaxis de kwargs en Cornamusa todavía); se podrá
 * extender cuando lleguen.
 *
 * Devuelve nulo.
 * ────────────────────────────────────────────────────────────────── */

static Valor nativa_imprimir(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    (void)err; (void)linea; (void)columna;
    char buffer[1024];
    for (int i = 0; i < n_args; i++) {
        if (i > 0) fputc(' ', stdout);
        valor_a_cadena(&args[i], buffer, sizeof(buffer));
        fputs(buffer, stdout);
    }
    fputc('\n', stdout);
    fflush(stdout);
    return valor_nulo();
}

/* ──────────────────────────────────────────────────────────────────
 * longitud(x)
 *
 * Para cadena: número de code points UTF-8.
 * Para rango: número de elementos producidos por iteración.
 * Para otros: ErrorDeTipo.
 * ────────────────────────────────────────────────────────────────── */

Valor nativos_calcular_longitud(EvalError *err, const Valor *v,
                                  int linea, int columna) {
    if (v->tipo == VAL_CADENA) {
        long n = 0;
        size_t pos = 0;
        size_t total = (size_t)v->como.cadena.longitud;
        while (pos < total) {
            utf8proc_int32_t cp;
            utf8proc_ssize_t consumido = utf8proc_iterate(
                (const utf8proc_uint8_t *)(v->como.cadena.texto + pos),
                (utf8proc_ssize_t)(total - pos), &cp);
            if (consumido <= 0) {
                return error_nativa(err, linea, columna,
                    "ErrorDeValor: cadena con UTF-8 invalido");
            }
            n++;
            pos += (size_t)consumido;
        }
        return valor_entero_de_long(n);
    }
    if (v->tipo == VAL_LISTA) {
        return valor_entero_de_long((long)v->como.lista->cuenta);
    }
    if (v->tipo == VAL_DICCIONARIO) {
        return valor_entero_de_long((long)v->como.dicc->cuenta);
    }
    if (v->tipo == VAL_CONJUNTO) {
        return valor_entero_de_long((long)v->como.conjunto->cuenta);
    }
    if (v->tipo == VAL_TUPLA) {
        return valor_entero_de_long((long)v->como.tupla->cuenta);
    }
    if (v->tipo == VAL_RANGO) {
        /* count = max(0, ceil((fin - inicio) / paso)) */
        mp_int diff;
        if (mp_init(&diff) != MP_OKAY) return valor_nulo();
        if (mp_sub(v->como.rango.fin, v->como.rango.inicio, &diff) != MP_OKAY) {
            mp_clear(&diff);
            return valor_nulo();
        }
        bool paso_neg = (mp_isneg(v->como.rango.paso) == MP_YES);
        bool diff_neg = (mp_isneg(&diff) == MP_YES);
        if ((!paso_neg && diff_neg) || (paso_neg && !diff_neg)
            || mp_iszero(&diff) == MP_YES) {
            mp_clear(&diff);
            return valor_entero_de_long(0);
        }
        /* Trabajar con magnitudes para evitar el signo. */
        mp_int mag_diff, mag_paso, q, r;
        if (mp_init_multi(&mag_diff, &mag_paso, &q, &r, NULL) != MP_OKAY) {
            mp_clear(&diff);
            return valor_nulo();
        }
        if (mp_abs(&diff, &mag_diff) != MP_OKAY
         || mp_abs(v->como.rango.paso, &mag_paso) != MP_OKAY
         || mp_div(&mag_diff, &mag_paso, &q, &r) != MP_OKAY) {
            mp_clear_multi(&mag_diff, &mag_paso, &q, &r, &diff, NULL);
            return valor_nulo();
        }
        if (mp_iszero(&r) == MP_NO) {
            if (mp_add_d(&q, 1, &q) != MP_OKAY) {
                mp_clear_multi(&mag_diff, &mag_paso, &q, &r, &diff, NULL);
                return valor_nulo();
            }
        }
        mp_int *resultado = (mp_int *)malloc(sizeof(mp_int));
        if (!resultado || mp_init(resultado) != MP_OKAY
                       || mp_copy(&q, resultado) != MP_OKAY) {
            free(resultado);
            mp_clear_multi(&mag_diff, &mag_paso, &q, &r, &diff, NULL);
            return valor_nulo();
        }
        mp_clear_multi(&mag_diff, &mag_paso, &q, &r, &diff, NULL);
        /* v0.11 (B9): longitud(rango) suele ser pequeña — demote. */
        return valor_entero_de_mp_normalizado(resultado);
    }
    return error_nativa(err, linea, columna,
        "ErrorDeTipo: longitud() no soporta '%s'", valor_nombre_tipo(v));
}

static Valor nativa_longitud(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: longitud() requiere 1 argumento, recibio %d", n_args);
    }
    return nativos_calcular_longitud(err, &args[0], linea, columna);
}

/* ──────────────────────────────────────────────────────────────────
 * tipo(x)
 *
 * Devuelve cadena con el nombre del tipo del valor, en castellano.
 * ────────────────────────────────────────────────────────────────── */

static Valor nativa_tipo(EvalError *err, int n_args, Valor *args,
                         int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tipo() requiere 1 argumento, recibio %d", n_args);
    }
    const char *nombre = valor_nombre_tipo(&args[0]);
    return valor_cadena_duplicar(nombre, (int)strlen(nombre));
}

/* ──────────────────────────────────────────────────────────────────
 * rango([inicio,] fin [, paso])
 *
 * Construye un VAL_RANGO. Tres formas:
 *   rango(n)         → 0..n-1, paso 1
 *   rango(a, b)      → a..b-1, paso 1
 *   rango(a, b, p)   → a, a+p, a+2p, ... mientras (p>0 ? <b : >b)
 *
 * paso == 0 produce ErrorDeValor. Argumentos no enteros producen
 * ErrorDeTipo.
 * ────────────────────────────────────────────────────────────────── */

static bool extraer_entero(const Valor *v, mp_int *out) {
    if (v->tipo == VAL_ENTERO) {
        return mp_copy(v->como.entero, out) == MP_OKAY;
    }
    if (v->tipo == VAL_ENTERO_SMALL) {
        mp_set_i64(out, v->como.entero_small);
        return true;
    }
    if (v->tipo == VAL_BOOLEANO) {
        mp_set_l(out, v->como.booleano ? 1 : 0);
        return true;
    }
    return false;
}

static Valor nativa_rango(EvalError *err, int n_args, Valor *args,
                          int linea, int columna) {
    if (n_args < 1 || n_args > 3) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: rango() acepta 1, 2 o 3 argumentos, recibio %d",
            n_args);
    }
    for (int i = 0; i < n_args; i++) {
        if (!valor_es_entero(&args[i]) && args[i].tipo != VAL_BOOLEANO) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: rango() solo acepta enteros, recibio '%s'",
                valor_nombre_tipo(&args[i]));
        }
    }

    mp_int *mi = (mp_int *)malloc(sizeof(mp_int));
    mp_int *mf = (mp_int *)malloc(sizeof(mp_int));
    mp_int *mp = (mp_int *)malloc(sizeof(mp_int));
    if (!mi || !mf || !mp) {
        free(mi); free(mf); free(mp);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    if (mp_init_multi(mi, mf, mp, NULL) != MP_OKAY) {
        free(mi); free(mf); free(mp);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }

    if (n_args == 1) {
        mp_set_l(mi, 0);
        if (!extraer_entero(&args[0], mf)) goto fail;
        mp_set_l(mp, 1);
    } else {
        if (!extraer_entero(&args[0], mi)) goto fail;
        if (!extraer_entero(&args[1], mf)) goto fail;
        if (n_args == 3) {
            if (!extraer_entero(&args[2], mp)) goto fail;
            if (mp_iszero(mp) == MP_YES) {
                mp_clear(mi); free(mi);
                mp_clear(mf); free(mf);
                mp_clear(mp); free(mp);
                return error_nativa(err, linea, columna,
                    "ErrorDeValor: rango() no admite paso 0");
            }
        } else {
            mp_set_l(mp, 1);
        }
    }
    return valor_rango_de_mp(mi, mf, mp);

fail:
    mp_clear(mi); free(mi);
    mp_clear(mf); free(mf);
    mp_clear(mp); free(mp);
    return error_nativa(err, linea, columna, "memoria insuficiente");
}

/* ──────────────────────────────────────────────────────────────────
 * Conversores explícitos (v1.1).
 *
 * `cadena(x)`, `entero(x)`, `decimal(x)`, `booleano(x)`. Permiten
 * coerción explícita entre tipos sin depender de operadores
 * implícitos. Útiles en programas que mezclan I/O (donde todo entra
 * como cadena) con aritmética.
 * ────────────────────────────────────────────────────────────────── */

/*
 * Trim de espacios ASCII al inicio y al final. No muta `*texto` (es
 * const) — devuelve nuevos `inicio`/`longitud` apuntando al sub-rango
 * limpio. Usado para parseo numérico desde cadenas.
 */
static void trim_ascii(const char *texto, int longitud,
                        const char **inicio_out, int *longitud_out) {
    int i = 0;
    while (i < longitud && (texto[i] == ' ' || texto[i] == '\t'
                             || texto[i] == '\n' || texto[i] == '\r')) i++;
    int j = longitud;
    while (j > i && (texto[j-1] == ' ' || texto[j-1] == '\t'
                      || texto[j-1] == '\n' || texto[j-1] == '\r')) j--;
    *inicio_out = texto + i;
    *longitud_out = j - i;
}

/*
 * cadena(x) — devuelve una cadena con la representación tipo `imprimir`.
 *
 * Para enteros bignum se usa `mp_radix_size` + alocación dinámica para
 * preservar todos los dígitos. El resto de tipos cabe holgadamente en
 * un buffer de 4096 bytes (decimales, listas pequeñas, etc.). Para
 * tipos compuestos muy grandes la cadena puede truncarse, pero no es
 * un escenario habitual.
 */
static Valor nativa_cadena(EvalError *err, int n_args, Valor *args,
                            int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena() requiere 1 argumento, recibio %d", n_args);
    }
    /* Delegamos en `valor_a_cadena_alocada` que dimensiona el buffer
       dinámicamente: para bignum exacto vía `mp_radix_size`, para
       cadena copia profunda directa, y para colecciones escala el
       buffer hasta 16 MB. Convertimos su VAL_NULO de OOM en un
       `error_nativa` con posición. */
    Valor r = valor_a_cadena_alocada(&args[0]);
    if (r.tipo == VAL_NULO && args[0].tipo != VAL_NULO) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    return r;
}

/*
 * entero(x) — convierte a entero.
 *
 *   entero(int)         → no-op (clon).
 *   entero(decimal)     → truncar hacia cero. Falla si NaN/Inf o fuera
 *                         del rango de int64 (caso raro).
 *   entero(booleano)    → 0 o 1.
 *   entero("123")       → parse base 10 (acepta signo). ErrorDeValor si
 *                         no es un literal entero válido.
 *   entero(otro)        → ErrorDeTipo.
 */
static Valor nativa_entero(EvalError *err, int n_args, Valor *args,
                            int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: entero() requiere 1 argumento, recibio %d", n_args);
    }
    const Valor *v = &args[0];
    if (valor_es_entero(v)) {
        Valor r = valor_clonar(v);
        if (r.tipo == VAL_NULO && v->tipo != VAL_NULO) {
            /* `valor_clonar` devuelve VAL_NULO ante OOM al copiar mp_int.
               Como v sí era entero, esto es un fallo de memoria, no un
               valor genuinamente nulo. */
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        return r;
    }
    if (v->tipo == VAL_BOOLEANO) {
        return valor_entero_de_i64(v->como.booleano ? 1 : 0);
    }
    if (v->tipo == VAL_DECIMAL) {
        double d = v->como.decimal;
        if (d != d) {  /* NaN */
            return error_nativa(err, linea, columna,
                "ErrorDeValor: no se puede convertir NaN a entero");
        }
        /* Rango de int64 expresado como double. INT64_MAX (2^63 - 1) NO
           es exactamente representable en double — el redondeo a nearest
           lo lleva a 2^63 = 9.2233720368547758e18 (que YA está fuera de
           int64). Por eso el límite superior aceptado es 2^63 - 1024 ≈
           9.2233720368547748e18. INT64_MIN sí es exacto en double:
           -2^63 = -9.2233720368547758e18. */
        if (d > 9.2233720368547748e18 || d < -9.2233720368547758e18) {
            return error_nativa(err, linea, columna,
                "ErrorDeValor: decimal fuera del rango de entero (truncado)");
        }
        int64_t n = (int64_t)d;  /* C99: truncado hacia cero. */
        return valor_entero_de_i64(n);
    }
    if (v->tipo == VAL_CADENA) {
        const char *txt; int len;
        trim_ascii(v->como.cadena.texto, v->como.cadena.longitud, &txt, &len);
        if (len == 0) {
            return error_nativa(err, linea, columna,
                "ErrorDeValor: cadena vacia no es entero valido");
        }
        bool negativo = false;
        if (txt[0] == '+' || txt[0] == '-') {
            negativo = (txt[0] == '-');
            txt++; len--;
        }
        if (len == 0) {
            return error_nativa(err, linea, columna,
                "ErrorDeValor: '%.*s' no es entero valido",
                v->como.cadena.longitud, v->como.cadena.texto);
        }
        /* Validar que solo hay dígitos y guiones bajos (mismo subconjunto
           que un literal Cornamusa sin prefijo). */
        for (int i = 0; i < len; i++) {
            char c = txt[i];
            if (!((c >= '0' && c <= '9') || c == '_')) {
                return error_nativa(err, linea, columna,
                    "ErrorDeValor: '%.*s' no es entero valido",
                    v->como.cadena.longitud, v->como.cadena.texto);
            }
        }
        Valor r = valor_entero_de_lexema(txt, len);
        if (r.tipo == VAL_NULO) {
            return error_nativa(err, linea, columna,
                "ErrorDeValor: '%.*s' no es entero valido",
                v->como.cadena.longitud, v->como.cadena.texto);
        }
        if (negativo) {
            /* Negar in-place. SMALL: int64 directo (no overflow porque
               SMALL_INT_MIN = -SMALL_INT_MAX - 1, y los valores que vienen
               de un parse positivo no llegan a SMALL_INT_MAX exacto sin
               quedar dentro). BIG: mp_neg in place, y normalizar. */
            if (r.tipo == VAL_ENTERO_SMALL) {
                r.como.entero_small = -r.como.entero_small;
            } else {
                /* mp_neg: documentación libtommath dice que destino
                   puede ser igual a fuente. */
                if (mp_neg(r.como.entero, r.como.entero) != MP_OKAY) {
                    valor_destruir(&r);
                    return error_nativa(err, linea, columna,
                        "memoria insuficiente");
                }
            }
        }
        return r;
    }
    return error_nativa(err, linea, columna,
        "ErrorDeTipo: entero() no acepta '%s'", valor_nombre_tipo(v));
}

/*
 * decimal(x) — convierte a decimal.
 *
 *   decimal(decimal)    → no-op.
 *   decimal(int)        → conversión exacta (puede perder precisión si
 *                         el bignum supera 2^53).
 *   decimal(booleano)   → 0.0 o 1.0.
 *   decimal("3.14")     → strtod. ErrorDeValor si no parsea limpio.
 *   decimal(otro)       → ErrorDeTipo.
 */
static Valor nativa_decimal(EvalError *err, int n_args, Valor *args,
                             int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: decimal() requiere 1 argumento, recibio %d", n_args);
    }
    const Valor *v = &args[0];
    if (v->tipo == VAL_DECIMAL) return *v;
    if (v->tipo == VAL_BOOLEANO) {
        return valor_decimal(v->como.booleano ? 1.0 : 0.0);
    }
    if (v->tipo == VAL_ENTERO_SMALL) {
        return valor_decimal((double)v->como.entero_small);
    }
    if (v->tipo == VAL_ENTERO) {
        return valor_decimal(mp_get_double(v->como.entero));
    }
    if (v->tipo == VAL_CADENA) {
        const char *txt; int len;
        trim_ascii(v->como.cadena.texto, v->como.cadena.longitud, &txt, &len);
        if (len == 0) {
            return error_nativa(err, linea, columna,
                "ErrorDeValor: cadena vacia no es decimal valido");
        }
        /* Necesitamos null-terminated para strtod; el buffer fuente
           no lo está. Stack para casos pequeños, malloc si no. */
        char stack_buf[64];
        char *buf = stack_buf;
        char *heap = NULL;
        if (len + 1 > (int)sizeof(stack_buf)) {
            heap = (char *)malloc((size_t)len + 1);
            if (!heap) return error_nativa(err, linea, columna,
                "memoria insuficiente");
            buf = heap;
        }
        memcpy(buf, txt, (size_t)len);
        buf[len] = '\0';
        char *end = NULL;
        double d = strtod(buf, &end);
        bool ok = (end != NULL) && (*end == '\0') && (end != buf);
        if (heap) free(heap);
        if (!ok) {
            return error_nativa(err, linea, columna,
                "ErrorDeValor: '%.*s' no es decimal valido",
                v->como.cadena.longitud, v->como.cadena.texto);
        }
        return valor_decimal(d);
    }
    return error_nativa(err, linea, columna,
        "ErrorDeTipo: decimal() no acepta '%s'", valor_nombre_tipo(v));
}

/*
 * booleano(x) — siempre éxito. Aplica las reglas de truthiness de
 * ESPEC §6.2: nulo, falso, 0, 0.0, "", [], {}, () → falso; resto verdadero.
 */
static Valor nativa_booleano(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: booleano() requiere 1 argumento, recibio %d", n_args);
    }
    return valor_booleano(valor_es_verdadero(&args[0]));
}

/*
 * lista([iterable]) — construye una lista materializada.
 *
 *   lista()          → []
 *   lista([1,2,3])   → [1, 2, 3] (copia)
 *   lista((1,2))     → [1, 2]
 *   lista({"a":1})   → ["a"]                 (claves del dicc)
 *   lista({1, 2})    → [1, 2]                (orden indeterminado)
 *   lista("abc")     → ["a", "b", "c"]
 *   lista(rango(3))  → [0, 1, 2]
 *   lista(otro)      → ErrorDeTipo
 */
static Valor nativa_lista(EvalError *err, int n_args, Valor *args,
                           int linea, int columna) {
    if (n_args > 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: lista() acepta 0 o 1 argumento, recibio %d", n_args);
    }
    Lista *l = lista_nueva(0);
    if (!l) return error_nativa(err, linea, columna, "memoria insuficiente");
    if (n_args == 0) return valor_lista(l);

    const Valor *it = &args[0];
    if (!valor_es_iterable(it)) {
        lista_liberar(l);
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: lista() no acepta '%s' como iterable",
            valor_nombre_tipo(it));
    }
    Iterador *iter = iter_nuevo(it);
    if (!iter) {
        lista_liberar(l);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    Valor elem;
    while (iter_siguiente(iter, &elem)) {
        if (!lista_agregar(l, elem)) {
            /* lista_agregar libera elem internamente cuando falla. */
            iter_destruir(iter);
            lista_liberar(l);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
    }
    iter_destruir(iter);
    return valor_lista(l);
}

/*
 * tupla([iterable]) — construye una tupla inmutable a partir de un
 * iterable. Materializa primero en lista para conocer el tamaño y
 * después transfiere a la tupla (que requiere `cuenta` fijo).
 */
static Valor nativa_tupla(EvalError *err, int n_args, Valor *args,
                           int linea, int columna) {
    if (n_args > 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tupla() acepta 0 o 1 argumento, recibio %d", n_args);
    }
    if (n_args == 0) {
        Tupla *t = tupla_nueva(0);
        if (!t) return error_nativa(err, linea, columna, "memoria insuficiente");
        return valor_tupla(t);
    }
    const Valor *it = &args[0];
    if (!valor_es_iterable(it)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tupla() no acepta '%s' como iterable",
            valor_nombre_tipo(it));
    }
    /* Recolectar primero en buffer dinámico (no conocemos el tamaño
       a priori para diccionario/conjunto/cadena/rango), luego copiar. */
    Lista *tmp = lista_nueva(0);
    if (!tmp) return error_nativa(err, linea, columna, "memoria insuficiente");
    Iterador *iter = iter_nuevo(it);
    if (!iter) {
        lista_liberar(tmp);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    Valor elem;
    while (iter_siguiente(iter, &elem)) {
        if (!lista_agregar(tmp, elem)) {
            /* lista_agregar libera elem internamente cuando falla. */
            iter_destruir(iter);
            lista_liberar(tmp);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
    }
    iter_destruir(iter);

    Tupla *t = tupla_nueva(tmp->cuenta);
    if (!t) {
        lista_liberar(tmp);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    /* Mover los Valores del array temporal a la tupla. */
    for (int i = 0; i < tmp->cuenta; i++) {
        t->elementos[i] = tmp->elementos[i];
    }
    /* Vaciar tmp para que `lista_liberar` no destruya los elementos
       que ya pertenecen a la tupla. */
    tmp->cuenta = 0;
    lista_liberar(tmp);
    return valor_tupla(t);
}

/*
 * diccionario([iterable_de_pares]) — construye un diccionario.
 *
 *   diccionario()                          → {}
 *   diccionario({"a":1})                   → copia del diccionario.
 *   diccionario([("a",1), ("b",2)])        → {"a":1, "b":2}
 *   diccionario([["k",1]])                 → {"k":1}
 *   diccionario(otro)                      → ErrorDeTipo
 *
 * Cada elemento del iterable debe ser a su vez un iterable de
 * exactamente dos elementos (lista o tupla). La clave debe ser
 * hashable; si no, ErrorDeTipo.
 */
static Valor nativa_diccionario(EvalError *err, int n_args, Valor *args,
                                  int linea, int columna) {
    if (n_args > 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: diccionario() acepta 0 o 1 argumento, recibio %d",
            n_args);
    }
    Diccionario *d = dicc_nuevo();
    if (!d) return error_nativa(err, linea, columna, "memoria insuficiente");
    if (n_args == 0) return valor_diccionario(d);

    const Valor *src = &args[0];
    /* Caso especial: dicc → dicc (copia entrada por entrada). v1.20:
       en orden de inserción para preservar el orden del original. */
    if (src->tipo == VAL_DICCIONARIO) {
        const Diccionario *origen = src->como.dicc;
        for (int idx = 0; idx < origen->cuenta; idx++) {
            int i = origen->orden_insercion[idx];
            const Valor *src_k = &origen->entradas[i].clave;
            const Valor *src_v = &origen->entradas[i].valor;
            Valor clave = valor_clonar(src_k);
            Valor valor = valor_clonar(src_v);
            /* `valor_clonar` devuelve VAL_NULO ante OOM al copiar
               mp_int/cadena con dueño. Distinguimos OOM de un VAL_NULO
               genuino comparando tipo original. */
            bool oom = (clave.tipo == VAL_NULO && src_k->tipo != VAL_NULO)
                    || (valor.tipo == VAL_NULO && src_v->tipo != VAL_NULO);
            if (oom) {
                valor_destruir(&clave);
                valor_destruir(&valor);
                dicc_liberar(d);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            if (!dicc_asignar(d, clave, valor)) {
                dicc_liberar(d);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
        }
        return valor_diccionario(d);
    }
    /* Caso general: iterable de pares. */
    if (!valor_es_iterable(src)) {
        dicc_liberar(d);
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: diccionario() no acepta '%s' como iterable",
            valor_nombre_tipo(src));
    }
    Iterador *iter = iter_nuevo(src);
    if (!iter) {
        dicc_liberar(d);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    Valor par;
    int idx = 0;
    while (iter_siguiente(iter, &par)) {
        Valor *elementos = NULL;
        int cuenta = 0;
        if (par.tipo == VAL_LISTA) {
            elementos = par.como.lista->elementos;
            cuenta = par.como.lista->cuenta;
        } else if (par.tipo == VAL_TUPLA) {
            elementos = par.como.tupla->elementos;
            cuenta = par.como.tupla->cuenta;
        } else {
            valor_destruir(&par);
            iter_destruir(iter);
            dicc_liberar(d);
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: diccionario() espera pares (clave, valor) en el indice %d",
                idx);
        }
        if (cuenta != 2) {
            valor_destruir(&par);
            iter_destruir(iter);
            dicc_liberar(d);
            return error_nativa(err, linea, columna,
                "ErrorDeValor: diccionario() requiere pares de longitud 2, recibio %d en indice %d",
                cuenta, idx);
        }
        if (!valor_es_hashable(&elementos[0])) {
            const char *tn = valor_nombre_tipo(&elementos[0]);
            valor_destruir(&par);
            iter_destruir(iter);
            dicc_liberar(d);
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: diccionario() clave no hashable: '%s'", tn);
        }
        Valor clave = valor_clonar(&elementos[0]);
        Valor valor = valor_clonar(&elementos[1]);
        valor_destruir(&par);
        if (!dicc_asignar(d, clave, valor)) {
            iter_destruir(iter);
            dicc_liberar(d);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        idx++;
    }
    iter_destruir(iter);
    return valor_diccionario(d);
}

/*
 * leer([prompt]) — lee una línea de stdin.
 *
 *   leer()           → lee hasta '\n' o EOF; devuelve cadena sin '\n'.
 *   leer(prompt)     → imprime prompt sin '\n' (con flush) y luego lee.
 *   leer(no-cadena)  → ErrorDeTipo.
 *   leer(>1 args)    → ErrorDeTipo.
 *
 * EOF inmediato y línea vacía son INDISTINGUIBLES — ambos devuelven
 * cadena vacía. Si tu programa necesita detectar fin-de-stream,
 * usa una sentinela (`leer("> ")` con palabra clave de fin tipo
 * "salir") o espera a v1.x para una API que reporte EOF explícito.
 *
 * Lee con buffer dinámico para soportar líneas arbitrariamente largas.
 *
 * UTF-8: los bytes se devuelven tal cual los recibe el SO; no se
 * normaliza NFC. Si el terminal entrega bytes inválidos en UTF-8,
 * `longitud(leer())` puede dar resultado inesperado al iterar
 * codepoints (`utf8proc_iterate` rechaza secuencias mal formadas).
 */
static Valor nativa_leer(EvalError *err, int n_args, Valor *args,
                          int linea, int columna) {
    if (n_args > 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: leer() acepta 0 o 1 argumento, recibio %d", n_args);
    }
    if (n_args == 1) {
        if (args[0].tipo != VAL_CADENA) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: leer() requiere una cadena como prompt, no '%s'",
                valor_nombre_tipo(&args[0]));
        }
        fwrite(args[0].como.cadena.texto, 1,
                (size_t)args[0].como.cadena.longitud, stdout);
        fflush(stdout);
    }

    /* Buffer dinámico. Empieza pequeño y dobla cuando se llena.
       Sin límite máximo; si fread agota memoria, devolvemos OOM. */
    int capacidad = 128;
    int longitud = 0;
    char *buf = (char *)malloc((size_t)capacidad);
    if (!buf) return error_nativa(err, linea, columna, "memoria insuficiente");

    int c;
    while ((c = getchar()) != EOF && c != '\n') {
        if (longitud + 1 >= capacidad) {
            int nueva = capacidad * 2;
            char *nuevo = (char *)realloc(buf, (size_t)nueva);
            if (!nuevo) {
                free(buf);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            buf = nuevo;
            capacidad = nueva;
        }
        buf[longitud++] = (char)c;
    }
    /* Trim de '\r' final si stdin viene en CRLF (ej. Windows con
       texto). El '\n' nunca queda incluido (lo consume el bucle). */
    if (longitud > 0 && buf[longitud - 1] == '\r') {
        longitud--;
    }
    Valor r = valor_cadena_duplicar(buf, longitud);
    free(buf);
    if (r.tipo == VAL_NULO) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    return r;
}

/* ──────────────────────────────────────────────────────────────────
 * Métodos sobre listas (built-ins de mutación)
 *
 * Cornamusa todavía no tiene método-syntax sobre tipos primitivos
 * (eso requiere clases, F8). Mientras tanto, las operaciones
 * mutadoras se exponen como funciones top-level que reciben la lista
 * como primer argumento. Cuando lleguen las clases mudaremos a
 * `lista.agregar(x)` con esta misma semántica.
 * ────────────────────────────────────────────────────────────────── */

static bool indice_a_long_natural(const Valor *v, long *out, int total) {
    long i;
    if (v->tipo == VAL_BOOLEANO) i = v->como.booleano ? 1 : 0;
    else {
        int64_t i64;
        if (!valor_entero_a_i64(v, &i64)) return false;
        i = (long)i64;
        if ((int64_t)i != i64) return false;
    }
    if (i < 0) i += total;
    if (i < 0 || i >= total) return false;
    *out = i;
    return true;
}

/*
 * agregar(lista|conjunto, x) — añade x al final (lista) o como nuevo
 * elemento (conjunto, deduplicado). Devuelve nulo.
 */
static Valor nativa_agregar(EvalError *err, int n_args, Valor *args,
                             int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: agregar() requiere 2 argumentos, recibio %d", n_args);
    }
    if (args[0].tipo == VAL_LISTA) {
        Valor copia = valor_clonar(&args[1]);
        if (!lista_agregar(args[0].como.lista, copia)) {
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        return valor_nulo();
    }
    if (args[0].tipo == VAL_CONJUNTO) {
        if (!valor_es_hashable(&args[1])) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: '%s' no se puede usar como elemento de conjunto",
                valor_nombre_tipo(&args[1]));
        }
        Valor copia = valor_clonar(&args[1]);
        if (!conj_agregar(args[0].como.conjunto, copia)) {
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        /* v1.42: si __hash__/__igual__ erró durante conj_agregar, el
           hook ya estableció `err->tuvo_error` (err = &vm->error). El
           llamador OP_LLAMAR_NATIVA detecta y propaga. */
        return valor_nulo();
    }
    return error_nativa(err, linea, columna,
        "ErrorDeTipo: agregar() no soporta '%s' como primer argumento",
        valor_nombre_tipo(&args[0]));
}

/*
 * quitar(lista, indice=-1) — elimina y devuelve el elemento en `indice`.
 * Sin argumento de índice usa -1 (último). Indice negativo cuenta
 * desde el final.
 */
static Valor nativa_quitar(EvalError *err, int n_args, Valor *args,
                            int linea, int columna) {
    if (n_args < 1 || n_args > 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: quitar() requiere 1 o 2 argumentos, recibio %d", n_args);
    }
    /* v1.16.1: además de listas, soporta diccionarios y conjuntos. */
    if (args[0].tipo == VAL_DICCIONARIO) {
        if (n_args != 2) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: quitar(dicc, clave) requiere 2 argumentos");
        }
        if (!valor_es_hashable(&args[1])) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: '%s' no es hashable como clave",
                valor_nombre_tipo(&args[1]));
        }
        Valor extraido;
        if (!dicc_quitar(args[0].como.dicc, &args[1], &extraido)) {
            char clave_buf[256];
            valor_a_repr(&args[1], clave_buf, sizeof(clave_buf));
            return error_nativa(err, linea, columna,
                "ErrorDeClave: clave %s no presente en diccionario",
                clave_buf);
        }
        return extraido;
    }
    if (args[0].tipo == VAL_CONJUNTO) {
        if (n_args != 2) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: quitar(conjunto, elemento) requiere 2 argumentos");
        }
        if (!valor_es_hashable(&args[1])) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: '%s' no es hashable como elemento de conjunto",
                valor_nombre_tipo(&args[1]));
        }
        if (!conj_quitar(args[0].como.conjunto, &args[1])) {
            char buf[256];
            valor_a_repr(&args[1], buf, sizeof(buf));
            return error_nativa(err, linea, columna,
                "ErrorDeClave: elemento %s no presente en conjunto", buf);
        }
        return valor_nulo();
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: quitar() requiere una lista, diccionario o conjunto, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Lista *l = args[0].como.lista;
    if (l->cuenta == 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeIndice: quitar() de una lista vacia");
    }
    long i;
    if (n_args == 2) {
        if (!indice_a_long_natural(&args[1], &i, l->cuenta)) {
            return error_nativa(err, linea, columna,
                "ErrorDeIndice: indice fuera de rango (lista de %d)", l->cuenta);
        }
    } else {
        i = l->cuenta - 1;
    }
    /* Extraer el valor (transferir ownership al cliente) y desplazar
       el resto del array. */
    Valor extraido = l->elementos[i];
    for (int k = (int)i; k < l->cuenta - 1; k++) {
        l->elementos[k] = l->elementos[k + 1];
    }
    l->cuenta--;
    return extraido;
}

/*
 * insertar(lista, indice, valor) — inserta antes del índice indicado.
 */
static Valor nativa_insertar(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args != 3) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: insertar() requiere 3 argumentos, recibio %d", n_args);
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: insertar() requiere una lista, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Lista *l = args[0].como.lista;
    /* Para insertar permitimos índices en [-cuenta, cuenta] (clamping
       a [0, cuenta] tras normalizar). */
    long i;
    if (args[1].tipo == VAL_BOOLEANO) i = args[1].como.booleano ? 1 : 0;
    else {
        int64_t i64;
        if (!valor_entero_a_i64(&args[1], &i64)) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: indice de insertar() debe ser entero");
        }
        i = (long)i64;
        if ((int64_t)i != i64) {
            return error_nativa(err, linea, columna,
                "ErrorDeValor: indice de insertar() demasiado grande");
        }
    }
    if (i < 0) i += l->cuenta;
    if (i < 0) i = 0;
    if (i > l->cuenta) i = l->cuenta;

    /* Reservar espacio (lista_agregar para forzar crecimiento, luego
       desplazar). */
    if (!lista_agregar(l, valor_nulo())) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    for (int k = l->cuenta - 1; k > (int)i; k--) {
        l->elementos[k] = l->elementos[k - 1];
    }
    l->elementos[i] = valor_clonar(&args[2]);
    return valor_nulo();
}

/*
 * invertir(lista) — invierte la lista en su sitio.
 */
static Valor nativa_invertir(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: invertir() requiere 1 argumento, recibio %d", n_args);
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: invertir() requiere una lista, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Lista *l = args[0].como.lista;
    for (int i = 0, j = l->cuenta - 1; i < j; i++, j--) {
        Valor tmp = l->elementos[i];
        l->elementos[i] = l->elementos[j];
        l->elementos[j] = tmp;
    }
    return valor_nulo();
}

/*
 * Comparador para ordenar(): devuelve <0, 0, >0 según el orden total
 * matemático/lexicográfico de los Valores. Tipos no comparables se
 * marcan vía estado global (puentea qsort que no admite contexto).
 */
static bool g_ordenar_error = false;

static int comparador_ordenar(const void *pa, const void *pb) {
    if (g_ordenar_error) return 0;
    const Valor *a = (const Valor *)pa;
    const Valor *b = (const Valor *)pb;

    /* Numéricos (entero/decimal/booleano) se comparan matemáticamente. */
    bool an = valor_es_entero(a) || a->tipo == VAL_DECIMAL || a->tipo == VAL_BOOLEANO;
    bool bn = valor_es_entero(b) || b->tipo == VAL_DECIMAL || b->tipo == VAL_BOOLEANO;
    if (an && bn) {
        bool a_ent = valor_es_entero(a) || a->tipo == VAL_BOOLEANO;
        bool b_ent = valor_es_entero(b) || b->tipo == VAL_BOOLEANO;
        if (a_ent && b_ent) {
            /* Camino rápido: ambos caben en i64. Frecuente porque los
               SMALL siempre caben, los BOOLEANOs caben, y la mayoría
               de BIGs en programas normales también. */
            int64_t ai = 0, bi = 0;
            bool a_fits = (a->tipo == VAL_BOOLEANO)
                          ? (ai = a->como.booleano ? 1 : 0, true)
                          : valor_entero_a_i64(a, &ai);
            bool b_fits = (b->tipo == VAL_BOOLEANO)
                          ? (bi = b->como.booleano ? 1 : 0, true)
                          : valor_entero_a_i64(b, &bi);
            if (a_fits && b_fits) {
                if (ai < bi) return -1;
                if (ai > bi) return 1;
                return 0;
            }
            mp_int ma, mb;
            if (mp_init_multi(&ma, &mb, NULL) != MP_OKAY) return 0;
            mp_err _r;
            if (a->tipo == VAL_BOOLEANO) mp_set_l(&ma, a->como.booleano ? 1 : 0);
            else if (a->tipo == VAL_ENTERO_SMALL) mp_set_i64(&ma, a->como.entero_small);
            else { _r = mp_copy(a->como.entero, &ma); (void)_r; }
            if (b->tipo == VAL_BOOLEANO) mp_set_l(&mb, b->como.booleano ? 1 : 0);
            else if (b->tipo == VAL_ENTERO_SMALL) mp_set_i64(&mb, b->como.entero_small);
            else { _r = mp_copy(b->como.entero, &mb); (void)_r; }
            int c = mp_cmp(&ma, &mb);
            mp_clear_multi(&ma, &mb, NULL);
            if (c == MP_LT) return -1;
            if (c == MP_GT) return 1;
            return 0;
        }
        double da = a->tipo == VAL_DECIMAL ? a->como.decimal
                  : a->tipo == VAL_BOOLEANO ? (a->como.booleano ? 1.0 : 0.0)
                  : a->tipo == VAL_ENTERO_SMALL ? (double)a->como.entero_small
                  : mp_get_double(a->como.entero);
        double db = b->tipo == VAL_DECIMAL ? b->como.decimal
                  : b->tipo == VAL_BOOLEANO ? (b->como.booleano ? 1.0 : 0.0)
                  : b->tipo == VAL_ENTERO_SMALL ? (double)b->como.entero_small
                  : mp_get_double(b->como.entero);
        if (da < db) return -1;
        if (da > db) return 1;
        return 0;
    }
    if (a->tipo == VAL_CADENA && b->tipo == VAL_CADENA) {
        int la = a->como.cadena.longitud;
        int lb = b->como.cadena.longitud;
        int min = la < lb ? la : lb;
        int c = (min > 0) ? memcmp(a->como.cadena.texto,
                                    b->como.cadena.texto, (size_t)min) : 0;
        if (c != 0) return c < 0 ? -1 : 1;
        return la < lb ? -1 : (la > lb ? 1 : 0);
    }
    g_ordenar_error = true;
    return 0;
}

static Valor nativa_ordenar(EvalError *err, int n_args, Valor *args,
                             int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: ordenar() requiere 1 argumento, recibio %d", n_args);
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: ordenar() requiere una lista, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Lista *l = args[0].como.lista;
    g_ordenar_error = false;
    qsort(l->elementos, (size_t)l->cuenta, sizeof(Valor), comparador_ordenar);
    if (g_ordenar_error) {
        g_ordenar_error = false;
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: ordenar() no puede comparar tipos mixtos no numericos");
    }
    return valor_nulo();
}

/*
 * conjunto() / conjunto(iterable) — construye un conjunto vacío o lo
 * inicializa con los elementos de `iterable` (lista, tupla, cadena, rango).
 * Necesario porque `{}` es diccionario vacío en la sintaxis literal.
 */
static Valor nativa_conjunto(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args > 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto() acepta 0 o 1 argumento, recibio %d", n_args);
    }
    Conjunto *c = conj_nuevo();
    if (!c) return error_nativa(err, linea, columna, "memoria insuficiente");
    if (n_args == 0) return valor_conjunto(c);

    const Valor *it = &args[0];
    if (it->tipo == VAL_LISTA) {
        Lista *l = it->como.lista;
        for (int i = 0; i < l->cuenta; i++) {
            if (!valor_es_hashable(&l->elementos[i])) {
                conj_liberar(c);
                return error_nativa(err, linea, columna,
                    "ErrorDeTipo: '%s' no se puede usar como elemento de conjunto",
                    valor_nombre_tipo(&l->elementos[i]));
            }
            conj_agregar(c, valor_clonar(&l->elementos[i]));
        }
    } else if (it->tipo == VAL_TUPLA) {
        Tupla *t = it->como.tupla;
        for (int i = 0; i < t->cuenta; i++) {
            if (!valor_es_hashable(&t->elementos[i])) {
                conj_liberar(c);
                return error_nativa(err, linea, columna,
                    "ErrorDeTipo: '%s' no se puede usar como elemento de conjunto",
                    valor_nombre_tipo(&t->elementos[i]));
            }
            conj_agregar(c, valor_clonar(&t->elementos[i]));
        }
    } else {
        conj_liberar(c);
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto() no acepta '%s' como iterable",
            valor_nombre_tipo(it));
    }
    return valor_conjunto(c);
}

/* ──────────────────────────────────────────────────────────────────
 * Excepciones (v0.6.3)
 *
 * `Excepcion(clase, mensaje)` construye una excepción con clase y
 * mensaje arbitrarios. Las clases canónicas (`ErrorAritmetico`, etc.)
 * son atajos que rellenan automáticamente la clase.
 * ────────────────────────────────────────────────────────────────── */

static Valor crear_excepcion(EvalError *err, const char *clase_default,
                              int n_args, Valor *args,
                              int linea, int columna) {
    /* Convenciones:
       - 0 argumentos: clase por defecto, mensaje vacío (v1.43 —
         útil para señales como `ErrorDeIteracion` sin contexto).
       - 1 argumento: la cadena es el mensaje, clase por defecto.
       - 2 argumentos: (clase, mensaje). */
    const char *cls = clase_default;
    int len_cls = (int)strlen(clase_default);
    const char *msg = "";
    int len_msg = 0;
    if (n_args == 0) {
        /* Defaults ya están listos. */
    } else if (n_args == 1) {
        if (args[0].tipo != VAL_CADENA) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: %s() espera una cadena con el mensaje",
                clase_default);
        }
        msg = args[0].como.cadena.texto;
        len_msg = args[0].como.cadena.longitud;
    } else if (n_args == 2) {
        if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: Excepcion() espera (clase: cadena, mensaje: cadena)");
        }
        cls = args[0].como.cadena.texto;
        len_cls = args[0].como.cadena.longitud;
        msg = args[1].como.cadena.texto;
        len_msg = args[1].como.cadena.longitud;
    } else {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: %s() acepta 0, 1 o 2 argumentos, recibio %d",
            clase_default, n_args);
    }
    Excepcion *e = excepcion_nueva(cls, len_cls, msg, len_msg);
    if (!e) return error_nativa(err, linea, columna, "memoria insuficiente");
    return valor_excepcion(e);
}

#define DEFINIR_EXC_NATIVA(nombre)                                              \
    static Valor nativa_exc_##nombre(EvalError *err, int n_args, Valor *args,   \
                                       int linea, int columna) {                \
        return crear_excepcion(err, #nombre, n_args, args, linea, columna);     \
    }

DEFINIR_EXC_NATIVA(Excepcion)
DEFINIR_EXC_NATIVA(ErrorAritmetico)
DEFINIR_EXC_NATIVA(ErrorDeTipo)
DEFINIR_EXC_NATIVA(ErrorDeValor)
DEFINIR_EXC_NATIVA(ErrorDeIndice)
DEFINIR_EXC_NATIVA(ErrorDeClave)
DEFINIR_EXC_NATIVA(ErrorDeNombre)
/* v1.27: para stdlib `proceso` (y futura `red`, `archivos` avanzado). */
DEFINIR_EXC_NATIVA(ErrorDeSistema)
DEFINIR_EXC_NATIVA(ErrorDeIO)
/* v1.43: señal de fin para iteradores lazy con `__siguiente__`. La VM
   la atrapa internamente en OP_ITER_SIGUIENTE para terminar el bucle
   `para`; también puede atraparse explícitamente por el usuario. */
DEFINIR_EXC_NATIVA(ErrorDeIteracion)

#undef DEFINIR_EXC_NATIVA

/* ──────────────────────────────────────────────────────────────────
 * Métodos sobre diccionarios
 * ────────────────────────────────────────────────────────────────── */

/*
 * claves(dicc) → lista de claves. El orden depende del layout interno
 * del hash table — Python 3.7+ garantiza orden de inserción; nosotros
 * iteramos por slot, lo que NO conserva el orden. Documentado.
 */
static Valor nativa_claves(EvalError *err, int n_args, Valor *args,
                            int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: claves() requiere 1 argumento, recibio %d", n_args);
    }
    if (args[0].tipo != VAL_DICCIONARIO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: claves() requiere un diccionario, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Diccionario *d = args[0].como.dicc;
    Lista *l = lista_nueva(d->cuenta);
    if (!l) return error_nativa(err, linea, columna, "memoria insuficiente");
    /* v1.20: orden de inserción. */
    for (int i = 0; i < d->cuenta; i++) {
        int slot = d->orden_insercion[i];
        lista_agregar(l, valor_clonar(&d->entradas[slot].clave));
    }
    return valor_lista(l);
}

/* valores(dicc) → lista de valores en orden de inserción (v1.20). */
static Valor nativa_valores(EvalError *err, int n_args, Valor *args,
                             int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: valores() requiere 1 argumento, recibio %d", n_args);
    }
    if (args[0].tipo != VAL_DICCIONARIO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: valores() requiere un diccionario, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Diccionario *d = args[0].como.dicc;
    Lista *l = lista_nueva(d->cuenta);
    if (!l) return error_nativa(err, linea, columna, "memoria insuficiente");
    for (int i = 0; i < d->cuenta; i++) {
        int slot = d->orden_insercion[i];
        lista_agregar(l, valor_clonar(&d->entradas[slot].valor));
    }
    return valor_lista(l);
}

/*
 * recolectar() → entero (objetos liberados).
 *
 * Fuerza un ciclo de mark-sweep manual. Útil cuando el código sospecha
 * que ha creado ciclos refcount (ej. dos diccionarios mutuamente
 * referenciados). El refcount sigue siendo el liberador primario para
 * objetos sin ciclos; recolectar() limpia los que quedan colgados.
 *
 * Acepta 0 args. Devuelve el número de objetos heap liberados durante
 * la pasada (entero ≥ 0).
 *
 * Si no hay GC instalado (ej. en evaluador tree-walking sin VM),
 * devuelve 0.
 */
static Valor nativa_recolectar(EvalError *err, int n_args, Valor *args,
                                int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: recolectar() no acepta argumentos, recibio %d",
            n_args);
    }
    Memoria *m = gc_actual();
    if (!m || !m->fn_marcar_raices) {
        return valor_entero_de_long(0);
    }
    /* Protección: si una recolección ya está en marcha (recursión a
       través de un callback), devolvemos 0 sin reentrar. */
    if (m->recolectando) {
        return valor_entero_de_long(0);
    }
    m->recolectando = true;
    size_t liberados = gc_recolectar(m, m->fn_marcar_raices,
                                       m->contexto_raices);
    m->trigger_pendiente = false;
    m->recolectando = false;
    return valor_entero_de_long((long)liberados);
}

/* ──────────────────────────────────────────────────────────────────
 * Nativos de sistema (v0.9.2)
 * ────────────────────────────────────────────────────────────────── */

/* Argv del proceso, set desde main.c via nativos_set_argv. */
static int g_argc_user = 0;
static char **g_argv_user = NULL;

void nativos_set_argv(int argc, char **argv) {
    g_argc_user = argc;
    g_argv_user = argv;
}

/*
 * obtener_argv() → lista de cadenas con los argumentos del programa.
 * El primer elemento es el nombre del archivo .cor ejecutado (si lo hay)
 * y los siguientes son cualquier argumento adicional pasado tras él.
 */
static Valor nativa_obtener_argv(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: obtener_argv() no acepta argumentos, recibio %d",
            n_args);
    }
    Lista *l = lista_nueva(g_argc_user);
    if (!l) return error_nativa(err, linea, columna, "memoria insuficiente");
    for (int i = 0; i < g_argc_user; i++) {
        const char *s = g_argv_user[i] ? g_argv_user[i] : "";
        lista_agregar(l, valor_cadena_duplicar(s, (int)strlen(s)));
    }
    return valor_lista(l);
}

/* ──────────────────────────────────────────────────────────────────
 * JSON (v1.9)
 *
 * Built-ins primitivos para parsear y serializar JSON estándar
 * (RFC 8259). La capa de usuario es `stdlib/json.cor` que reexporta
 * con nombres castellanos (`json.parsear`, `json.serializar`).
 *
 * Mapeo (Cornamusa ↔ JSON):
 *   nulo        ↔ null
 *   verdadero   ↔ true
 *   falso       ↔ false
 *   entero/decimal ↔ number
 *   cadena      ↔ string
 *   lista/tupla ↔ array (tupla → array; al re-parsear es lista)
 *   diccionario ↔ object (claves cadena obligatorias)
 *
 * Filosofía v1.9: JSON es un formato de intercambio universal.
 * Cornamusa preserva su identidad castellana en CÓDIGO (los
 * literales `verdadero/falso/nulo` siguen igual) pero acepta JSON
 * estándar para interoperar. El usuario nunca ve `true/false/null`
 * en su código Cornamusa — solo en archivos JSON externos.
 * ────────────────────────────────────────────────────────────────── */

typedef struct {
    const char *texto;
    int pos;
    int len;
    char err_msg[256];
    bool tuvo_error;
} JsonParser;

static void jp_set_error(JsonParser *p, const char *fmt, ...) {
    if (p->tuvo_error) return;
    p->tuvo_error = true;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(p->err_msg, sizeof(p->err_msg), fmt, ap);
    va_end(ap);
}

static void jp_saltar_espacios(JsonParser *p) {
    while (p->pos < p->len) {
        char c = p->texto[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->pos++;
        else break;
    }
}

static Valor jp_valor(JsonParser *p);  /* fwd decl */

static Valor jp_string(JsonParser *p) {
    if (p->pos >= p->len || p->texto[p->pos] != '"') {
        jp_set_error(p, "se esperaba '\"' en posicion %d", p->pos);
        return valor_nulo();
    }
    p->pos++;
    int cap = 64, cuenta = 0;
    char *buf = (char *)malloc((size_t)cap);
    if (!buf) { jp_set_error(p, "memoria insuficiente"); return valor_nulo(); }
#define APPEND_C(c) do { \
        if (cuenta + 1 > cap) { \
            cap *= 2; \
            char *nb = (char *)realloc(buf, (size_t)cap); \
            if (!nb) { free(buf); jp_set_error(p, "memoria insuficiente"); return valor_nulo(); } \
            buf = nb; \
        } \
        buf[cuenta++] = (c); \
    } while (0)
    while (p->pos < p->len) {
        char c = p->texto[p->pos];
        if (c == '"') {
            p->pos++;
            Valor r = valor_cadena_duplicar(buf, cuenta);
            free(buf);
            if (r.tipo == VAL_NULO) jp_set_error(p, "memoria insuficiente");
            return r;
        }
        if (c == '\\') {
            p->pos++;
            if (p->pos >= p->len) {
                free(buf);
                jp_set_error(p, "escape sin completar al final del JSON");
                return valor_nulo();
            }
            char esc = p->texto[p->pos++];
            switch (esc) {
                case '"':  APPEND_C('"');  break;
                case '\\': APPEND_C('\\'); break;
                case '/':  APPEND_C('/');  break;
                case 'b':  APPEND_C('\b'); break;
                case 'f':  APPEND_C('\f'); break;
                case 'n':  APPEND_C('\n'); break;
                case 'r':  APPEND_C('\r'); break;
                case 't':  APPEND_C('\t'); break;
                case 'u': {
                    if (p->pos + 4 > p->len) {
                        free(buf);
                        jp_set_error(p, "\\u sin 4 hex digits");
                        return valor_nulo();
                    }
                    unsigned cp = 0;
                    for (int i = 0; i < 4; i++) {
                        char h = p->texto[p->pos + i];
                        unsigned d;
                        if (h >= '0' && h <= '9') d = (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') d = (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') d = (unsigned)(h - 'A' + 10);
                        else {
                            free(buf);
                            jp_set_error(p, "hex invalido en \\u");
                            return valor_nulo();
                        }
                        cp = (cp << 4) | d;
                    }
                    p->pos += 4;
                    if (cp < 0x80) {
                        APPEND_C((char)cp);
                    } else if (cp < 0x800) {
                        APPEND_C((char)(0xC0 | (cp >> 6)));
                        APPEND_C((char)(0x80 | (cp & 0x3F)));
                    } else {
                        APPEND_C((char)(0xE0 | (cp >> 12)));
                        APPEND_C((char)(0x80 | ((cp >> 6) & 0x3F)));
                        APPEND_C((char)(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default:
                    free(buf);
                    jp_set_error(p, "escape invalido \\%c", esc);
                    return valor_nulo();
            }
        } else if ((unsigned char)c < 0x20) {
            free(buf);
            jp_set_error(p, "caracter de control sin escapar en cadena");
            return valor_nulo();
        } else {
            APPEND_C(c);
            p->pos++;
        }
    }
    free(buf);
    jp_set_error(p, "cadena sin cerrar");
    return valor_nulo();
#undef APPEND_C
}

static Valor jp_numero(JsonParser *p) {
    int inicio = p->pos;
    bool tiene_punto = false, tiene_exp = false;
    if (p->pos < p->len && p->texto[p->pos] == '-') p->pos++;
    while (p->pos < p->len && p->texto[p->pos] >= '0' && p->texto[p->pos] <= '9') p->pos++;
    if (p->pos < p->len && p->texto[p->pos] == '.') {
        tiene_punto = true; p->pos++;
        while (p->pos < p->len && p->texto[p->pos] >= '0' && p->texto[p->pos] <= '9') p->pos++;
    }
    if (p->pos < p->len && (p->texto[p->pos] == 'e' || p->texto[p->pos] == 'E')) {
        tiene_exp = true; p->pos++;
        if (p->pos < p->len && (p->texto[p->pos] == '+' || p->texto[p->pos] == '-')) p->pos++;
        while (p->pos < p->len && p->texto[p->pos] >= '0' && p->texto[p->pos] <= '9') p->pos++;
    }
    int len_num = p->pos - inicio;
    if (len_num == 0 || (len_num == 1 && p->texto[inicio] == '-')) {
        jp_set_error(p, "numero invalido");
        return valor_nulo();
    }
    char buf_stack[64];
    char *buf = buf_stack;
    char *heap = NULL;
    if (len_num + 1 > (int)sizeof(buf_stack)) {
        heap = (char *)malloc((size_t)len_num + 1);
        if (!heap) { jp_set_error(p, "memoria insuficiente"); return valor_nulo(); }
        buf = heap;
    }
    memcpy(buf, p->texto + inicio, (size_t)len_num);
    buf[len_num] = '\0';
    Valor r;
    if (tiene_punto || tiene_exp) {
        r = valor_decimal(strtod(buf, NULL));
    } else {
        char *end = NULL;
        long long v = strtoll(buf, &end, 10);
        if (end == buf || *end != '\0') {
            r = valor_decimal(strtod(buf, NULL));
        } else {
            r = valor_entero_de_i64((int64_t)v);
        }
    }
    if (heap) free(heap);
    return r;
}

static Valor jp_array(JsonParser *p) {
    p->pos++;
    Lista *l = lista_nueva(0);
    if (!l) { jp_set_error(p, "memoria insuficiente"); return valor_nulo(); }
    jp_saltar_espacios(p);
    if (p->pos < p->len && p->texto[p->pos] == ']') {
        p->pos++;
        return valor_lista(l);
    }
    for (;;) {
        Valor v = jp_valor(p);
        if (p->tuvo_error) {
            valor_destruir(&v);
            lista_liberar(l);
            return valor_nulo();
        }
        if (!lista_agregar(l, v)) {
            lista_liberar(l);
            jp_set_error(p, "memoria insuficiente");
            return valor_nulo();
        }
        jp_saltar_espacios(p);
        if (p->pos >= p->len) {
            lista_liberar(l);
            jp_set_error(p, "array sin cerrar");
            return valor_nulo();
        }
        char c = p->texto[p->pos];
        if (c == ']') { p->pos++; return valor_lista(l); }
        if (c != ',') {
            lista_liberar(l);
            jp_set_error(p, "se esperaba ',' o ']'");
            return valor_nulo();
        }
        p->pos++;
        jp_saltar_espacios(p);
    }
}

static Valor jp_object(JsonParser *p) {
    p->pos++;
    Diccionario *d = dicc_nuevo();
    if (!d) { jp_set_error(p, "memoria insuficiente"); return valor_nulo(); }
    jp_saltar_espacios(p);
    if (p->pos < p->len && p->texto[p->pos] == '}') {
        p->pos++;
        return valor_diccionario(d);
    }
    for (;;) {
        jp_saltar_espacios(p);
        Valor clave = jp_string(p);
        if (p->tuvo_error) {
            valor_destruir(&clave);
            dicc_liberar(d);
            return valor_nulo();
        }
        jp_saltar_espacios(p);
        if (p->pos >= p->len || p->texto[p->pos] != ':') {
            valor_destruir(&clave);
            dicc_liberar(d);
            jp_set_error(p, "se esperaba ':' tras clave");
            return valor_nulo();
        }
        p->pos++;
        jp_saltar_espacios(p);
        Valor val = jp_valor(p);
        if (p->tuvo_error) {
            valor_destruir(&clave);
            valor_destruir(&val);
            dicc_liberar(d);
            return valor_nulo();
        }
        if (!dicc_asignar(d, clave, val)) {
            dicc_liberar(d);
            jp_set_error(p, "memoria insuficiente");
            return valor_nulo();
        }
        jp_saltar_espacios(p);
        if (p->pos >= p->len) {
            dicc_liberar(d);
            jp_set_error(p, "objeto sin cerrar");
            return valor_nulo();
        }
        char c = p->texto[p->pos];
        if (c == '}') { p->pos++; return valor_diccionario(d); }
        if (c != ',') {
            dicc_liberar(d);
            jp_set_error(p, "se esperaba ',' o '}'");
            return valor_nulo();
        }
        p->pos++;
    }
}

static bool jp_match_lit(JsonParser *p, const char *lit) {
    int n = (int)strlen(lit);
    if (p->pos + n > p->len) return false;
    if (memcmp(p->texto + p->pos, lit, (size_t)n) != 0) return false;
    p->pos += n;
    return true;
}

static Valor jp_valor(JsonParser *p) {
    jp_saltar_espacios(p);
    if (p->pos >= p->len) {
        jp_set_error(p, "JSON vacio o truncado");
        return valor_nulo();
    }
    char c = p->texto[p->pos];
    if (c == '"') return jp_string(p);
    if (c == '[') return jp_array(p);
    if (c == '{') return jp_object(p);
    if (c == '-' || (c >= '0' && c <= '9')) return jp_numero(p);
    if (jp_match_lit(p, "true"))  return valor_booleano(true);
    if (jp_match_lit(p, "false")) return valor_booleano(false);
    if (jp_match_lit(p, "null"))  return valor_nulo();
    jp_set_error(p, "valor JSON no reconocido en pos %d", p->pos);
    return valor_nulo();
}

static Valor nativa_json_parsear(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: json_parsear() requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: json_parsear() espera una cadena con JSON");
    }
    JsonParser p;
    p.texto = args[0].como.cadena.texto;
    p.len = args[0].como.cadena.longitud;
    p.pos = 0;
    p.tuvo_error = false;
    p.err_msg[0] = '\0';
    Valor r = jp_valor(&p);
    if (p.tuvo_error) {
        valor_destruir(&r);
        return error_nativa(err, linea, columna,
            "ErrorDeValor: JSON invalido — %s", p.err_msg);
    }
    jp_saltar_espacios(&p);
    if (p.pos < p.len) {
        valor_destruir(&r);
        return error_nativa(err, linea, columna,
            "ErrorDeValor: JSON con caracteres sobrantes");
    }
    return r;
}

/* ───── Serializer ───── */

typedef struct {
    char *buf;
    int cap;
    int cuenta;
    bool oom;
} JsonOut;

static void jo_iniciar(JsonOut *o) {
    o->cap = 256;
    o->cuenta = 0;
    o->oom = false;
    o->buf = (char *)malloc((size_t)o->cap);
    if (!o->buf) o->oom = true;
}

static void jo_append(JsonOut *o, const char *s, int n) {
    if (o->oom) return;
    if (o->cuenta + n + 1 > o->cap) {
        int nuevo = o->cap;
        while (nuevo < o->cuenta + n + 1) nuevo *= 2;
        char *nb = (char *)realloc(o->buf, (size_t)nuevo);
        if (!nb) { o->oom = true; return; }
        o->buf = nb;
        o->cap = nuevo;
    }
    memcpy(o->buf + o->cuenta, s, (size_t)n);
    o->cuenta += n;
}

static void jo_append_str(JsonOut *o, const char *s) {
    jo_append(o, s, (int)strlen(s));
}

static void jo_append_char(JsonOut *o, char c) {
    jo_append(o, &c, 1);
}

static void jo_escape_cadena(JsonOut *o, const char *s, int n) {
    jo_append_char(o, '"');
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  jo_append_str(o, "\\\""); break;
            case '\\': jo_append_str(o, "\\\\"); break;
            case '\b': jo_append_str(o, "\\b"); break;
            case '\f': jo_append_str(o, "\\f"); break;
            case '\n': jo_append_str(o, "\\n"); break;
            case '\r': jo_append_str(o, "\\r"); break;
            case '\t': jo_append_str(o, "\\t"); break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    jo_append_str(o, buf);
                } else {
                    jo_append_char(o, (char)c);
                }
        }
    }
    jo_append_char(o, '"');
}

/* v1.16.1: emite indentación según `nivel` (cuenta de aperturas
   anidadas) e `indent` (espacios por nivel). NO emite si indent=0. */
static void jo_indent(JsonOut *o, int indent, int nivel) {
    if (indent <= 0) return;
    jo_append_char(o, '\n');
    for (int i = 0; i < nivel * indent; i++) jo_append_char(o, ' ');
}

static bool js_serializar(JsonOut *o, const Valor *v, int indent, int nivel,
                            char *err, int err_cap) {
    if (o->oom) { snprintf(err, err_cap, "memoria insuficiente"); return false; }
    switch (v->tipo) {
        case VAL_NULO: jo_append_str(o, "null"); return true;
        case VAL_BOOLEANO:
            jo_append_str(o, v->como.booleano ? "true" : "false");
            return true;
        case VAL_ENTERO_SMALL: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%lld", (long long)v->como.entero_small);
            jo_append_str(o, buf);
            return true;
        }
        case VAL_ENTERO: {
            int tam = 0;
            if (mp_radix_size(v->como.entero, 10, &tam) != MP_OKAY) {
                snprintf(err, err_cap, "memoria insuficiente");
                return false;
            }
            char *buf = (char *)malloc((size_t)tam);
            if (!buf) { snprintf(err, err_cap, "memoria insuficiente"); return false; }
            size_t escritos;
            if (mp_to_radix(v->como.entero, buf, (size_t)tam, &escritos, 10) != MP_OKAY) {
                free(buf);
                snprintf(err, err_cap, "error formateando entero");
                return false;
            }
            jo_append(o, buf, (int)escritos - 1);
            free(buf);
            return true;
        }
        case VAL_DECIMAL: {
            double d = v->como.decimal;
            if (d != d) {
                snprintf(err, err_cap,
                    "ErrorDeValor: NaN no es serializable a JSON");
                return false;
            }
            /* Detectar Inf: 2*Inf == Inf, pero d != d*0.5 si d es Inf. */
            if (d > 1.7976931348623157e308 || d < -1.7976931348623157e308) {
                snprintf(err, err_cap,
                    "ErrorDeValor: Infinito no es serializable a JSON");
                return false;
            }
            char buf[64];
            snprintf(buf, sizeof(buf), "%.17g", d);
            jo_append_str(o, buf);
            return true;
        }
        case VAL_CADENA:
            jo_escape_cadena(o, v->como.cadena.texto, v->como.cadena.longitud);
            return true;
        case VAL_LISTA: {
            Lista *l = v->como.lista;
            jo_append_char(o, '[');
            for (int i = 0; i < l->cuenta; i++) {
                if (i > 0) jo_append_char(o, ',');
                jo_indent(o, indent, nivel + 1);
                if (!js_serializar(o, &l->elementos[i], indent, nivel + 1,
                                    err, err_cap)) return false;
            }
            if (l->cuenta > 0) jo_indent(o, indent, nivel);
            jo_append_char(o, ']');
            return true;
        }
        case VAL_TUPLA: {
            Tupla *t = v->como.tupla;
            jo_append_char(o, '[');
            for (int i = 0; i < t->cuenta; i++) {
                if (i > 0) jo_append_char(o, ',');
                jo_indent(o, indent, nivel + 1);
                if (!js_serializar(o, &t->elementos[i], indent, nivel + 1,
                                    err, err_cap)) return false;
            }
            if (t->cuenta > 0) jo_indent(o, indent, nivel);
            jo_append_char(o, ']');
            return true;
        }
        case VAL_DICCIONARIO: {
            jo_append_char(o, '{');
            Diccionario *d = v->como.dicc;
            /* v1.20: serializar en orden de inserción. JSON output
               estable y predecible para configs/snapshots. */
            for (int idx = 0; idx < d->cuenta; idx++) {
                int slot = d->orden_insercion[idx];
                EntradaDicc *e = &d->entradas[slot];
                if (e->clave.tipo != VAL_CADENA) {
                    snprintf(err, err_cap,
                        "ErrorDeTipo: solo claves cadena son serializables a JSON");
                    return false;
                }
                if (idx > 0) jo_append_char(o, ',');
                jo_indent(o, indent, nivel + 1);
                jo_escape_cadena(o, e->clave.como.cadena.texto,
                                  e->clave.como.cadena.longitud);
                jo_append_char(o, ':');
                if (indent > 0) jo_append_char(o, ' ');
                if (!js_serializar(o, &e->valor, indent, nivel + 1,
                                    err, err_cap)) return false;
            }
            if (d->cuenta > 0) jo_indent(o, indent, nivel);
            jo_append_char(o, '}');
            return true;
        }
        default:
            snprintf(err, err_cap,
                "ErrorDeTipo: '%s' no es serializable a JSON",
                valor_nombre_tipo(v));
            return false;
    }
}

static Valor nativa_json_serializar(EvalError *err, int n_args, Valor *args,
                                      int linea, int columna) {
    if (n_args < 1 || n_args > 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: json_serializar() acepta 1 o 2 argumentos, recibio %d",
            n_args);
    }
    /* v1.16.1: segundo argumento opcional `indentar` para pretty-print.
       0 = compacto (default, comportamiento v1.9). >0 = espacios por
       nivel. Negativo o no-entero: ErrorDeTipo. */
    int indent = 0;
    if (n_args == 2) {
        int64_t k;
        if (!valor_entero_a_i64(&args[1], &k) || k < 0) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: json_serializar() requiere entero >= 0 como 'indentar'");
        }
        if (k > 32) k = 32;  /* clamp razonable */
        indent = (int)k;
    }
    JsonOut o;
    jo_iniciar(&o);
    if (o.oom) {
        free(o.buf);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    char err_local[256];
    err_local[0] = '\0';
    if (!js_serializar(&o, &args[0], indent, 0, err_local, sizeof(err_local))) {
        free(o.buf);
        return error_nativa(err, linea, columna, "%s", err_local);
    }
    Valor r = valor_cadena_duplicar(o.buf, o.cuenta);
    free(o.buf);
    if (r.tipo == VAL_NULO) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    return r;
}

/* ──────────────────────────────────────────────────────────────────
 * I/O de archivos (v1.8)
 *
 * Built-ins primitivos para leer y escribir archivos. La capa de
 * usuario es el módulo `stdlib/archivos.cor` que reexporta estos
 * con nombres amigables (`archivos.leer`, etc.).
 *
 * Manejo de errores: si fopen/fread/fwrite fallan (archivo no existe,
 * permisos, OOM), reportan error de runtime con `error_nativa`. Los
 * errores no son atrapables vía `intentar/atrapar` (limitación
 * preexistente de las nativas) — el programa termina. Para v1.8 esto
 * es aceptable; iterar a una API que reporte excepciones atrapables
 * queda para v1.9+.
 *
 * Encoding: bytes crudos. Cornamusa cadenas son UTF-8 pero estas
 * funciones no validan ni convierten. El programa que lea un archivo
 * UTF-16 vería bytes raros. Documentado.
 * ────────────────────────────────────────────────────────────────── */

/*
 * archivo_leer(ruta) → cadena con todo el contenido del archivo.
 *
 * Modo de lectura binaria. Para archivos grandes, usa el sistema
 * operativo para alocar el tamaño exacto.
 */
static Valor nativa_archivo_leer(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_leer() requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_leer() espera una cadena con la ruta");
    }
    /* Necesitamos null-terminar la ruta — VAL_CADENA no lo está. */
    int len_ruta = args[0].como.cadena.longitud;
    char buf_ruta_stack[1024];
    char *ruta = buf_ruta_stack;
    char *ruta_heap = NULL;
    if (len_ruta + 1 > (int)sizeof(buf_ruta_stack)) {
        ruta_heap = (char *)malloc((size_t)len_ruta + 1);
        if (!ruta_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        ruta = ruta_heap;
    }
    memcpy(ruta, args[0].como.cadena.texto, (size_t)len_ruta);
    ruta[len_ruta] = '\0';

    FILE *f = fopen(ruta, "rb");
    if (!f) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo abrir '%s' para lectura", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        if (ruta_heap) free(ruta_heap);
        return error_nativa(err, linea, columna,
            "ErrorDeIO: fseek fallo en archivo");
    }
    long tam = ftell(f);
    if (tam < 0) {
        fclose(f);
        if (ruta_heap) free(ruta_heap);
        return error_nativa(err, linea, columna,
            "ErrorDeIO: ftell fallo en archivo");
    }
    rewind(f);

    char *contenido = (char *)malloc((size_t)tam + 1);
    if (!contenido) {
        fclose(f);
        if (ruta_heap) free(ruta_heap);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    size_t leido = fread(contenido, 1, (size_t)tam, f);
    fclose(f);
    if (ruta_heap) free(ruta_heap);
    contenido[leido] = '\0';
    Valor r = valor_cadena_duplicar(contenido, (int)leido);
    free(contenido);
    if (r.tipo == VAL_NULO) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    return r;
}

/*
 * archivo_escribir(ruta, contenido) → nulo.
 *
 * Modo de escritura: trunca el archivo si existe (rb+ → wb).
 */
static Valor nativa_archivo_escribir(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_escribir() requiere 2 argumentos, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_escribir() espera ruta como cadena");
    }
    if (args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_escribir() espera contenido como cadena");
    }
    int len_ruta = args[0].como.cadena.longitud;
    char buf_ruta_stack[1024];
    char *ruta = buf_ruta_stack;
    char *ruta_heap = NULL;
    if (len_ruta + 1 > (int)sizeof(buf_ruta_stack)) {
        ruta_heap = (char *)malloc((size_t)len_ruta + 1);
        if (!ruta_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        ruta = ruta_heap;
    }
    memcpy(ruta, args[0].como.cadena.texto, (size_t)len_ruta);
    ruta[len_ruta] = '\0';

    FILE *f = fopen(ruta, "wb");
    if (!f) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo abrir '%s' para escritura", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
    int len_contenido = args[1].como.cadena.longitud;
    if (len_contenido > 0) {
        size_t escrito = fwrite(args[1].como.cadena.texto, 1,
                                  (size_t)len_contenido, f);
        if ((int)escrito != len_contenido) {
            fclose(f);
            if (ruta_heap) free(ruta_heap);
            return error_nativa(err, linea, columna,
                "ErrorDeIO: fwrite fallo en archivo");
        }
    }
    fclose(f);
    if (ruta_heap) free(ruta_heap);
    return valor_nulo();
}

/*
 * archivo_existe(ruta) → booleano.
 *
 * Implementación portable usando fopen("rb"). NO distingue entre
 * "archivo no existe" y "permisos negados" — ambos retornan falso.
 */
static Valor nativa_archivo_existe(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_existe() requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_existe() espera una cadena con la ruta");
    }
    int len_ruta = args[0].como.cadena.longitud;
    char buf_ruta_stack[1024];
    char *ruta = buf_ruta_stack;
    char *ruta_heap = NULL;
    if (len_ruta + 1 > (int)sizeof(buf_ruta_stack)) {
        ruta_heap = (char *)malloc((size_t)len_ruta + 1);
        if (!ruta_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        ruta = ruta_heap;
    }
    memcpy(ruta, args[0].como.cadena.texto, (size_t)len_ruta);
    ruta[len_ruta] = '\0';

    FILE *f = fopen(ruta, "rb");
    bool existe = (f != NULL);
    if (f) fclose(f);
    if (ruta_heap) free(ruta_heap);
    return valor_booleano(existe);
}

/*
 * archivo_lineas(ruta) → lista de cadenas, una por línea.
 *
 * El separador es '\n'. La línea final se incluye aunque no termine
 * con '\n'. Para archivos CRLF (Windows), el '\r' final de cada línea
 * se conserva — el programa puede limpiarlo si lo necesita.
 */
static Valor nativa_archivo_lineas(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    /* Reusa la lógica de archivo_leer y luego split. */
    Valor contenido = nativa_archivo_leer(err, n_args, args, linea, columna);
    if (err->tuvo_error || contenido.tipo != VAL_CADENA) {
        return contenido;
    }
    Lista *l = lista_nueva(0);
    if (!l) {
        valor_destruir(&contenido);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    const char *texto = contenido.como.cadena.texto;
    int total = contenido.como.cadena.longitud;
    int inicio_linea = 0;
    for (int i = 0; i <= total; i++) {
        if (i == total || texto[i] == '\n') {
            int len_linea = i - inicio_linea;
            /* Si la última línea está vacía y veníamos de un '\n', no
               la añadimos (estilo readlines de Python). */
            if (i == total && len_linea == 0 && i > 0 && texto[i-1] == '\n') {
                break;
            }
            Valor v = valor_cadena_duplicar(texto + inicio_linea, len_linea);
            if (v.tipo == VAL_NULO) {
                lista_liberar(l);
                valor_destruir(&contenido);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            if (!lista_agregar(l, v)) {
                lista_liberar(l);
                valor_destruir(&contenido);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            inicio_linea = i + 1;
        }
    }
    valor_destruir(&contenido);
    return valor_lista(l);
}

/*
 * archivo_agregar(ruta, contenido) → nulo.
 *
 * Modo append: añade `contenido` al final de `ruta`. Crea el archivo
 * si no existe. Útil para logs.
 */
static Valor nativa_archivo_agregar(EvalError *err, int n_args, Valor *args,
                                      int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_agregar() requiere 2 argumentos, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_agregar() espera ruta como cadena");
    }
    if (args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_agregar() espera contenido como cadena");
    }
    int len_ruta = args[0].como.cadena.longitud;
    char buf_ruta_stack[1024];
    char *ruta = buf_ruta_stack;
    char *ruta_heap = NULL;
    if (len_ruta + 1 > (int)sizeof(buf_ruta_stack)) {
        ruta_heap = (char *)malloc((size_t)len_ruta + 1);
        if (!ruta_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        ruta = ruta_heap;
    }
    memcpy(ruta, args[0].como.cadena.texto, (size_t)len_ruta);
    ruta[len_ruta] = '\0';

    FILE *f = fopen(ruta, "ab");
    if (!f) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo abrir '%s' para append", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
    int len_contenido = args[1].como.cadena.longitud;
    if (len_contenido > 0) {
        size_t escrito = fwrite(args[1].como.cadena.texto, 1,
                                  (size_t)len_contenido, f);
        if ((int)escrito != len_contenido) {
            fclose(f);
            if (ruta_heap) free(ruta_heap);
            return error_nativa(err, linea, columna,
                "ErrorDeIO: fwrite fallo en archivo");
        }
    }
    fclose(f);
    if (ruta_heap) free(ruta_heap);
    return valor_nulo();
}

/*
 * salir(codigo) → no retorna. Termina el proceso con el código indicado.
 * Si codigo no es entero/booleano, error de tipo.
 */
static Valor nativa_salir(EvalError *err, int n_args, Valor *args,
                           int linea, int columna) {
    int codigo = 0;
    if (n_args > 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: salir() acepta como máximo 1 argumento, recibio %d",
            n_args);
    }
    if (n_args == 1) {
        const Valor *v = &args[0];
        if (v->tipo == VAL_BOOLEANO) {
            codigo = v->como.booleano ? 1 : 0;
        } else {
            int64_t i64;
            if (!valor_entero_a_i64(v, &i64)) {
                return error_nativa(err, linea, columna,
                    "ErrorDeTipo: salir() requiere un entero, no '%s'",
                    valor_nombre_tipo(v));
            }
            codigo = (int)i64;
        }
    }
    exit(codigo);
    return valor_nulo();   /* unreachable */
}

/* ──────────────────────────────────────────────────────────────────
 * Numéricos (v1.11): absoluto, redondear
 * ────────────────────────────────────────────────────────────────── */

/*
 * absoluto(n) → |n|. Soporta entero (SMALL/BIG, signo flip exacto),
 * decimal (fabs), booleano (entero 0/1). Para otros tipos: ErrorDeTipo.
 *
 * NaN se preserva como decimal (consistente con `cadena()` en NaN).
 * No invoca dunder `__absoluto__` todavía (pendiente para v1.12+).
 */
static Valor nativa_absoluto(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: absoluto() requiere 1 argumento, recibio %d", n_args);
    }
    const Valor *v = &args[0];
    if (v->tipo == VAL_ENTERO_SMALL) {
        int64_t n = v->como.entero_small;
        if (n == INT64_MIN) {
            /* Imposible en práctica: SMALL_INT_MIN = -2^62, pero por
               ortogonalidad si llegase n = INT64_MIN, promovemos a BIG. */
            mp_int *r = (mp_int *)malloc(sizeof(mp_int));
            if (!r || mp_init(r) != MP_OKAY) {
                free(r);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            mp_set_i64(r, n);
            if (mp_neg(r, r) != MP_OKAY) {
                mp_clear(r); free(r);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            return valor_entero_de_mp_normalizado(r);
        }
        return valor_entero_de_i64(n < 0 ? -n : n);
    }
    if (v->tipo == VAL_ENTERO) {
        mp_int *r = (mp_int *)malloc(sizeof(mp_int));
        if (!r || mp_init(r) != MP_OKAY) {
            free(r);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        if (mp_abs(v->como.entero, r) != MP_OKAY) {
            mp_clear(r); free(r);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        return valor_entero_de_mp_normalizado(r);
    }
    if (v->tipo == VAL_DECIMAL) {
        double d = v->como.decimal;
        return valor_decimal(d < 0.0 ? -d : d);
    }
    if (v->tipo == VAL_BOOLEANO) {
        return valor_entero_de_i64(v->como.booleano ? 1 : 0);
    }
    return error_nativa(err, linea, columna,
        "ErrorDeTipo: absoluto() no acepta '%s'", valor_nombre_tipo(v));
}

/*
 * redondear(n) → entero más próximo (half-away-from-zero, como Python 2).
 * redondear(n, k) → decimal con k decimales (k entero >=0).
 *
 * Soporta entero (no-op para 1 arg, redondea a 10^(-k)? — nope, k>=0
 * sobre entero es no-op), decimal y booleano. Otros: ErrorDeTipo.
 *
 * Half-away-from-zero (no banker's): redondear(0.5) = 1, redondear(-0.5)
 * = -1. Es lo que el alumno espera; banker's es sorpresa.
 */
static Valor nativa_redondear(EvalError *err, int n_args, Valor *args,
                               int linea, int columna) {
    if (n_args < 1 || n_args > 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: redondear() acepta 1 o 2 argumentos, recibio %d", n_args);
    }
    const Valor *v = &args[0];

    int decimales = 0;
    if (n_args == 2) {
        int64_t k;
        if (!valor_entero_a_i64(&args[1], &k) || k < 0) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: redondear() requiere entero >= 0 como segundo argumento");
        }
        if (k > 30) k = 30;  /* clamp; doubles solo tienen ~15-17 digitos */
        decimales = (int)k;
    }

    /* Enteros: redondear es no-op para k>=0. */
    if (valor_es_entero(v)) {
        if (n_args == 1) {
            Valor r = valor_clonar(v);
            if (r.tipo == VAL_NULO && v->tipo != VAL_NULO) {
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            return r;
        }
        /* Con `decimales`, devolver decimal por consistencia con Python. */
        int64_t n;
        double d;
        if (valor_entero_a_i64(v, &n)) {
            d = (double)n;
        } else {
            /* Bignum: convertir a double (puede perder precisión). */
            bool propio = false;
            mp_int *m = valor_entero_a_mp_int(v, &propio);
            if (!m) return error_nativa(err, linea, columna, "memoria insuficiente");
            d = mp_get_double(m);
            if (propio) { mp_clear(m); free(m); }
        }
        return valor_decimal(d);
    }

    if (v->tipo == VAL_BOOLEANO) {
        if (n_args == 1) return valor_entero_de_i64(v->como.booleano ? 1 : 0);
        return valor_decimal(v->como.booleano ? 1.0 : 0.0);
    }

    if (v->tipo == VAL_DECIMAL) {
        double d = v->como.decimal;
        if (d != d) {  /* NaN */
            return error_nativa(err, linea, columna,
                "ErrorDeValor: no se puede redondear NaN");
        }
        if (n_args == 1) {
            /* Half-away-from-zero a entero. */
            double rounded = (d >= 0.0) ? (double)(int64_t)(d + 0.5)
                                         : (double)(int64_t)(d - 0.5);
            /* Rango chequeo: idéntico a `entero(decimal)`. */
            if (rounded > 9.2233720368547748e18 || rounded < -9.2233720368547758e18) {
                return error_nativa(err, linea, columna,
                    "ErrorDeValor: decimal fuera del rango de entero");
            }
            return valor_entero_de_i64((int64_t)rounded);
        }
        /* Con decimales: pow(10, k) escalado. */
        double escala = 1.0;
        for (int i = 0; i < decimales; i++) escala *= 10.0;
        double r = (d >= 0.0) ? ((double)(int64_t)(d * escala + 0.5)) / escala
                              : ((double)(int64_t)(d * escala - 0.5)) / escala;
        return valor_decimal(r);
    }

    return error_nativa(err, linea, columna,
        "ErrorDeTipo: redondear() no acepta '%s'", valor_nombre_tipo(v));
}

/* ──────────────────────────────────────────────────────────────────
 * Reflexión (v1.11): instancia_de, subclase_de, id, repr
 * ────────────────────────────────────────────────────────────────── */

/*
 * instancia_de(obj, clase) → booleano. Verdadero si `obj` es VAL_INSTANCIA
 * y su clase coincide con `clase` o es subclase de ella (vía la cadena
 * de superclases).
 *
 * Para tipos primitivos (entero, decimal, cadena, etc.), `obj` no es
 * VAL_INSTANCIA y por tanto retorna falso. Usar `tipo(x) == "entero"`
 * para chequear primitivos.
 *
 * `clase` debe ser VAL_CLASE; si no, ErrorDeTipo.
 */
static Valor nativa_instancia_de(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: instancia_de() requiere 2 argumentos, recibio %d", n_args);
    }
    if (args[1].tipo != VAL_CLASE) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: instancia_de() requiere una clase como segundo argumento, no '%s'",
            valor_nombre_tipo(&args[1]));
    }
    if (args[0].tipo != VAL_INSTANCIA) {
        return valor_booleano(false);
    }
    Clase *objetivo = args[1].como.clase;
    Clase *actual = args[0].como.instancia->clase;
    while (actual != NULL) {
        if (actual == objetivo) return valor_booleano(true);
        actual = actual->superclase;
    }
    return valor_booleano(false);
}

/*
 * subclase_de(A, B) → booleano. Verdadero si A == B o A hereda
 * (directa o indirectamente) de B. Ambos deben ser VAL_CLASE.
 */
static Valor nativa_subclase_de(EvalError *err, int n_args, Valor *args,
                                  int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: subclase_de() requiere 2 argumentos, recibio %d", n_args);
    }
    if (args[0].tipo != VAL_CLASE) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: subclase_de() requiere una clase como primer argumento, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    if (args[1].tipo != VAL_CLASE) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: subclase_de() requiere una clase como segundo argumento, no '%s'",
            valor_nombre_tipo(&args[1]));
    }
    Clase *objetivo = args[1].como.clase;
    Clase *actual = args[0].como.clase;
    while (actual != NULL) {
        if (actual == objetivo) return valor_booleano(true);
        actual = actual->superclase;
    }
    return valor_booleano(false);
}

/*
 * id(obj) → entero único de la identidad del objeto.
 *
 * Para tipos por referencia (lista, dicc, conjunto, tupla, instancia,
 * clase, closure, módulo, excepción, método ligado), retorna el puntero
 * al objeto convertido a entero. Dos referencias al mismo objeto dan
 * el mismo `id`. Tras GC el puntero puede reusarse — `id` solo es
 * estable durante la vida del objeto.
 *
 * Para tipos por valor (entero, decimal, booleano, cadena, nulo) NO
 * hay identidad estable: dos `5` distintos pueden tener punteros
 * distintos o iguales. Por simplicidad, hash el contenido para que
 * `id(x) == id(x)` sea siempre verdadero, pero `id(5) == id(5)` no
 * está garantizado a través de re-evaluaciones.
 *
 * Implementación pragmática: para por-valor, retornar 0 (todos iguales)
 * habría sido legal pero confuso; para por-ref retornar el puntero.
 * Hoy: retornamos el puntero del campo correspondiente, o un hash del
 * contenido para tipos inline.
 */
static Valor nativa_id(EvalError *err, int n_args, Valor *args,
                        int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: id() requiere 1 argumento, recibio %d", n_args);
    }
    const Valor *v = &args[0];
    int64_t pid = 0;
    switch (v->tipo) {
        case VAL_LISTA:         pid = (int64_t)(uintptr_t)v->como.lista; break;
        case VAL_DICCIONARIO:   pid = (int64_t)(uintptr_t)v->como.dicc; break;
        case VAL_CONJUNTO:      pid = (int64_t)(uintptr_t)v->como.conjunto; break;
        case VAL_TUPLA:         pid = (int64_t)(uintptr_t)v->como.tupla; break;
        case VAL_INSTANCIA:     pid = (int64_t)(uintptr_t)v->como.instancia; break;
        case VAL_CLASE:         pid = (int64_t)(uintptr_t)v->como.clase; break;
        case VAL_FUNCION_BC:    pid = (int64_t)(uintptr_t)v->como.closure; break;
        case VAL_PLANTILLA_BC:  pid = (int64_t)(uintptr_t)v->como.plantilla; break;
        case VAL_MODULO:        pid = (int64_t)(uintptr_t)v->como.modulo; break;
        case VAL_EXCEPCION:     pid = (int64_t)(uintptr_t)v->como.excepcion; break;
        case VAL_METODO_LIGADO: pid = (int64_t)(uintptr_t)v->como.metodo_ligado; break;
        case VAL_NATIVA:        pid = (int64_t)(uintptr_t)v->como.nativa.fn; break;
        case VAL_ENTERO:        pid = (int64_t)(uintptr_t)v->como.entero; break;
        case VAL_ENTERO_SMALL:  pid = (int64_t)v->como.entero_small; break;
        case VAL_DECIMAL: {
            /* Bit-pattern del double — id consistente para mismo valor. */
            int64_t bits;
            memcpy(&bits, &v->como.decimal, sizeof(bits));
            pid = bits;
            break;
        }
        case VAL_BOOLEANO:      pid = v->como.booleano ? 1 : 0; break;
        case VAL_CADENA:        pid = (int64_t)(uintptr_t)v->como.cadena.texto; break;
        case VAL_RANGO:         pid = (int64_t)(uintptr_t)v->como.rango.inicio; break;
        case VAL_FUNCION:       pid = (int64_t)(uintptr_t)v->como.funcion.def; break;
        case VAL_ITERADOR:      pid = (int64_t)(uintptr_t)v->como.iterador; break;
        case VAL_NULO:          pid = 0; break;
    }
    return valor_entero_de_i64(pid);
}

/*
 * repr(x) → cadena. Llama a `valor_a_repr` que añade comillas a las
 * cadenas y formatea anidadas con repr. Equivalente a `cadena(x)`
 * salvo en cadenas y dentro de listas (entonces sí se ve la diferencia).
 *
 * Útil para depuración y reportes: `imprimir(repr(s))` muestra la
 * cadena entre comillas, distinguiéndola de números o de listas.
 */
static Valor nativa_repr(EvalError *err, int n_args, Valor *args,
                          int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: repr() requiere 1 argumento, recibio %d", n_args);
    }
    char buffer[4096];
    int n = valor_a_repr(&args[0], buffer, sizeof(buffer));
    if (n < 0) n = 0;
    if (n >= (int)sizeof(buffer)) n = (int)sizeof(buffer) - 1;
    return valor_cadena_duplicar(buffer, n);
}

/* ──────────────────────────────────────────────────────────────────
 * Atributos dinamicos (v1.86): tiene_atributo, obtener_atributo,
 * asignar_atributo. Analogo a `hasattr`/`getattr`/`setattr` de Python.
 *
 * Para programacion dinamica: serializadores genericos, frameworks
 * de validacion, REPL helpers, etc.
 *
 * Tipos soportados:
 *   - VAL_INSTANCIA: atributos propios + metodos heredados de la clase.
 *   - VAL_CLASE: metodos de la clase.
 *   - VAL_MODULO: atributos del modulo (lo que se pueda importar).
 *   - Otros tipos: tiene_atributo() retorna falso silenciosamente;
 *     obtener_atributo() devuelve el defecto; asignar_atributo() lanza
 *     ErrorDeTipo.
 * ────────────────────────────────────────────────────────────────── */

/* Helper: chequea si `obj` tiene el atributo `nombre`. */
static bool valor_tiene_atributo(const Valor *obj, const Valor *nombre) {
    switch (obj->tipo) {
        case VAL_INSTANCIA: {
            Instancia *i = obj->como.instancia;
            if (dicc_contiene(i->atributos, nombre)) return true;
            if (i->clase && dicc_contiene(i->clase->metodos, nombre)) return true;
            return false;
        }
        case VAL_CLASE:
            return dicc_contiene(obj->como.clase->metodos, nombre);
        case VAL_MODULO:
            return dicc_contiene(obj->como.modulo->atributos, nombre);
        default:
            return false;
    }
}

static Valor nativa_tiene_atributo(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiene_atributo() requiere 2 argumentos (obj, nombre)");
    }
    if (args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiene_atributo() requiere cadena como segundo argumento, no '%s'",
            valor_nombre_tipo(&args[1]));
    }
    return valor_booleano(valor_tiene_atributo(&args[0], &args[1]));
}

/* obtener_atributo(obj, nombre, defecto=nulo) → valor o defecto.
 * NUNCA lanza ErrorDeAtributo — el defecto cubre el caso ausente. */
static Valor nativa_obtener_atributo(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args < 2 || n_args > 3) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: obtener_atributo() requiere 2 o 3 argumentos "
            "(obj, nombre, defecto=nulo)");
    }
    if (args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: obtener_atributo() requiere cadena como segundo argumento, no '%s'",
            valor_nombre_tipo(&args[1]));
    }
    Valor defecto = (n_args == 3) ? valor_clonar(&args[2]) : valor_nulo();
    const Valor *obj = &args[0];
    const Valor *nombre = &args[1];

    switch (obj->tipo) {
        case VAL_INSTANCIA: {
            Instancia *i = obj->como.instancia;
            Valor v;
            if (dicc_obtener(i->atributos, nombre, &v)) {
                valor_destruir(&defecto);
                return v;  /* dicc_obtener clona */
            }
            if (i->clase && dicc_obtener(i->clase->metodos, nombre, &v)) {
                valor_destruir(&defecto);
                /* Para coherencia con `obj.metodo`: si es una closure
                 * normal, envolver en MetodoLigado para que la
                 * invocacion inyecte `yo` automaticamente. Las
                 * propiedades NO se evaluan aqui (eso es lookup, no
                 * acceso) — solo se devuelven como tales. */
                if (v.tipo == VAL_FUNCION_BC) {
                    MetodoLigado *bm = metodo_ligado_nuevo(obj, v.como.closure);
                    valor_destruir(&v);
                    if (!bm) {
                        return error_nativa(err, linea, columna,
                            "memoria insuficiente al ligar metodo");
                    }
                    return valor_metodo_ligado(bm);
                }
                return v;
            }
            return defecto;
        }
        case VAL_CLASE: {
            Valor v;
            if (dicc_obtener(obj->como.clase->metodos, nombre, &v)) {
                valor_destruir(&defecto);
                return v;
            }
            return defecto;
        }
        case VAL_MODULO: {
            Valor v;
            if (dicc_obtener(obj->como.modulo->atributos, nombre, &v)) {
                valor_destruir(&defecto);
                return v;
            }
            return defecto;
        }
        default:
            return defecto;
    }
}

/* ──────────────────────────────────────────────────────────────────
 * Inspeccion / reflexion (v1.91). Helpers de bajo nivel que exponen
 * info estructural del runtime. Wrapper de mas alto nivel en
 * stdlib/inspeccion.cor.
 * ────────────────────────────────────────────────────────────────── */

/* Helper: extrae las claves (cadena) de un Diccionario a una Lista. */
static Lista *claves_a_lista(const Diccionario *d) {
    if (!d) return lista_nueva(0);
    Lista *l = lista_nueva(d->cuenta);
    if (!l) return NULL;
    for (int i = 0; i < d->capacidad; i++) {
        if (!d->entradas[i].ocupada) continue;
        if (l->cuenta >= l->capacidad) {
            int nueva_cap = l->capacidad == 0 ? 4 : l->capacidad * 2;
            Valor *nueva = (Valor *)realloc(l->elementos, sizeof(Valor) * (size_t)nueva_cap);
            if (!nueva) { lista_liberar(l); return NULL; }
            l->elementos = nueva;
            l->capacidad = nueva_cap;
        }
        l->elementos[l->cuenta++] = valor_clonar(&d->entradas[i].clave);
    }
    return l;
}

/* clase_de(instancia) → la clase (VAL_CLASE), o nulo si no es instancia. */
static Valor nativa_clase_de(EvalError *err, int n_args, Valor *args,
                               int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: clase_de() requiere 1 argumento");
    }
    if (args[0].tipo != VAL_INSTANCIA) {
        return valor_nulo();
    }
    Clase *c = args[0].como.instancia->clase;
    clase_retener(c);
    return valor_clase(c);
}

/* nombre_clase(clase_o_instancia) → cadena con el nombre exacto.
 * Para VAL_INSTANCIA devuelve el nombre de su clase (e.g. "Persona"
 * en lugar de "instancia"). Para VAL_CLASE devuelve el nombre.
 * Para otros tipos lanza ErrorDeTipo. */
static Valor nativa_nombre_clase(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: nombre_clase() requiere 1 argumento");
    }
    const Clase *c = NULL;
    if (args[0].tipo == VAL_INSTANCIA) {
        c = args[0].como.instancia->clase;
    } else if (args[0].tipo == VAL_CLASE) {
        c = args[0].como.clase;
    } else {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: nombre_clase() requiere instancia o clase, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    if (!c || !c->nombre) {
        return valor_cadena_duplicar("?", 1);
    }
    return valor_cadena_duplicar(c->nombre, c->longitud_nombre);
}

/* metodos_de(clase) → lista de cadenas con los nombres de metodos
 * (incluye metodos heredados copiados a clase.metodos por OP_HEREDAR). */
static Valor nativa_metodos_de(EvalError *err, int n_args, Valor *args,
                                 int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: metodos_de() requiere 1 argumento");
    }
    const Clase *c = NULL;
    if (args[0].tipo == VAL_CLASE) {
        c = args[0].como.clase;
    } else if (args[0].tipo == VAL_INSTANCIA) {
        c = args[0].como.instancia->clase;
    } else {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: metodos_de() requiere instancia o clase, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Lista *l = claves_a_lista(c ? c->metodos : NULL);
    if (!l) return error_nativa(err, linea, columna, "memoria insuficiente");
    return valor_lista(l);
}

/* atributos_de(instancia) → lista de cadenas con los nombres de los
 * atributos propios (NO incluye metodos heredados de la clase). */
static Valor nativa_atributos_de(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: atributos_de() requiere 1 argumento");
    }
    if (args[0].tipo != VAL_INSTANCIA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: atributos_de() requiere instancia, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Lista *l = claves_a_lista(args[0].como.instancia->atributos);
    if (!l) return error_nativa(err, linea, columna, "memoria insuficiente");
    return valor_lista(l);
}

/* asignar_atributo(obj, nombre, valor) → nulo. Muta la instancia.
 * Solo soporta VAL_INSTANCIA. */
static Valor nativa_asignar_atributo(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 3) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: asignar_atributo() requiere 3 argumentos (obj, nombre, valor)");
    }
    if (args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: asignar_atributo() requiere cadena como segundo argumento, no '%s'",
            valor_nombre_tipo(&args[1]));
    }
    if (args[0].tipo != VAL_INSTANCIA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: asignar_atributo() solo soporta instancias, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Valor clave = valor_clonar(&args[1]);
    Valor valor = valor_clonar(&args[2]);
    if (!dicc_asignar(args[0].como.instancia->atributos, clave, valor)) {
        return error_nativa(err, linea, columna,
            "memoria insuficiente al asignar atributo");
    }
    return valor_nulo();
}

/* ──────────────────────────────────────────────────────────────────
 * Tiempo (v1.19): tiempo_actual, tiempo_descomponer, tiempo_componer,
 * tiempo_formato.
 *
 * Trabajan con segundos Unix epoch (1970-01-01 00:00:00 UTC). Las
 * funciones de descomposición/composición usan local time (zona del
 * sistema). Para UTC los usuarios deben restar/sumar el offset
 * manualmente (futuro: opcional arg `utc=verdadero`).
 * ────────────────────────────────────────────────────────────────── */

static Valor nativa_tiempo_actual(EvalError *err, int n_args, Valor *args,
                                    int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiempo_actual() no acepta argumentos");
    }
    time_t t = time(NULL);
    if (t == (time_t)-1) {
        return error_nativa(err, linea, columna,
            "ErrorDeSistema: time() fallo");
    }
    return valor_entero_de_i64((int64_t)t);
}

/* Helper: crea tupla (año, mes, día, hora, min, seg, día_semana, día_año).
   día_semana: 0=lunes ... 6=domingo (ISO).
   día_año: 1-366. */
static Valor tupla_de_tm(struct tm *tm) {
    Tupla *t = tupla_nueva(8);
    if (!t) return valor_nulo();
    /* C struct tm: wday: 0=sunday..6=saturday; yday: 0-365.
       Convertimos a ISO: wday 0=lunes..6=domingo; yday 1-366. */
    int wday_iso = (tm->tm_wday + 6) % 7;
    t->elementos[0] = valor_entero_de_i64((int64_t)(tm->tm_year + 1900));
    t->elementos[1] = valor_entero_de_i64((int64_t)(tm->tm_mon + 1));
    t->elementos[2] = valor_entero_de_i64((int64_t)tm->tm_mday);
    t->elementos[3] = valor_entero_de_i64((int64_t)tm->tm_hour);
    t->elementos[4] = valor_entero_de_i64((int64_t)tm->tm_min);
    t->elementos[5] = valor_entero_de_i64((int64_t)tm->tm_sec);
    t->elementos[6] = valor_entero_de_i64((int64_t)wday_iso);
    t->elementos[7] = valor_entero_de_i64((int64_t)(tm->tm_yday + 1));
    return valor_tupla(t);
}

static Valor nativa_tiempo_descomponer(EvalError *err, int n_args, Valor *args,
                                         int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiempo_descomponer() requiere 1 argumento (ts entero)");
    }
    int64_t ts;
    if (!valor_entero_a_i64(&args[0], &ts)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiempo_descomponer() requiere un entero");
    }
    time_t t = (time_t)ts;
    struct tm tm_local;
#if defined(_WIN32) || defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
    if (localtime_s(&tm_local, &t) != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: timestamp invalido para localtime");
    }
#else
    struct tm *tm_ptr = localtime_r(&t, &tm_local);
    if (!tm_ptr) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: timestamp invalido para localtime");
    }
#endif
    Valor r = tupla_de_tm(&tm_local);
    if (r.tipo == VAL_NULO) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    return r;
}

static Valor nativa_tiempo_componer(EvalError *err, int n_args, Valor *args,
                                      int linea, int columna) {
    if (n_args != 6) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiempo_componer() requiere 6 argumentos (año, mes, día, hora, min, seg)");
    }
    int64_t comps[6];
    for (int i = 0; i < 6; i++) {
        if (!valor_entero_a_i64(&args[i], &comps[i])) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: tiempo_componer() requiere enteros en todos los args");
        }
    }
    struct tm tm_in;
    memset(&tm_in, 0, sizeof(tm_in));
    tm_in.tm_year = (int)comps[0] - 1900;
    tm_in.tm_mon  = (int)comps[1] - 1;
    tm_in.tm_mday = (int)comps[2];
    tm_in.tm_hour = (int)comps[3];
    tm_in.tm_min  = (int)comps[4];
    tm_in.tm_sec  = (int)comps[5];
    tm_in.tm_isdst = -1;   /* deja a mktime decidir DST */
    time_t t = mktime(&tm_in);
    if (t == (time_t)-1) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: componentes de tiempo invalidos");
    }
    return valor_entero_de_i64((int64_t)t);
}

static Valor nativa_tiempo_formato(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiempo_formato() requiere 2 argumentos (ts, formato)");
    }
    int64_t ts;
    if (!valor_entero_a_i64(&args[0], &ts)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiempo_formato() requiere entero como primer argumento");
    }
    if (args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiempo_formato() requiere cadena como segundo argumento");
    }
    /* Asegurar terminación nul para strftime. */
    int flen = args[1].como.cadena.longitud;
    char fmt[256];
    if (flen >= (int)sizeof(fmt)) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: formato de tiempo demasiado largo (>%d)",
            (int)sizeof(fmt) - 1);
    }
    memcpy(fmt, args[1].como.cadena.texto, (size_t)flen);
    fmt[flen] = '\0';

    time_t t = (time_t)ts;
    struct tm tm_local;
#if defined(_WIN32) || defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
    if (localtime_s(&tm_local, &t) != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: timestamp invalido");
    }
#else
    struct tm *tm_ptr = localtime_r(&t, &tm_local);
    if (!tm_ptr) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: timestamp invalido");
    }
#endif
    char buf[1024];
    size_t n = strftime(buf, sizeof(buf), fmt, &tm_local);
    if (n == 0 && flen > 0) {
        /* strftime devuelve 0 si el buffer es insuficiente O si el
           formato produce cadena vacía. Distinguir: si el formato no
           es vacío, asumimos overflow. */
        return error_nativa(err, linea, columna,
            "ErrorDeValor: salida de formato excede %d bytes", (int)sizeof(buf) - 1);
    }
    return valor_cadena_duplicar(buf, (int)n);
}

/* ──────────────────────────────────────────────────────────────────
 * @propiedad (v1.78). Envuelve una funcion como getter.
 * El usuario hace:
 *
 *   @propiedad
 *   funcion area(yo):
 *       retornar yo.ancho * yo.alto
 *   fin funcion
 *
 * que desugar a `area = propiedad(area)`, dejando un VAL_PROPIEDAD
 * que OP_METODO guarda en clase.metodos. Al acceder `obj.area` el
 * opcode OP_OBTENER_ATRIBUTO_INSTANCIA invoca el getter con `yo`.
 * ────────────────────────────────────────────────────────────────── */

static Valor nativa_propiedad(EvalError *err, int n_args, Valor *args,
                                int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: propiedad() requiere 1 argumento (callable)");
    }
    if (args[0].tipo != VAL_FUNCION_BC) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: propiedad() espera una funcion, recibio '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Propiedad *p = propiedad_nueva(args[0].como.closure);
    if (!p) {
        return error_nativa(err, linea, columna,
            "memoria insuficiente al crear propiedad");
    }
    return valor_propiedad(p);
}

/* v1.84: `@estaticometodo` — marca una funcion para que cuando se
 * acceda via `Clase.X` o `instancia.X`, NO se le ligue el receptor.
 * El usuario hace:
 *
 *   clase Color:
 *       @estaticometodo
 *       funcion desde_hex(s):
 *           ...
 *       fin funcion
 *   fin clase
 *
 *   c = Color.desde_hex("#FF8800")   # invoca closure desnuda
 *
 * desugar a `desde_hex = estaticometodo(desde_hex)`, lo cual produce
 * un VAL_METODO_ESTATICO que OP_METODO guarda en clase.metodos. Al
 * acceder, OP_OBTENER_ATRIBUTO devuelve la closure interna sin
 * envolver en MetodoLigado. */
static Valor nativa_estaticometodo(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: estaticometodo() requiere 1 argumento (callable)");
    }
    if (args[0].tipo != VAL_FUNCION_BC) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: estaticometodo() espera una funcion, recibio '%s'",
            valor_nombre_tipo(&args[0]));
    }
    MetodoEstatico *m = metodo_estatico_nuevo(args[0].como.closure);
    if (!m) {
        return error_nativa(err, linea, columna,
            "memoria insuficiente al crear estaticometodo");
    }
    return valor_metodo_estatico(m);
}

/* v1.85: `@clasemetodo` — marca una funcion para que cuando se acceda
 * via `Clase.X` o `instancia.X`, se liga a la clase como primer arg.
 * Patron Python `@classmethod`:
 *
 *   clase Foo:
 *       @clasemetodo
 *       funcion crear(cls, ...):
 *           retornar cls(...)   # polimorfico: si Bar hereda de Foo,
 *                                # Bar.crear() crea un Bar.
 *       fin funcion
 *   fin clase
 *
 * desugar a `crear = clasemetodo(crear)`, produciendo un
 * VAL_METODO_DE_CLASE que OP_METODO guarda en clase.metodos. Al
 * acceder, OP_OBTENER_ATRIBUTO crea un MetodoLigado con receptor =
 * valor_clase(la_clase), de modo que cuando se invoque la closure
 * recibe la clase como slot 0. */
static Valor nativa_clasemetodo(EvalError *err, int n_args, Valor *args,
                                  int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: clasemetodo() requiere 1 argumento (callable)");
    }
    if (args[0].tipo != VAL_FUNCION_BC) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: clasemetodo() espera una funcion, recibio '%s'",
            valor_nombre_tipo(&args[0]));
    }
    MetodoDeClase *m = metodo_de_clase_nuevo(args[0].como.closure);
    if (!m) {
        return error_nativa(err, linea, columna,
            "memoria insuficiente al crear clasemetodo");
    }
    return valor_metodo_de_clase(m);
}

/* ──────────────────────────────────────────────────────────────────
 * Tiempo monotónico + sleep + epoch_ms (v1.73).
 * Wrappers del stdlib `tiempo.cor`. `tiempo_actual` (segundos) ya
 * existe desde v1.19; aqui añadimos ms, monotonic y dormir.
 * ────────────────────────────────────────────────────────────────── */

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <errno.h>
#  include <time.h>
#endif

#include "profiler.h"  /* profiler_tiempo_ns reusa el reloj monotonico */

static Valor nativa_tiempo_epoch_ms(EvalError *err, int n_args, Valor *args,
                                      int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiempo_epoch_ms() no acepta argumentos");
    }
#ifdef _WIN32
    /* FILETIME es 100-ns desde 1601-01-01. Diferencia con epoch Unix:
       11644473600 segundos = 116444736000000000 unidades de 100-ns. */
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t ft100 = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    int64_t ms = (int64_t)((ft100 - 116444736000000000ULL) / 10000ULL);
    return valor_entero_de_i64(ms);
#else
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeSistema: clock_gettime(CLOCK_REALTIME) fallo");
    }
    int64_t ms = (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
    return valor_entero_de_i64(ms);
#endif
}

static Valor nativa_tiempo_monotonic(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiempo_monotonic() no acepta argumentos");
    }
    /* Reusa el reloj monotonico del profiler. */
    uint64_t ns = profiler_tiempo_ns();
    return valor_decimal((double)ns / 1e9);
}

static Valor nativa_tiempo_dormir(EvalError *err, int n_args, Valor *args,
                                    int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiempo_dormir() requiere 1 argumento (segundos)");
    }
    double s;
    if (args[0].tipo == VAL_DECIMAL) {
        s = args[0].como.decimal;
    } else if (args[0].tipo == VAL_ENTERO_SMALL) {
        s = (double)args[0].como.entero_small;
    } else if (args[0].tipo == VAL_ENTERO) {
        s = mp_get_double(args[0].como.entero);
    } else if (args[0].tipo == VAL_BOOLEANO) {
        s = args[0].como.booleano ? 1.0 : 0.0;
    } else {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tiempo_dormir() requiere segundos numericos");
    }
    /* Rechazar NaN / inf antes de castear a tipos enteros (cast de NaN
     * a DWORD/time_t es undefined behavior). */
    if (s != s || s > 1e15 || s < -1e15) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: tiempo_dormir() segundos invalidos (NaN, inf, o fuera de rango)");
    }
    if (s <= 0.0) return valor_nulo();
#ifdef _WIN32
    /* Sleep toma milisegundos. Para s muy grande clamp a UINT32_MAX-1. */
    double ms = s * 1000.0;
    if (ms > 4294967294.0) ms = 4294967294.0;
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (long)((s - (double)ts.tv_sec) * 1e9);
    if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
    /* Reintentar si interrumpido por signal (EINTR). Otros errores
     * (EINVAL, EFAULT) NO entran al loop — evitamos busy-spin si
     * nanosleep falla por argumentos invalidos. */
    while (nanosleep(&ts, &ts) == -1) {
        if (errno != EINTR) break;
    }
#endif
    return valor_nulo();
}

/* ──────────────────────────────────────────────────────────────────
 * Proceso (v1.27) — built-in primitivo. El módulo `proceso.cor` envuelve.
 * ────────────────────────────────────────────────────────────────── */

#include "proceso.h"

static Valor nativa_proceso_ejecutar(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: proceso_ejecutar(programa, argv) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: programa debe ser cadena");
    }
    if (args[1].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: argv debe ser lista");
    }
    /* Construir argv C: nombre_programa + lista cornamusa + NULL.
       Cada elemento debe ser cadena. */
    Lista *l = args[1].como.lista;
    int n_extra = l->cuenta;
    /* argv tendrá: programa, args[0..n_extra-1], NULL */
    int argv_sz = 1 + n_extra + 1;
    char **buf = (char **)malloc(sizeof(char *) * (size_t)argv_sz);
    if (!buf) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    /* Duplicar cada cadena para asegurar terminación NUL. */
    /* programa: */
    {
        const char *t = args[0].como.cadena.texto;
        int len = args[0].como.cadena.longitud;
        char *copia = (char *)malloc((size_t)len + 1);
        if (!copia) { free(buf); return error_nativa(err, linea, columna, "memoria insuficiente"); }
        memcpy(copia, t, (size_t)len);
        copia[len] = '\0';
        buf[0] = copia;
    }
    int construidos = 1;
    for (int i = 0; i < n_extra; i++) {
        if (l->elementos[i].tipo != VAL_CADENA) {
            for (int k = 0; k < construidos; k++) free(buf[k]);
            free(buf);
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: argv[%d] debe ser cadena (no '%s')",
                i, valor_nombre_tipo(&l->elementos[i]));
        }
        const char *t = l->elementos[i].como.cadena.texto;
        int len = l->elementos[i].como.cadena.longitud;
        char *copia = (char *)malloc((size_t)len + 1);
        if (!copia) {
            for (int k = 0; k < construidos; k++) free(buf[k]);
            free(buf);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        memcpy(copia, t, (size_t)len);
        copia[len] = '\0';
        buf[1 + i] = copia;
        construidos++;
    }
    buf[1 + n_extra] = NULL;

    ProcesoResultado pr;
    int rc = proceso_ejecutar_c(buf[0], (const char *const *)buf, &pr);

    /* Limpiar argv. */
    for (int i = 0; i < 1 + n_extra; i++) free(buf[i]);
    free(buf);

    if (rc != 0) {
        Valor v_err = error_nativa(err, linea, columna,
            "ErrorDeSistema: proceso_ejecutar fallo (%s)", pr.mensaje_error);
        free(pr.stdout_buf); free(pr.stderr_buf);
        return v_err;
    }

    /* Construir dict {stdout, stderr, codigo}. */
    Diccionario *d = dicc_nuevo();
    if (!d) {
        free(pr.stdout_buf); free(pr.stderr_buf);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    Valor k_out = valor_cadena_duplicar("stdout", 6);
    Valor v_out = valor_cadena_duplicar(pr.stdout_buf ? pr.stdout_buf : "",
                                          pr.stdout_len);
    dicc_asignar(d, k_out, v_out);
    Valor k_err = valor_cadena_duplicar("stderr", 6);
    Valor v_se = valor_cadena_duplicar(pr.stderr_buf ? pr.stderr_buf : "",
                                          pr.stderr_len);
    dicc_asignar(d, k_err, v_se);
    Valor k_cod = valor_cadena_duplicar("codigo", 6);
    Valor v_cod = valor_entero_de_i64((int64_t)pr.exit_codigo);
    dicc_asignar(d, k_cod, v_cod);

    free(pr.stdout_buf);
    free(pr.stderr_buf);
    return valor_diccionario(d);
}

/* ──────────────────────────────────────────────────────────────────
 * Red (v1.29) — HTTP cliente plano. El módulo `red.cor` envuelve.
 * ────────────────────────────────────────────────────────────────── */

#include "red.h"

static Valor nativa_red_http_obtener(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args < 1 || n_args > 3) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: red_http_obtener(url, [cabeceras_extra], [timeout]) requiere 1-3 args");
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: url debe ser cadena");
    }
    /* Duplicar URL para NUL-terminate. */
    int ulen = args[0].como.cadena.longitud;
    char *url = (char *)malloc((size_t)ulen + 1);
    if (!url) return error_nativa(err, linea, columna, "memoria insuficiente");
    memcpy(url, args[0].como.cadena.texto, (size_t)ulen);
    url[ulen] = '\0';

    /* Cabeceras extra opcional (cadena con CRLFs). */
    char *cabeceras = NULL;
    if (n_args >= 2 && args[1].tipo == VAL_CADENA) {
        int clen = args[1].como.cadena.longitud;
        cabeceras = (char *)malloc((size_t)clen + 1);
        if (!cabeceras) { free(url); return error_nativa(err, linea, columna, "memoria insuficiente"); }
        memcpy(cabeceras, args[1].como.cadena.texto, (size_t)clen);
        cabeceras[clen] = '\0';
    } else if (n_args >= 2 && args[1].tipo != VAL_NULO) {
        free(url);
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cabeceras_extra debe ser cadena o nulo");
    }
    int timeout_seg = 10;
    if (n_args >= 3) {
        int64_t t;
        if (!valor_entero_a_i64(&args[2], &t) || t <= 0 || t > 3600) {
            free(url); free(cabeceras);
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: timeout debe ser entero positivo (segundos)");
        }
        timeout_seg = (int)t;
    }

    RedHttpResultado r;
    int rc = red_http_obtener_c(url, cabeceras, timeout_seg, &r);
    free(url); free(cabeceras);
    if (rc != 0) {
        Valor v_err = error_nativa(err, linea, columna,
            "ErrorDeSistema: %s", r.mensaje_error);
        free(r.cuerpo); free(r.cabeceras_raw);
        return v_err;
    }

    /* Dict {codigo, cabeceras, cuerpo}. */
    Diccionario *d = dicc_nuevo();
    if (!d) {
        free(r.cuerpo); free(r.cabeceras_raw);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    dicc_asignar(d, valor_cadena_duplicar("codigo", 6),
                    valor_entero_de_i64((int64_t)r.codigo));
    dicc_asignar(d, valor_cadena_duplicar("cabeceras", 9),
                    valor_cadena_duplicar(r.cabeceras_raw ? r.cabeceras_raw : "",
                                            r.cabeceras_len));
    dicc_asignar(d, valor_cadena_duplicar("cuerpo", 6),
                    valor_cadena_duplicar(r.cuerpo ? r.cuerpo : "", r.cuerpo_len));
    free(r.cuerpo); free(r.cabeceras_raw);
    return valor_diccionario(d);
}

/* ──────────────────────────────────────────────────────────────────
 * Base64 (v1.59) — RFC 4648.
 *
 * Implementacion C de codificacion/decodificacion base64 estandar
 * (alfabeto A-Z a-z 0-9 + / con padding `=`). Variante URL-safe
 * (- _ sin padding) queda para v1.60+.
 *
 * Las funciones operan sobre las cadenas Cornamusa como secuencia
 * de bytes (la representacion UTF-8 subyacente). Esto permite
 * codificar tanto ASCII como datos binarios siempre que el caller
 * pueda representarlos como cadena valida (los bytes 0x00 se
 * permiten gracias a que las cadenas Cornamusa son length-prefixed).
 * ────────────────────────────────────────────────────────────────── */

static const char B64_ALFABETO[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* v1.66: alfabeto URL-safe segun RFC 4648 §5. Mismo orden, los dos
 * ultimos chars cambiados: `+/` → `-_`. Por convencion (JWT, etc.)
 * la variante URL-safe se emite SIN padding `=`. */
static const char B64_ALFABETO_URL[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/* Helper compartido por las dos variantes (estandar + URL-safe).
 * `alfabeto` apunta a los 64 chars del alfabeto. Si `con_padding`,
 * se rellena con `=` al final cuando la entrada no es multiplo de 3. */
static Valor base64_codificar_impl(EvalError *err, int linea, int columna,
                                    const Valor *arg, const char *alfabeto,
                                    bool con_padding) {
    if (arg->tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: base64 codificar requiere una cadena");
    }
    const unsigned char *in = (const unsigned char *)arg->como.cadena.texto;
    int n = arg->como.cadena.longitud;

    /* Output size: 4 chars por cada 3 bytes. Sin padding ahorra hasta 2 chars. */
    int out_n;
    int resto = n % 3;
    if (con_padding) {
        out_n = 4 * ((n + 2) / 3);
    } else {
        out_n = (n / 3) * 4 + (resto == 0 ? 0 : (resto + 1));
    }
    char *out = (char *)malloc((size_t)out_n + 1);
    if (!out) return error_nativa(err, linea, columna, "memoria insuficiente");

    int i, j;
    for (i = 0, j = 0; i + 3 <= n; i += 3, j += 4) {
        unsigned x = ((unsigned)in[i] << 16)
                    | ((unsigned)in[i + 1] << 8)
                    | (unsigned)in[i + 2];
        out[j]     = alfabeto[(x >> 18) & 0x3F];
        out[j + 1] = alfabeto[(x >> 12) & 0x3F];
        out[j + 2] = alfabeto[(x >> 6)  & 0x3F];
        out[j + 3] = alfabeto[x         & 0x3F];
    }
    /* Resto: 1 o 2 bytes. Padding opcional. */
    if (i < n) {
        int rest = n - i;
        unsigned x = (unsigned)in[i] << 16;
        if (rest == 2) x |= (unsigned)in[i + 1] << 8;
        out[j]     = alfabeto[(x >> 18) & 0x3F];
        out[j + 1] = alfabeto[(x >> 12) & 0x3F];
        if (rest == 2) {
            out[j + 2] = alfabeto[(x >> 6) & 0x3F];
            if (con_padding) { out[j + 3] = '='; j += 4; }
            else j += 3;
        } else {
            if (con_padding) {
                out[j + 2] = '=';
                out[j + 3] = '=';
                j += 4;
            } else {
                j += 2;
            }
        }
    }
    out[j] = '\0';

    Valor v = valor_cadena_duplicar(out, j);
    free(out);
    return v;
}

static Valor nativa_base64_codificar(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: base64_codificar(cadena) requiere 1 argumento, recibio %d",
            n_args);
    }
    return base64_codificar_impl(err, linea, columna, &args[0],
                                   B64_ALFABETO, /*con_padding=*/true);
}

/* v1.66: URL-safe base64 sin padding (RFC 4648 §5). */
static Valor nativa_base64_codificar_url(EvalError *err, int n_args, Valor *args,
                                           int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: base64_codificar_url(cadena) requiere 1 argumento, recibio %d",
            n_args);
    }
    return base64_codificar_impl(err, linea, columna, &args[0],
                                   B64_ALFABETO_URL, /*con_padding=*/false);
}

/* Lookup inverso: char base64 → valor 0-63, o -1 si invalido.
 * v1.66: acepta tanto el alfabeto estandar (+/) como URL-safe (-_)
 * — el decoder es tolerante a ambos, util cuando no sabes de
 * antemano que variante usaste. */
static int b64_decode_char(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

static Valor nativa_base64_decodificar(EvalError *err, int n_args, Valor *args,
                                         int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: base64_decodificar(cadena) requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: base64_decodificar() requiere una cadena");
    }
    const unsigned char *in = (const unsigned char *)args[0].como.cadena.texto;
    int n = args[0].como.cadena.longitud;

    /* Filtrar whitespace, contar caracteres validos. Permitimos `\n` y
     * espacios para tolerar entradas formateadas (MIME-style). */
    unsigned char *limpio = (unsigned char *)malloc((size_t)n + 1);
    if (!limpio) return error_nativa(err, linea, columna, "memoria insuficiente");
    int m = 0;
    int padding = 0;
    for (int k = 0; k < n; k++) {
        unsigned char c = in[k];
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        if (c == '=') { padding++; continue; }
        if (padding > 0) {
            /* Padding interno: solo se permite al final. */
            free(limpio);
            return error_nativa(err, linea, columna,
                "ErrorDeValor: base64 invalido (padding '=' en mitad de la cadena)");
        }
        if (b64_decode_char(c) < 0) {
            free(limpio);
            return error_nativa(err, linea, columna,
                "ErrorDeValor: caracter '%c' no es base64 valido", c);
        }
        limpio[m++] = c;
    }
    /* Padding debe ser 0, 1 o 2. */
    if (padding > 2) {
        free(limpio);
        return error_nativa(err, linea, columna,
            "ErrorDeValor: base64 invalido (demasiado padding)");
    }
    /* v1.66: si hay padding, total debe ser multiplo de 4 (clasico).
     * Si no hay padding (URL-safe), m debe terminar en 0, 2 o 3
     * caracteres dentro del ultimo bloque (1 char solitario es
     * imposible: codifica 6 bits sueltos sin sentido). */
    if (padding > 0) {
        if (((m + padding) % 4) != 0) {
            free(limpio);
            return error_nativa(err, linea, columna,
                "ErrorDeValor: longitud de base64 no es multiplo de 4 con padding");
        }
    } else {
        if ((m % 4) == 1) {
            free(limpio);
            return error_nativa(err, linea, columna,
                "ErrorDeValor: base64 sin padding termina en 1 char (incompleto)");
        }
    }

    int out_cap = (m / 4) * 3 + 3;
    char *out = (char *)malloc((size_t)out_cap);
    if (!out) { free(limpio); return error_nativa(err, linea, columna, "memoria insuficiente"); }
    int out_n = 0;

    int i = 0;
    while (i + 4 <= m) {
        unsigned x = ((unsigned)b64_decode_char(limpio[i])     << 18)
                    | ((unsigned)b64_decode_char(limpio[i + 1]) << 12)
                    | ((unsigned)b64_decode_char(limpio[i + 2]) << 6)
                    | (unsigned)b64_decode_char(limpio[i + 3]);
        out[out_n++] = (char)((x >> 16) & 0xFF);
        out[out_n++] = (char)((x >> 8)  & 0xFF);
        out[out_n++] = (char)(x         & 0xFF);
        i += 4;
    }
    /* Bloque parcial con padding (m % 4 == 2 o 3 implica padding 2 o 1). */
    int restantes = m - i;
    if (restantes == 2) {
        /* 2 chars base64 → 1 byte. */
        unsigned x = ((unsigned)b64_decode_char(limpio[i])     << 18)
                    | ((unsigned)b64_decode_char(limpio[i + 1]) << 12);
        out[out_n++] = (char)((x >> 16) & 0xFF);
    } else if (restantes == 3) {
        /* 3 chars base64 → 2 bytes. */
        unsigned x = ((unsigned)b64_decode_char(limpio[i])     << 18)
                    | ((unsigned)b64_decode_char(limpio[i + 1]) << 12)
                    | ((unsigned)b64_decode_char(limpio[i + 2]) << 6);
        out[out_n++] = (char)((x >> 16) & 0xFF);
        out[out_n++] = (char)((x >> 8)  & 0xFF);
    } else if (restantes == 1) {
        free(out); free(limpio);
        return error_nativa(err, linea, columna,
            "ErrorDeValor: base64 invalido (resto de 1 char incompleto)");
    }
    free(limpio);

    Valor v = valor_cadena_duplicar(out, out_n);
    free(out);
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Cadenas (v1.62): nativas para operaciones que en pure-Cornamusa
 * eran O(n^2) por la combinacion de `texto[i]` UTF-8 walk + concat
 * incremental. Caso por caso, miden 10-100x speedup respecto a las
 * versiones puras documentadas en stdlib/cadenas.cor.
 * ────────────────────────────────────────────────────────────────── */

static Valor nativa_cadena_indice_de(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_indice_de(s, sub) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_indice_de() requiere cadenas");
    }
    const char *s = args[0].como.cadena.texto;
    int sl = args[0].como.cadena.longitud;
    const char *sub = args[1].como.cadena.texto;
    int subl = args[1].como.cadena.longitud;
    if (subl == 0) return valor_entero_de_i64(0);
    if (subl > sl) return valor_entero_de_i64(-1);

    /* Naive substring search byte-a-byte. Para inputs tipicos
     * (cadenas cortas) es muy rapido. */
    int byte_pos = -1;
    int max_i = sl - subl;
    for (int i = 0; i <= max_i; i++) {
        if (s[i] == sub[0] && memcmp(s + i, sub, (size_t)subl) == 0) {
            byte_pos = i;
            break;
        }
    }
    if (byte_pos < 0) return valor_entero_de_i64(-1);

    /* Convertir byte_pos a indice de caracter. Para ASCII puro,
     * char_idx == byte_pos (loop sin iteraciones extra). */
    int char_idx = 0;
    int p = 0;
    while (p < byte_pos) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t cons = utf8proc_iterate(
            (const utf8proc_uint8_t *)(s + p), sl - p, &cp);
        if (cons <= 0) break;
        p += (int)cons;
        char_idx++;
    }
    return valor_entero_de_i64(char_idx);
}

static Valor nativa_cadena_empieza_con(EvalError *err, int n_args, Valor *args,
                                         int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_empieza_con(s, prefijo) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_empieza_con() requiere cadenas");
    }
    int sl = args[0].como.cadena.longitud;
    int pl = args[1].como.cadena.longitud;
    if (pl > sl) return valor_booleano(false);
    bool match = (memcmp(args[0].como.cadena.texto,
                          args[1].como.cadena.texto, (size_t)pl) == 0);
    return valor_booleano(match);
}

static Valor nativa_cadena_termina_con(EvalError *err, int n_args, Valor *args,
                                         int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_termina_con(s, sufijo) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_termina_con() requiere cadenas");
    }
    int sl = args[0].como.cadena.longitud;
    int suflen = args[1].como.cadena.longitud;
    if (suflen > sl) return valor_booleano(false);
    bool match = (memcmp(args[0].como.cadena.texto + (sl - suflen),
                          args[1].como.cadena.texto, (size_t)suflen) == 0);
    return valor_booleano(match);
}

static Valor cadena_caso_ascii(EvalError *err, Valor *arg, bool a_min,
                                int linea, int columna) {
    if (arg->tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: requiere una cadena");
    }
    const char *in = arg->como.cadena.texto;
    int len = arg->como.cadena.longitud;
    char *out = (char *)malloc((size_t)len + 1);
    if (!out) return error_nativa(err, linea, columna, "memoria insuficiente");
    for (int i = 0; i < len; i++) {
        unsigned char c = (unsigned char)in[i];
        if (a_min) {
            out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
        } else {
            out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : (char)c;
        }
    }
    out[len] = '\0';
    Valor v = valor_cadena_duplicar(out, len);
    free(out);
    return v;
}

static Valor nativa_cadena_minusculas_ascii(EvalError *err, int n_args, Valor *args,
                                              int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_minusculas_ascii(s) requiere 1 argumento");
    }
    return cadena_caso_ascii(err, &args[0], true, linea, columna);
}

static Valor nativa_cadena_mayusculas_ascii(EvalError *err, int n_args, Valor *args,
                                              int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_mayusculas_ascii(s) requiere 1 argumento");
    }
    return cadena_caso_ascii(err, &args[0], false, linea, columna);
}

/* ──────────────────────────────────────────────────────────────────
 * cadena_unir (v1.61): nativa O(n) para concatenar lista de cadenas.
 *
 * El `cadenas.unir` puro-Cornamusa hace `resultado += sep + parte[i]`
 * que es O(N^2) por la copia que cadena+cadena obliga. Mediado en el
 * benchmark csv_parse_1000 (1000 filas, 5 campos): 1000ms en pure
 * Cornamusa vs ~150ms con esta nativa.
 *
 * Acepta:
 *   - cadena_unir(lista_de_cadenas) → cadena con sep="" (concat).
 *   - cadena_unir(lista, sep)       → cadena con separador dado.
 * ────────────────────────────────────────────────────────────────── */

static Valor nativa_cadena_unir(EvalError *err, int n_args, Valor *args,
                                  int linea, int columna) {
    if (n_args < 1 || n_args > 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_unir(lista, [sep]) requiere 1 o 2 argumentos");
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_unir() requiere una lista");
    }
    const char *sep_txt = "";
    int sep_len = 0;
    if (n_args == 2) {
        if (args[1].tipo != VAL_CADENA) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: cadena_unir(_, sep) requiere sep cadena");
        }
        sep_txt = args[1].como.cadena.texto;
        sep_len = args[1].como.cadena.longitud;
    }

    Lista *l = args[0].como.lista;
    /* Validar tipos + sumar longitud total en una pasada. */
    long total = 0;
    for (int i = 0; i < l->cuenta; i++) {
        if (l->elementos[i].tipo != VAL_CADENA) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: elemento %d de la lista no es cadena ('%s')",
                i, valor_nombre_tipo(&l->elementos[i]));
        }
        total += l->elementos[i].como.cadena.longitud;
    }
    if (l->cuenta > 1) total += (long)sep_len * (l->cuenta - 1);

    char *out = (char *)malloc((size_t)total + 1);
    if (!out) return error_nativa(err, linea, columna, "memoria insuficiente");
    long pos = 0;
    for (int i = 0; i < l->cuenta; i++) {
        if (i > 0 && sep_len > 0) {
            memcpy(out + pos, sep_txt, (size_t)sep_len);
            pos += sep_len;
        }
        int ll = l->elementos[i].como.cadena.longitud;
        memcpy(out + pos, l->elementos[i].como.cadena.texto, (size_t)ll);
        pos += ll;
    }
    out[pos] = '\0';
    Valor v = valor_cadena_duplicar(out, (int)pos);
    free(out);
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Hashing (v1.60) — motor en src/hashing.c.
 *
 * Wrappers de SHA-256 y MD5. Ambos toman una cadena (los bytes
 * UTF-8 subyacentes) y devuelven el digest como cadena hexadecimal
 * en minusculas.
 * ────────────────────────────────────────────────────────────────── */

#include "hashing.h"

static Valor nativa_sha256(EvalError *err, int n_args, Valor *args,
                             int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: sha256(cadena) requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: sha256() requiere una cadena");
    }
    char hex[65];
    hashing_sha256_hex((const uint8_t *)args[0].como.cadena.texto,
                         (size_t)args[0].como.cadena.longitud, hex);
    return valor_cadena_duplicar(hex, 64);
}

static Valor nativa_md5(EvalError *err, int n_args, Valor *args,
                          int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: md5(cadena) requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: md5() requiere una cadena");
    }
    char hex[33];
    hashing_md5_hex((const uint8_t *)args[0].como.cadena.texto,
                      (size_t)args[0].como.cadena.longitud, hex);
    return valor_cadena_duplicar(hex, 32);
}

static Valor nativa_hmac_sha256(EvalError *err, int n_args, Valor *args,
                                  int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: hmac_sha256(clave, mensaje) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: hmac_sha256() requiere cadenas (clave, mensaje)");
    }
    char hex[65];
    hashing_hmac_sha256_hex(
        (const uint8_t *)args[0].como.cadena.texto, (size_t)args[0].como.cadena.longitud,
        (const uint8_t *)args[1].como.cadena.texto, (size_t)args[1].como.cadena.longitud,
        hex);
    return valor_cadena_duplicar(hex, 64);
}

/* v1.67: HMAC-SHA-256 que devuelve los 32 bytes raw (no hex) como
 * cadena Cornamusa. Necesario para JWT que debe codificar la firma
 * con base64-url; con hex el JWT seria invalido. */
static Valor nativa_hmac_sha256_bytes(EvalError *err, int n_args, Valor *args,
                                        int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: hmac_sha256_bytes(clave, mensaje) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: hmac_sha256_bytes() requiere cadenas");
    }
    uint8_t bytes[32];
    hashing_hmac_sha256_bytes(
        (const uint8_t *)args[0].como.cadena.texto, (size_t)args[0].como.cadena.longitud,
        (const uint8_t *)args[1].como.cadena.texto, (size_t)args[1].como.cadena.longitud,
        bytes);
    return valor_cadena_duplicar((const char *)bytes, 32);
}

static Valor nativa_hmac_md5(EvalError *err, int n_args, Valor *args,
                               int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: hmac_md5(clave, mensaje) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: hmac_md5() requiere cadenas (clave, mensaje)");
    }
    char hex[33];
    hashing_hmac_md5_hex(
        (const uint8_t *)args[0].como.cadena.texto, (size_t)args[0].como.cadena.longitud,
        (const uint8_t *)args[1].como.cadena.texto, (size_t)args[1].como.cadena.longitud,
        hex);
    return valor_cadena_duplicar(hex, 32);
}

/* ──────────────────────────────────────────────────────────────────
 * Regex (v1.28) — motor en src/regex.c. El módulo `regex.cor` envuelve.
 * ────────────────────────────────────────────────────────────────── */

#include "regex.h"

static Valor nativa_regex_coincide(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: regex_coincide(patron, texto) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: regex_coincide() requiere cadenas");
    }
    /* Asegurar terminación NUL en patrón. */
    int plen = args[0].como.cadena.longitud;
    char *patron = (char *)malloc((size_t)plen + 1);
    if (!patron) return error_nativa(err, linea, columna, "memoria insuficiente");
    memcpy(patron, args[0].como.cadena.texto, (size_t)plen);
    patron[plen] = '\0';
    char err_buf[256] = {0};
    int fin = 0;
    int texto_len = args[1].como.cadena.longitud;
    bool ok = regex_coincidir(patron,
                                args[1].como.cadena.texto,
                                texto_len,
                                &fin, err_buf, sizeof(err_buf));
    free(patron);
    if (!ok && err_buf[0]) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: patron regex invalido: %s", err_buf);
    }
    /* Cornamusa: `regex_coincide` exige fullmatch (consumir todo el
       texto). Es lo más intuitivo y consistente con Python `re.fullmatch`. */
    return valor_booleano(ok && fin == texto_len);
}

static Valor nativa_regex_buscar(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: regex_buscar(patron, texto) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: regex_buscar() requiere cadenas");
    }
    int plen = args[0].como.cadena.longitud;
    char *patron = (char *)malloc((size_t)plen + 1);
    if (!patron) return error_nativa(err, linea, columna, "memoria insuficiente");
    memcpy(patron, args[0].como.cadena.texto, (size_t)plen);
    patron[plen] = '\0';
    char err_buf[256] = {0};
    int inicio = -1, fin = -1;
    bool ok = regex_buscar(patron,
                              args[1].como.cadena.texto,
                              args[1].como.cadena.longitud,
                              &inicio, &fin, err_buf, sizeof(err_buf));
    free(patron);
    if (!ok && err_buf[0]) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: patron regex invalido: %s", err_buf);
    }
    if (!ok) return valor_nulo();
    /* Retornar tupla (inicio, fin). */
    Tupla *t = tupla_nueva(2);
    if (!t) return error_nativa(err, linea, columna, "memoria insuficiente");
    t->elementos[0] = valor_entero_de_i64((int64_t)inicio);
    t->elementos[1] = valor_entero_de_i64((int64_t)fin);
    return valor_tupla(t);
}

/* Callback para regex_todos: añade par (inicio, fin) a una lista. */
typedef struct {
    Lista *lista;
    const char *texto;
} TodosCtx;

static bool todos_callback(int inicio, int fin, void *datos) {
    TodosCtx *ctx = (TodosCtx *)datos;
    /* Empujar subcadena texto[inicio..fin] como elemento. */
    int sublen = fin - inicio;
    Valor sub = valor_cadena_duplicar(ctx->texto + inicio, sublen);
    lista_agregar(ctx->lista, sub);
    return true;
}

static Valor nativa_regex_todos(EvalError *err, int n_args, Valor *args,
                                  int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: regex_todos(patron, texto) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: regex_todos() requiere cadenas");
    }
    int plen = args[0].como.cadena.longitud;
    char *patron = (char *)malloc((size_t)plen + 1);
    if (!patron) return error_nativa(err, linea, columna, "memoria insuficiente");
    memcpy(patron, args[0].como.cadena.texto, (size_t)plen);
    patron[plen] = '\0';
    Lista *l = lista_nueva(0);
    if (!l) { free(patron); return error_nativa(err, linea, columna, "memoria insuficiente"); }
    TodosCtx ctx = { l, args[1].como.cadena.texto };
    char err_buf[256] = {0};
    int rc = regex_todos(patron,
                            args[1].como.cadena.texto,
                            args[1].como.cadena.longitud,
                            todos_callback, &ctx,
                            err_buf, sizeof(err_buf));
    free(patron);
    if (rc < 0) {
        lista_liberar(l);
        return error_nativa(err, linea, columna,
            "ErrorDeValor: patron regex invalido: %s", err_buf);
    }
    return valor_lista(l);
}

/* Reemplaza ocurrencias del patrón con `rep` literal. NO soporta
   backreferences `\1` en v1.28.0 — el rep es literal. */
static Valor nativa_regex_reemplazar(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 3) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: regex_reemplazar(patron, texto, rep) requiere 3 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA
        || args[2].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: regex_reemplazar() requiere cadenas");
    }
    int plen = args[0].como.cadena.longitud;
    char *patron = (char *)malloc((size_t)plen + 1);
    if (!patron) return error_nativa(err, linea, columna, "memoria insuficiente");
    memcpy(patron, args[0].como.cadena.texto, (size_t)plen);
    patron[plen] = '\0';
    const char *texto = args[1].como.cadena.texto;
    int tlen = args[1].como.cadena.longitud;
    const char *rep = args[2].como.cadena.texto;
    int rlen = args[2].como.cadena.longitud;

    /* Estrategia: avanzar linealmente, buscar match desde i hasta el
       final, copiar texto previo + reemplazo + saltar al fin del match.
       Si match vacío, avanzar 1 carácter para evitar loop infinito. */
    size_t out_cap = (size_t)tlen + 1;
    size_t out_len = 0;
    char *out = (char *)malloc(out_cap);
    if (!out) { free(patron); return error_nativa(err, linea, columna, "memoria insuficiente"); }
    int i = 0;
    char err_buf[256] = {0};
    while (i <= tlen) {
        int ini = -1, fin = -1;
        bool encontrado = regex_buscar(patron, texto + i, tlen - i,
                                          &ini, &fin, err_buf, sizeof(err_buf));
        if (!encontrado && err_buf[0]) {
            free(out); free(patron);
            return error_nativa(err, linea, columna,
                "ErrorDeValor: patron regex invalido: %s", err_buf);
        }
        if (!encontrado) {
            /* Copiar resto y salir. */
            int restante = tlen - i;
            if (out_len + (size_t)restante > out_cap) {
                out_cap = out_len + (size_t)restante + 16;
                char *nuevo = (char *)realloc(out, out_cap);
                if (!nuevo) { free(out); free(patron); return error_nativa(err, linea, columna, "memoria insuficiente"); }
                out = nuevo;
            }
            memcpy(out + out_len, texto + i, (size_t)restante);
            out_len += (size_t)restante;
            break;
        }
        int real_ini = i + ini;
        int real_fin = i + fin;
        int prev_len = real_ini - i;
        /* Crecer buffer si hace falta para texto previo + reemplazo. */
        if (out_len + (size_t)prev_len + (size_t)rlen + 1 > out_cap) {
            while (out_len + (size_t)prev_len + (size_t)rlen + 1 > out_cap) out_cap *= 2;
            char *nuevo = (char *)realloc(out, out_cap);
            if (!nuevo) { free(out); free(patron); return error_nativa(err, linea, columna, "memoria insuficiente"); }
            out = nuevo;
        }
        memcpy(out + out_len, texto + i, (size_t)prev_len);
        out_len += (size_t)prev_len;
        memcpy(out + out_len, rep, (size_t)rlen);
        out_len += (size_t)rlen;
        if (real_fin == real_ini) {
            /* Match vacío: copiar 1 byte para no perderlo, avanzar 1. */
            if (real_ini < tlen) {
                if (out_len + 1 > out_cap) {
                    out_cap *= 2;
                    char *nuevo = (char *)realloc(out, out_cap);
                    if (!nuevo) { free(out); free(patron); return error_nativa(err, linea, columna, "memoria insuficiente"); }
                    out = nuevo;
                }
                out[out_len++] = texto[real_ini];
            }
            i = real_ini + 1;
        } else {
            i = real_fin;
        }
    }
    free(patron);
    Valor v = valor_cadena_duplicar(out, (int)out_len);
    free(out);
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Azar (v1.26) — built-ins primitivos. El módulo `azar.cor` los envuelve.
 * ────────────────────────────────────────────────────────────────── */

#include "azar.h"

static Valor nativa_azar_decimal(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: azar_decimal() no acepta argumentos");
    }
    return valor_decimal(azar_decimal());
}

static Valor nativa_azar_entero(EvalError *err, int n_args, Valor *args,
                                  int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: azar_entero(a, b) requiere 2 argumentos");
    }
    int64_t a, b;
    if (!valor_entero_a_i64(&args[0], &a) || !valor_entero_a_i64(&args[1], &b)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: azar_entero() requiere enteros");
    }
    if (a > b) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: azar_entero() requiere a <= b (recibio a=%lld, b=%lld)",
            (long long)a, (long long)b);
    }
    return valor_entero_de_i64(azar_entero_en(a, b));
}

static Valor nativa_azar_semilla(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: azar_semilla(n) requiere 1 argumento");
    }
    int64_t n;
    if (!valor_entero_a_i64(&args[0], &n)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: azar_semilla() requiere un entero");
    }
    azar_sembrar((uint64_t)n);
    return valor_nulo();
}

/* ──────────────────────────────────────────────────────────────────
 * Registro
 * ────────────────────────────────────────────────────────────────── */

/*
 * Lista canónica de nativas. Tanto `nativos_registrar` (Entorno) como
 * `nativos_registrar_dicc` (Diccionario) iteran esta tabla. Mantener
 * en un solo sitio garantiza que ambos motores ofrezcan exactamente
 * los mismos built-ins.
 */
typedef struct {
    const char *nombre;
    int longitud;
    FnNativa fn;
} EntradaNativa;

static const EntradaNativa NATIVAS[] = {
    {"imprimir", 8, nativa_imprimir},
    {"longitud", 8, nativa_longitud},
    {"tipo",     4, nativa_tipo},
    {"rango",    5, nativa_rango},
    /* Conversores (v1.1). */
    {"cadena",      6,  nativa_cadena},
    {"entero",      6,  nativa_entero},
    {"decimal",     7,  nativa_decimal},
    {"booleano",    8,  nativa_booleano},
    {"lista",       5,  nativa_lista},
    {"tupla",       5,  nativa_tupla},
    {"diccionario", 11, nativa_diccionario},
    {"leer",        4,  nativa_leer},
    {"agregar",  7, nativa_agregar},
    {"quitar",   6, nativa_quitar},
    {"insertar", 8, nativa_insertar},
    {"invertir", 8, nativa_invertir},
    {"ordenar",  7, nativa_ordenar},
    {"claves",   6, nativa_claves},
    {"valores",  7, nativa_valores},
    {"conjunto", 8, nativa_conjunto},
    /* Excepciones (v0.6.3). */
    {"Excepcion",       9,  nativa_exc_Excepcion},
    {"ErrorAritmetico", 15, nativa_exc_ErrorAritmetico},
    {"ErrorDeTipo",     11, nativa_exc_ErrorDeTipo},
    {"ErrorDeValor",    12, nativa_exc_ErrorDeValor},
    {"ErrorDeIndice",   13, nativa_exc_ErrorDeIndice},
    {"ErrorDeClave",    12, nativa_exc_ErrorDeClave},
    {"ErrorDeNombre",   13, nativa_exc_ErrorDeNombre},
    {"ErrorDeSistema",  14, nativa_exc_ErrorDeSistema},
    {"ErrorDeIO",       9,  nativa_exc_ErrorDeIO},
    {"ErrorDeIteracion", 16, nativa_exc_ErrorDeIteracion},
    /* GC manual (v0.8.1). */
    {"recolectar",      10, nativa_recolectar},
    /* Sistema (v0.9.2). */
    {"obtener_argv",    12, nativa_obtener_argv},
    {"salir",            5, nativa_salir},
    /* I/O de archivos (v1.8). */
    {"archivo_leer",     12, nativa_archivo_leer},
    {"archivo_escribir", 16, nativa_archivo_escribir},
    {"archivo_existe",   14, nativa_archivo_existe},
    {"archivo_lineas",   14, nativa_archivo_lineas},
    {"archivo_agregar",  15, nativa_archivo_agregar},
    /* JSON (v1.9). */
    {"json_parsear",     12, nativa_json_parsear},
    {"json_serializar",  15, nativa_json_serializar},
    /* Numéricos y reflexión (v1.11). */
    {"absoluto",         8,  nativa_absoluto},
    {"redondear",        9,  nativa_redondear},
    {"instancia_de",    12,  nativa_instancia_de},
    {"subclase_de",     11,  nativa_subclase_de},
    {"id",               2,  nativa_id},
    {"repr",             4,  nativa_repr},
    /* Atributos dinamicos (v1.86). */
    {"tiene_atributo",   14, nativa_tiene_atributo},
    {"obtener_atributo", 16, nativa_obtener_atributo},
    {"asignar_atributo", 16, nativa_asignar_atributo},
    /* Inspeccion / reflexion (v1.91). */
    {"clase_de",          8, nativa_clase_de},
    {"nombre_clase",     12, nativa_nombre_clase},
    {"metodos_de",       10, nativa_metodos_de},
    {"atributos_de",     12, nativa_atributos_de},
    /* Tiempo (v1.19). */
    {"tiempo_actual",       13, nativa_tiempo_actual},
    {"tiempo_descomponer",  18, nativa_tiempo_descomponer},
    {"tiempo_componer",     15, nativa_tiempo_componer},
    {"tiempo_formato",      14, nativa_tiempo_formato},
    /* Tiempo monotónico + sleep + epoch_ms (v1.73). */
    {"tiempo_epoch_ms",     15, nativa_tiempo_epoch_ms},
    {"tiempo_monotonic",    16, nativa_tiempo_monotonic},
    {"tiempo_dormir",       13, nativa_tiempo_dormir},
    /* @propiedad (v1.78). */
    {"propiedad",            9, nativa_propiedad},
    /* @estaticometodo (v1.84). */
    {"estaticometodo",      14, nativa_estaticometodo},
    /* @clasemetodo (v1.85). */
    {"clasemetodo",         11, nativa_clasemetodo},
    /* Azar (v1.26). */
    {"azar_decimal",        12, nativa_azar_decimal},
    {"azar_entero",         11, nativa_azar_entero},
    {"azar_semilla",        12, nativa_azar_semilla},
    /* Proceso (v1.27). */
    {"proceso_ejecutar",    16, nativa_proceso_ejecutar},
    /* Regex (v1.28). */
    {"regex_coincide",      14, nativa_regex_coincide},
    {"regex_buscar",        12, nativa_regex_buscar},
    {"regex_todos",         11, nativa_regex_todos},
    {"regex_reemplazar",    16, nativa_regex_reemplazar},
    /* Red (v1.29). */
    {"red_http_obtener",    16, nativa_red_http_obtener},
    /* Base64 (v1.59 estandar, v1.66 URL-safe). */
    {"base64_codificar",       16, nativa_base64_codificar},
    {"base64_decodificar",     18, nativa_base64_decodificar},
    {"base64_codificar_url",   20, nativa_base64_codificar_url},
    /* base64_decodificar_url no se necesita: nativa_base64_decodificar
     * acepta `-_` ademas de `+/`, y tolera entrada sin padding. */
    /* Hashing (v1.60, v1.65 HMAC). */
    {"hash_sha256",         11, nativa_sha256},
    {"hash_md5",             8, nativa_md5},
    {"hash_hmac_sha256",         16, nativa_hmac_sha256},
    {"hash_hmac_sha256_bytes",   22, nativa_hmac_sha256_bytes},
    {"hash_hmac_md5",            13, nativa_hmac_md5},
    /* Cadenas perf (v1.61): unir O(n). */
    {"cadena_unir",         11, nativa_cadena_unir},
    /* Cadenas perf (v1.62): otras nativas O(bytes). */
    {"cadena_indice_de",         16, nativa_cadena_indice_de},
    {"cadena_empieza_con",       18, nativa_cadena_empieza_con},
    {"cadena_termina_con",       18, nativa_cadena_termina_con},
    {"cadena_minusculas_ascii",  23, nativa_cadena_minusculas_ascii},
    {"cadena_mayusculas_ascii",  23, nativa_cadena_mayusculas_ascii},
};

#define N_NATIVAS (int)(sizeof(NATIVAS) / sizeof(NATIVAS[0]))

void nativos_registrar(Entorno *globales) {
    for (int i = 0; i < N_NATIVAS; i++) {
        entorno_definir(globales, NATIVAS[i].nombre, NATIVAS[i].longitud,
            valor_nativa(NATIVAS[i].nombre, NATIVAS[i].fn));
    }
}

void nativos_registrar_dicc(Diccionario *globales) {
    for (int i = 0; i < N_NATIVAS; i++) {
        Valor clave = valor_cadena_referencia(NATIVAS[i].nombre,
                                                NATIVAS[i].longitud);
        Valor fn = valor_nativa(NATIVAS[i].nombre, NATIVAS[i].fn);
        dicc_asignar(globales, clave, fn);
    }
}
