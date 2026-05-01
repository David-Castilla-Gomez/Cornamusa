#include "nativos.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    /* Caso especial: dicc → dicc (copia entrada por entrada). */
    if (src->tipo == VAL_DICCIONARIO) {
        const Diccionario *origen = src->como.dicc;
        for (int i = 0; i < origen->capacidad; i++) {
            if (!origen->entradas[i].ocupada) continue;
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
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: quitar() requiere una lista, no '%s'",
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
    /* Si solo se pasa un argumento, es el mensaje y la clase se toma
       de `clase_default`. Si se pasan dos, son (clase, mensaje). */
    const char *cls = clase_default;
    int len_cls = (int)strlen(clase_default);
    const char *msg = "";
    int len_msg = 0;
    if (n_args == 1) {
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
            "ErrorDeTipo: %s() acepta 1 o 2 argumentos, recibio %d",
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
    for (int i = 0; i < d->capacidad; i++) {
        if (d->entradas[i].ocupada) {
            lista_agregar(l, valor_clonar(&d->entradas[i].clave));
        }
    }
    return valor_lista(l);
}

/* valores(dicc) → lista de valores. Mismo orden indeterminado que claves(). */
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
    for (int i = 0; i < d->capacidad; i++) {
        if (d->entradas[i].ocupada) {
            lista_agregar(l, valor_clonar(&d->entradas[i].valor));
        }
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
