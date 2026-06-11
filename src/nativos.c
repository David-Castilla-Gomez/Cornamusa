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

/* v1.148: como `imprimir` pero SIN salto de linea final. Util para
 * construir lineas por trozos (progreso interactivo, prompts) y
 * para CLIs que quieren manejar el formato. Separa args por
 * espacio igual que imprimir. */
static Valor nativa_escribir(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    (void)err; (void)linea; (void)columna;
    char buffer[1024];
    for (int i = 0; i < n_args; i++) {
        if (i > 0) fputc(' ', stdout);
        valor_a_cadena(&args[i], buffer, sizeof(buffer));
        fputs(buffer, stdout);
    }
    fflush(stdout);
    return valor_nulo();
}

/* v1.148: como `imprimir` pero a stderr en vez de stdout. Util para
 * separar mensajes de progreso/errores del resultado del programa
 * (idiomatico en CLIs). Anade salto de linea final. */
static Valor nativa_imprimir_error(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    (void)err; (void)linea; (void)columna;
    char buffer[1024];
    for (int i = 0; i < n_args; i++) {
        if (i > 0) fputc(' ', stderr);
        valor_a_cadena(&args[i], buffer, sizeof(buffer));
        fputs(buffer, stderr);
    }
    fputc('\n', stderr);
    fflush(stderr);
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
    if (n_args < 1 || n_args > 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: entero() requiere 1 o 2 argumentos, recibio %d", n_args);
    }
    const Valor *v = &args[0];

    /* v1.159: entero(cadena, base) parsea con base 2..36 o 0 (auto). */
    if (n_args == 2) {
        if (v->tipo != VAL_CADENA) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: entero(s, base) requiere cadena, no '%s'",
                valor_nombre_tipo(v));
        }
        int64_t base_i64;
        if (!valor_entero_a_i64(&args[1], &base_i64)) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: base debe ser entero, no '%s'",
                valor_nombre_tipo(&args[1]));
        }
        if (base_i64 != 0 && (base_i64 < 2 || base_i64 > 36)) {
            return error_nativa(err, linea, columna,
                "ErrorDeValor: base debe ser 0 o estar en [2, 36]");
        }
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
        /* Auto-detectar base por prefijo si base == 0. */
        int base = (int)base_i64;
        if (base == 0) {
            if (len >= 2 && txt[0] == '0'
                && (txt[1] == 'x' || txt[1] == 'X')) { base = 16; txt += 2; len -= 2; }
            else if (len >= 2 && txt[0] == '0'
                && (txt[1] == 'b' || txt[1] == 'B')) { base = 2;  txt += 2; len -= 2; }
            else if (len >= 2 && txt[0] == '0'
                && (txt[1] == 'o' || txt[1] == 'O')) { base = 8;  txt += 2; len -= 2; }
            else { base = 10; }
        } else if (len >= 2 && txt[0] == '0') {
            /* Si base explicita coincide con prefijo, saltarlo (Python compat). */
            if ((base == 16 && (txt[1] == 'x' || txt[1] == 'X'))
                || (base == 2 && (txt[1] == 'b' || txt[1] == 'B'))
                || (base == 8 && (txt[1] == 'o' || txt[1] == 'O'))) {
                txt += 2; len -= 2;
            }
        }
        if (len == 0) {
            return error_nativa(err, linea, columna,
                "ErrorDeValor: '%.*s' no es entero valido",
                v->como.cadena.longitud, v->como.cadena.texto);
        }
        /* Copiar a buffer null-terminado (mp_read_radix lo necesita) y
         * descartar guiones bajos. */
        char *buf = (char *)malloc((size_t)len + 1);
        if (!buf) return error_nativa(err, linea, columna, "memoria insuficiente");
        int w = 0;
        for (int i = 0; i < len; i++) {
            char c = txt[i];
            if (c == '_') continue;
            /* Validar caracter para la base. */
            int dig;
            if (c >= '0' && c <= '9') dig = c - '0';
            else if (c >= 'a' && c <= 'z') dig = c - 'a' + 10;
            else if (c >= 'A' && c <= 'Z') dig = c - 'A' + 10;
            else {
                free(buf);
                return error_nativa(err, linea, columna,
                    "ErrorDeValor: caracter '%c' invalido para base %d", c, base);
            }
            if (dig >= base) {
                free(buf);
                return error_nativa(err, linea, columna,
                    "ErrorDeValor: digito '%c' invalido para base %d", c, base);
            }
            buf[w++] = c;
        }
        buf[w] = '\0';
        if (w == 0) {
            free(buf);
            return error_nativa(err, linea, columna,
                "ErrorDeValor: '%.*s' no es entero valido",
                v->como.cadena.longitud, v->como.cadena.texto);
        }
        mp_int *m = (mp_int *)malloc(sizeof(mp_int));
        if (!m) { free(buf); return error_nativa(err, linea, columna, "memoria insuficiente"); }
        if (mp_init(m) != MP_OKAY) {
            free(buf); free(m);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        if (mp_read_radix(m, buf, base) != MP_OKAY) {
            mp_clear(m); free(m); free(buf);
            return error_nativa(err, linea, columna,
                "ErrorDeValor: '%.*s' no es entero valido en base %d",
                v->como.cadena.longitud, v->como.cadena.texto, base);
        }
        free(buf);
        if (negativo) mp_neg(m, m);
        return valor_entero_de_mp_normalizado(m);
    }

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

/* v1.159: helper compartido — entero → cadena en base con prefijo
 * Python-style. Acepta SMALL/BIG, decimales y booleanos rechazados.
 * `prefijo`: "0b", "0o", "0x" o "" si NULL. */
static Valor entero_a_radix_con_prefijo(EvalError *err, const Valor *v,
                                            int base, const char *prefijo,
                                            const char *nombre,
                                            int linea, int columna) {
    if (!valor_es_entero(v) && v->tipo != VAL_BOOLEANO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: %s() requiere entero, no '%s'",
            nombre, valor_nombre_tipo(v));
    }
    /* Convertir a mp_int para usar mp_to_radix. */
    mp_int m;
    if (mp_init(&m) != MP_OKAY) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    if (v->tipo == VAL_ENTERO_SMALL) mp_set_i64(&m, v->como.entero_small);
    else if (v->tipo == VAL_BOOLEANO) mp_set_l(&m, v->como.booleano ? 1 : 0);
    else mp_copy(v->como.entero, &m);

    bool negativo = mp_isneg(&m);
    if (negativo) mp_neg(&m, &m);

    int tam = 0;
    if (mp_radix_size(&m, base, &tam) != MP_OKAY) {
        mp_clear(&m);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    char *digitos = (char *)malloc((size_t)tam);
    if (!digitos) {
        mp_clear(&m);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    size_t escritos;
    if (mp_to_radix(&m, digitos, (size_t)tam, &escritos, base) != MP_OKAY) {
        free(digitos);
        mp_clear(&m);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    mp_clear(&m);
    int dig_len = (int)escritos - 1;  /* excluir null terminator */
    if (dig_len < 0) dig_len = 0;
    /* mp_to_radix devuelve dígitos en uppercase para hex (A-F). Python
     * usa lowercase en hex(), oct(), bin(). Normalizar. */
    for (int i = 0; i < dig_len; i++) {
        if (digitos[i] >= 'A' && digitos[i] <= 'Z') {
            digitos[i] = (char)(digitos[i] + ('a' - 'A'));
        }
    }
    int pref_len = prefijo ? (int)strlen(prefijo) : 0;
    int total = (negativo ? 1 : 0) + pref_len + dig_len;
    char *buf = (char *)malloc((size_t)total);
    if (!buf) {
        free(digitos);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    int w = 0;
    if (negativo) buf[w++] = '-';
    if (pref_len) { memcpy(buf + w, prefijo, (size_t)pref_len); w += pref_len; }
    memcpy(buf + w, digitos, (size_t)dig_len);
    w += dig_len;
    Valor r = valor_cadena_duplicar(buf, w);
    free(buf);
    free(digitos);
    return r;
}

/* v1.159: binario(n) → cadena en base 2 con prefijo "0b".
 * Paridad con Python bin(). Negativos: "-0b101". */
static Valor nativa_binario(EvalError *err, int n_args, Valor *args,
                                int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: binario() requiere 1 argumento");
    }
    return entero_a_radix_con_prefijo(err, &args[0], 2, "0b",
                                          "binario", linea, columna);
}

/* v1.159: hexadecimal(n) → cadena en base 16 con prefijo "0x".
 * Paridad con Python hex(). Letras a-f minusculas. */
static Valor nativa_hexadecimal(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: hexadecimal() requiere 1 argumento");
    }
    return entero_a_radix_con_prefijo(err, &args[0], 16, "0x",
                                          "hexadecimal", linea, columna);
}

/* v1.159: octal(n) → cadena en base 8 con prefijo "0o".
 * Paridad con Python oct(). */
static Valor nativa_octal(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: octal() requiere 1 argumento");
    }
    return entero_a_radix_con_prefijo(err, &args[0], 8, "0o",
                                          "octal", linea, columna);
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

/* v1.164: helper compartido — rechazar mutaciones sobre un conjunto
 * congelado (frozenset). Se inserta al inicio de cada mutador. */
static inline Valor error_conjunto_congelado(EvalError *err, int linea,
                                              int columna, const char *metodo) {
    return error_nativa(err, linea, columna,
        "ErrorDeTipo: %s() no se puede usar en un conjunto congelado",
        metodo);
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
        if (args[0].como.conjunto->congelado) {
            return error_conjunto_congelado(err, linea, columna, "agregar");
        }
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
        if (args[0].como.conjunto->congelado) {
            return error_conjunto_congelado(err, linea, columna, "quitar");
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

/* v1.160: inverso(iterable) — devuelve una nueva LISTA con los
 * elementos en orden inverso, sin mutar el original. Acepta lista,
 * tupla, cadena (cada code-point como cadena de 1 cp), rango y
 * conjunto. Paridad con `lista(reversed(it))` en Python.
 *
 * Para `lst.invertir()` (que muta) o `xs[::-1]` (slice) hay
 * alternativas; esta es la version idiomatica cuando quieres una
 * NUEVA lista y no te importa el tipo original. */
/* v1.192: enumerar(iterable, inicio=0) — builtin global. Devuelve
 * lista de tuplas (indice, elemento). Antes solo existia en
 * stdlib/funcionales (requeria importar). Es el idiom mas comun en
 * bucles `para i, x en enumerar(xs)`, asi que se promueve a builtin
 * como `rango`. Eager (lista materializada), igual que la version
 * stdlib.
 *
 * Soporta lista, tupla, cadena (code points), conjunto, dicc
 * (claves) y rango. */
static Valor nativa_enumerar(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args < 1 || n_args > 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: enumerar() requiere 1 o 2 argumentos, recibio %d",
            n_args);
    }
    int64_t inicio = 0;
    if (n_args == 2) {
        if (!valor_entero_a_i64(&args[1], &inicio)) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: el inicio de enumerar() debe ser entero");
        }
    }
    const Valor *it = &args[0];
    Lista *r = lista_nueva(0);
    if (!r) return error_nativa(err, linea, columna, "memoria insuficiente");

    #define EMPUJAR_PAR(elem_v) do { \
        Tupla *t = tupla_nueva(2); \
        if (!t) { \
            valor_destruir(&(elem_v)); \
            lista_liberar(r); \
            return error_nativa(err, linea, columna, "memoria insuficiente"); \
        } \
        t->elementos[0] = valor_entero_de_i64(inicio++); \
        t->elementos[1] = (elem_v); \
        if (!lista_agregar(r, valor_tupla(t))) { \
            tupla_liberar(t); \
            lista_liberar(r); \
            return error_nativa(err, linea, columna, "memoria insuficiente"); \
        } \
    } while (0)

    if (it->tipo == VAL_LISTA) {
        Lista *src = it->como.lista;
        for (int i = 0; i < src->cuenta; i++) {
            Valor e = valor_clonar(&src->elementos[i]);
            EMPUJAR_PAR(e);
        }
    } else if (it->tipo == VAL_TUPLA) {
        Tupla *src = it->como.tupla;
        for (int i = 0; i < src->cuenta; i++) {
            Valor e = valor_clonar(&src->elementos[i]);
            EMPUJAR_PAR(e);
        }
    } else if (it->tipo == VAL_CADENA) {
        const char *s = it->como.cadena.texto;
        int sl = it->como.cadena.longitud;
        int p = 0;
        while (p < sl) {
            utf8proc_int32_t cp;
            utf8proc_ssize_t cons = utf8proc_iterate(
                (const utf8proc_uint8_t *)(s + p), sl - p, &cp);
            if (cons <= 0) { p++; continue; }
            Valor e = valor_cadena_duplicar(s + p, (int)cons);
            EMPUJAR_PAR(e);
            p += (int)cons;
        }
    } else if (it->tipo == VAL_CONJUNTO) {
        Conjunto *c = it->como.conjunto;
        for (int i = 0; i < c->capacidad; i++) {
            if (!c->entradas[i].ocupada) continue;
            Valor e = valor_clonar(&c->entradas[i].elemento);
            EMPUJAR_PAR(e);
        }
    } else if (it->tipo == VAL_DICCIONARIO) {
        Diccionario *d = it->como.dicc;
        for (int idx = 0; idx < d->cuenta; idx++) {
            int slot = d->orden_insercion[idx];
            Valor e = valor_clonar(&d->entradas[slot].clave);
            EMPUJAR_PAR(e);
        }
    } else if (it->tipo == VAL_RANGO) {
        mp_int idx_mp, paso, fin;
        mp_init_multi(&idx_mp, &paso, &fin, NULL);
        mp_copy(it->como.rango.inicio, &idx_mp);
        mp_copy(it->como.rango.paso, &paso);
        mp_copy(it->como.rango.fin, &fin);
        int signo_paso = mp_isneg(&paso) == MP_YES ? -1 : 1;
        while (true) {
            int cmp = mp_cmp(&idx_mp, &fin);
            if (signo_paso > 0 ? cmp != MP_LT : cmp != MP_GT) break;
            mp_int *copia = (mp_int *)malloc(sizeof(mp_int));
            if (!copia) break;
            mp_init(copia);
            mp_copy(&idx_mp, copia);
            Valor e = valor_entero_de_mp_normalizado(copia);
            /* EMPUJAR_PAR con limpieza extra de los mp temporales. */
            Tupla *t = tupla_nueva(2);
            if (!t) {
                valor_destruir(&e);
                mp_clear_multi(&idx_mp, &paso, &fin, NULL);
                lista_liberar(r);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            t->elementos[0] = valor_entero_de_i64(inicio++);
            t->elementos[1] = e;
            if (!lista_agregar(r, valor_tupla(t))) {
                tupla_liberar(t);
                mp_clear_multi(&idx_mp, &paso, &fin, NULL);
                lista_liberar(r);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            mp_add(&idx_mp, &paso, &idx_mp);
        }
        mp_clear_multi(&idx_mp, &paso, &fin, NULL);
    } else {
        lista_liberar(r);
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: enumerar() no soporta '%s'",
            valor_nombre_tipo(it));
    }
    #undef EMPUJAR_PAR
    return valor_lista(r);
}

static Valor nativa_inverso(EvalError *err, int n_args, Valor *args,
                                 int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: inverso() requiere 1 argumento");
    }
    const Valor *it = &args[0];
    Lista *r = lista_nueva(0);
    if (!r) return error_nativa(err, linea, columna, "memoria insuficiente");

    if (it->tipo == VAL_LISTA) {
        Lista *src = it->como.lista;
        for (int i = src->cuenta - 1; i >= 0; i--) {
            if (!lista_agregar(r, valor_clonar(&src->elementos[i]))) {
                lista_liberar(r);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
        }
        return valor_lista(r);
    }
    if (it->tipo == VAL_TUPLA) {
        Tupla *t = it->como.tupla;
        for (int i = t->cuenta - 1; i >= 0; i--) {
            if (!lista_agregar(r, valor_clonar(&t->elementos[i]))) {
                lista_liberar(r);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
        }
        return valor_lista(r);
    }
    if (it->tipo == VAL_CADENA) {
        /* Recolectar los offsets de cada code-point primero, luego
         * iterar al reves. */
        const char *s = it->como.cadena.texto;
        int sl = it->como.cadena.longitud;
        /* Estimar capacidad: el peor caso son sl code-points. */
        int *offsets = (int *)malloc((size_t)(sl + 1) * sizeof(int));
        if (!offsets) {
            lista_liberar(r);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        int n_cp = 0;
        int p = 0;
        while (p < sl) {
            offsets[n_cp++] = p;
            utf8proc_int32_t cp;
            utf8proc_ssize_t cons = utf8proc_iterate(
                (const utf8proc_uint8_t *)(s + p), sl - p, &cp);
            p += (cons > 0) ? (int)cons : 1;
        }
        offsets[n_cp] = sl;
        for (int i = n_cp - 1; i >= 0; i--) {
            int inicio = offsets[i];
            int fin = offsets[i + 1];
            Valor v = valor_cadena_duplicar(s + inicio, fin - inicio);
            if (!lista_agregar(r, v)) {
                free(offsets);
                lista_liberar(r);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
        }
        free(offsets);
        return valor_lista(r);
    }
    if (it->tipo == VAL_CONJUNTO) {
        /* El orden del conjunto no es determinista, asi que "invertir"
         * no tiene una semantica fuerte. Para coherencia con extender(),
         * volcamos los elementos en orden inverso al de iteracion del
         * array de entradas (mismo orden que produce lista(conjunto)). */
        Conjunto *c = it->como.conjunto;
        /* Recolectar en orden de iteracion. */
        int n = 0;
        Valor *temp = (Valor *)malloc((size_t)c->cuenta * sizeof(Valor));
        if (!temp && c->cuenta > 0) {
            lista_liberar(r);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        for (int i = 0; i < c->capacidad; i++) {
            const EntradaConjunto *e = &c->entradas[i];
            if (!e->ocupada) continue;
            temp[n++] = valor_clonar(&e->elemento);
        }
        for (int i = n - 1; i >= 0; i--) {
            if (!lista_agregar(r, temp[i])) {
                /* Liberar los pendientes. */
                for (int k = 0; k <= i; k++) valor_destruir(&temp[k]);
                free(temp);
                lista_liberar(r);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
        }
        free(temp);
        return valor_lista(r);
    }
    /* Rango: requiere materializar. Mas simple via OP_ITER del lado
     * VM, pero como nativa C, recreamos: rango tiene inicio/fin/paso
     * estandar — pero hay que delegar a la API. */
    if (it->tipo == VAL_RANGO) {
        /* Materializar el rango usando los métodos publicos. La API
         * de Rango no es trivial — los enteros pueden ser bignum.
         * Lo más simple: iterar y agregar al reves. Pero no tenemos
         * acceso facil a la API de iteracion desde aqui. Por ahora,
         * rechazar con sugerencia clara. */
        lista_liberar(r);
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: inverso() no soporta rango directamente. "
            "Usa inverso(lista(rango(...))).");
    }
    lista_liberar(r);
    return error_nativa(err, linea, columna,
        "ErrorDeTipo: inverso() no acepta '%s'", valor_nombre_tipo(it));
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
    /* v1.145: tuplas y listas se comparan lexicograficamente. Recurre
     * en comparador_ordenar (sobre los elementos), igual que Python.
     * Si algun par mismo-indice es incomparable, marca g_ordenar_error
     * y aborta. */
    if (a->tipo == VAL_TUPLA && b->tipo == VAL_TUPLA) {
        Tupla *ta = a->como.tupla;
        Tupla *tb = b->como.tupla;
        int min = ta->cuenta < tb->cuenta ? ta->cuenta : tb->cuenta;
        for (int i = 0; i < min; i++) {
            int c = comparador_ordenar(&ta->elementos[i], &tb->elementos[i]);
            if (g_ordenar_error) return 0;
            if (c != 0) return c;
        }
        return ta->cuenta < tb->cuenta ? -1 : (ta->cuenta > tb->cuenta ? 1 : 0);
    }
    if (a->tipo == VAL_LISTA && b->tipo == VAL_LISTA) {
        Lista *la = a->como.lista;
        Lista *lb = b->como.lista;
        int min = la->cuenta < lb->cuenta ? la->cuenta : lb->cuenta;
        for (int i = 0; i < min; i++) {
            int c = comparador_ordenar(&la->elementos[i], &lb->elementos[i]);
            if (g_ordenar_error) return 0;
            if (c != 0) return c;
        }
        return la->cuenta < lb->cuenta ? -1 : (la->cuenta > lb->cuenta ? 1 : 0);
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
/* v1.109: usado cuando se accede o asigna a una propiedad que no
   permite la operacion (propiedad solo lectura sin setter, o atributo
   inexistente sobre un valor sin atributos). */
DEFINIR_EXC_NATIVA(ErrorDeAtributo)

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

/* v1.104: variables de entorno y directorio de inicio del usuario.
 *
 * Lectura via getenv() (estandar C). Escritura: setenv POSIX, _putenv_s
 * Windows. Listado completo via `environ` (POSIX) o `_environ` (Windows).
 *
 * `obtener_variable_entorno(nombre)` -> cadena o nulo si no existe.
 * `establecer_variable_entorno(nombre, valor)` -> nulo. valor=""
 *   normalmente no quita la variable; usa nulo para borrarla.
 * `variables_entorno()` -> dict {nombre: valor} de todas.
 * `directorio_inicio()` -> cadena con HOME (POSIX) o USERPROFILE
 *   (Windows). Separadores normalizados a "/".
 */
#ifdef _WIN32
extern char **_environ;
#define cor_environ _environ
#else
extern char **environ;
#define cor_environ environ
#endif

static Valor nativa_obtener_variable_entorno(EvalError *err, int n_args,
                                                Valor *args, int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: obtener_variable_entorno() requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: obtener_variable_entorno() requiere una cadena");
    }
    int len = args[0].como.cadena.longitud;
    char buf_stack[256];
    char *nombre = buf_stack;
    char *nombre_heap = NULL;
    if (len + 1 > (int)sizeof(buf_stack)) {
        nombre_heap = (char *)malloc((size_t)len + 1);
        if (!nombre_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        nombre = nombre_heap;
    }
    memcpy(nombre, args[0].como.cadena.texto, (size_t)len);
    nombre[len] = '\0';

    const char *v = getenv(nombre);
    if (nombre_heap) free(nombre_heap);
    if (v == NULL) return valor_nulo();
    return valor_cadena_duplicar(v, (int)strlen(v));
}

static Valor nativa_establecer_variable_entorno(EvalError *err, int n_args,
                                                   Valor *args, int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: establecer_variable_entorno() requiere 2 argumentos, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: establecer_variable_entorno(): nombre debe ser cadena");
    }
    if (args[1].tipo != VAL_CADENA && args[1].tipo != VAL_NULO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: establecer_variable_entorno(): valor debe ser cadena o nulo");
    }
    int nlen = args[0].como.cadena.longitud;
    char *nombre = (char *)malloc((size_t)nlen + 1);
    if (!nombre) return error_nativa(err, linea, columna, "memoria insuficiente");
    memcpy(nombre, args[0].como.cadena.texto, (size_t)nlen);
    nombre[nlen] = '\0';

    int rc;
    if (args[1].tipo == VAL_NULO) {
        /* Borrar la variable */
#ifdef _WIN32
        rc = _putenv_s(nombre, "");
#else
        rc = unsetenv(nombre);
#endif
    } else {
        int vlen = args[1].como.cadena.longitud;
        char *valor = (char *)malloc((size_t)vlen + 1);
        if (!valor) {
            free(nombre);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        memcpy(valor, args[1].como.cadena.texto, (size_t)vlen);
        valor[vlen] = '\0';
#ifdef _WIN32
        rc = _putenv_s(nombre, valor);
#else
        rc = setenv(nombre, valor, 1);
#endif
        free(valor);
    }
    free(nombre);
    if (rc != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeSistema: no se pudo establecer la variable de entorno");
    }
    return valor_nulo();
}

static Valor nativa_variables_entorno(EvalError *err, int n_args, Valor *args,
                                         int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: variables_entorno() no acepta argumentos");
    }
    Diccionario *d = dicc_nuevo();
    if (!d) return error_nativa(err, linea, columna, "memoria insuficiente");
    char **env = cor_environ;
    if (env == NULL) return valor_diccionario(d);
    for (int i = 0; env[i] != NULL; i++) {
        const char *entry = env[i];
        const char *eq = strchr(entry, '=');
        if (!eq) continue;
        int nlen = (int)(eq - entry);
        int vlen = (int)strlen(eq + 1);
        Valor k = valor_cadena_duplicar(entry, nlen);
        Valor v = valor_cadena_duplicar(eq + 1, vlen);
        if (k.tipo == VAL_NULO || v.tipo == VAL_NULO || !dicc_asignar(d, k, v)) {
            dicc_liberar(d);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
    }
    return valor_diccionario(d);
}

static Valor nativa_directorio_inicio(EvalError *err, int n_args, Valor *args,
                                         int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: directorio_inicio() no acepta argumentos");
    }
#ifdef _WIN32
    const char *h = getenv("USERPROFILE");
    if (!h) h = getenv("HOMEDRIVE");
#else
    const char *h = getenv("HOME");
#endif
    if (!h) {
        return error_nativa(err, linea, columna,
            "ErrorDeSistema: no se pudo determinar el directorio de inicio");
    }
    /* Normalizar separadores a "/" como hace obtener_cwd. */
    int len = (int)strlen(h);
    char *copia = (char *)malloc((size_t)len + 1);
    if (!copia) return error_nativa(err, linea, columna, "memoria insuficiente");
    for (int i = 0; i < len; i++) {
        copia[i] = (h[i] == '\\') ? '/' : h[i];
    }
    copia[len] = '\0';
    Valor v = valor_cadena_duplicar(copia, len);
    free(copia);
    return v;
}

/* v1.108: usuario_actual, hostname y directorio_temporal.
 *
 * usuario_actual() -> cadena con el nombre del usuario actual.
 *   POSIX: getenv("USER") con fallback a getenv("LOGNAME").
 *   Windows: getenv("USERNAME").
 *   Si nada esta definido, lanza ErrorDeSistema.
 *
 * hostname() -> cadena con el nombre de la maquina.
 *   POSIX: gethostname(buf, len).
 *   Windows: GetComputerNameA(buf, &len). Requiere <windows.h>.
 *
 * directorio_temporal() -> cadena con el directorio temporal del SO.
 *   POSIX: TMPDIR env -> "/tmp" -> ErrorDeSistema.
 *   Windows: TEMP env -> TMP env -> "C:/Windows/Temp".
 *   Separadores normalizados a "/".
 */
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static Valor nativa_usuario_actual(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: usuario_actual() no acepta argumentos");
    }
#ifdef _WIN32
    const char *u = getenv("USERNAME");
#else
    const char *u = getenv("USER");
    if (!u) u = getenv("LOGNAME");
#endif
    if (!u || u[0] == '\0') {
        return error_nativa(err, linea, columna,
            "ErrorDeSistema: no se pudo determinar el usuario actual");
    }
    return valor_cadena_duplicar(u, (int)strlen(u));
}

static Valor nativa_hostname(EvalError *err, int n_args, Valor *args,
                               int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: hostname() no acepta argumentos");
    }
#ifdef _WIN32
    char buf[256];
    DWORD len = sizeof(buf);
    if (!GetComputerNameA(buf, &len)) {
        return error_nativa(err, linea, columna,
            "ErrorDeSistema: no se pudo obtener el hostname");
    }
    return valor_cadena_duplicar(buf, (int)len);
#else
    char buf[256];
    if (gethostname(buf, sizeof(buf)) != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeSistema: no se pudo obtener el hostname");
    }
    buf[sizeof(buf) - 1] = '\0';  /* gethostname puede no terminar en NUL */
    return valor_cadena_duplicar(buf, (int)strlen(buf));
#endif
}

static Valor nativa_directorio_temporal(EvalError *err, int n_args, Valor *args,
                                          int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: directorio_temporal() no acepta argumentos");
    }
#ifdef _WIN32
    const char *t = getenv("TEMP");
    if (!t) t = getenv("TMP");
    if (!t) t = "C:/Windows/Temp";
#else
    const char *t = getenv("TMPDIR");
    if (!t) t = "/tmp";
#endif
    int len = (int)strlen(t);
    char *copia = (char *)malloc((size_t)len + 1);
    if (!copia) return error_nativa(err, linea, columna, "memoria insuficiente");
    for (int i = 0; i < len; i++) {
        copia[i] = (t[i] == '\\') ? '/' : t[i];
    }
    copia[len] = '\0';
    Valor v = valor_cadena_duplicar(copia, len);
    free(copia);
    return v;
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

/* ─── v1.97: operaciones de filesystem ─────────────────────────────
 *
 * archivo_es_directorio(ruta) → booleano (false si no existe).
 * directorio_listar(ruta) → lista de cadenas (entradas, sin "." ni "..").
 * obtener_cwd() → cadena con el directorio actual.
 * directorio_crear(ruta) → nulo. Lanza ErrorDeIO atrapable si falla
 *                          (no se trata como exito que ya existiera).
 *
 * Portabilidad: usamos sys/stat.h (universal en Win + POSIX) y dirent.h
 * para POSIX / FindFirstFile-W para Windows. mkdir / _mkdir; getcwd /
 * _getcwd. Errores se reportan via error_nativa (excepciones atrapables).
 */
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define cor_mkdir(p) _mkdir(p)
#define cor_getcwd  _getcwd
#else
#include <dirent.h>
#include <unistd.h>
#define cor_mkdir(p) mkdir((p), 0755)
#define cor_getcwd  getcwd
#endif

static Valor nativa_archivo_es_directorio(EvalError *err, int n_args, Valor *args,
                                            int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_es_directorio() requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_es_directorio() espera una cadena con la ruta");
    }
    int len_ruta = args[0].como.cadena.longitud;
    char buf_stack[1024];
    char *ruta = buf_stack;
    char *ruta_heap = NULL;
    if (len_ruta + 1 > (int)sizeof(buf_stack)) {
        ruta_heap = (char *)malloc((size_t)len_ruta + 1);
        if (!ruta_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        ruta = ruta_heap;
    }
    memcpy(ruta, args[0].como.cadena.texto, (size_t)len_ruta);
    ruta[len_ruta] = '\0';

#ifdef _WIN32
    struct _stat st;
    int rc = _stat(ruta, &st);
    bool es_dir = (rc == 0) && ((st.st_mode & _S_IFDIR) != 0);
#else
    struct stat st;
    int rc = stat(ruta, &st);
    bool es_dir = (rc == 0) && S_ISDIR(st.st_mode);
#endif
    if (ruta_heap) free(ruta_heap);
    return valor_booleano(es_dir);
}

static Valor nativa_directorio_listar(EvalError *err, int n_args, Valor *args,
                                        int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: directorio_listar() requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: directorio_listar() espera una cadena con la ruta");
    }
    int len_ruta = args[0].como.cadena.longitud;
    char buf_stack[1024];
    char *ruta = buf_stack;
    char *ruta_heap = NULL;
    if (len_ruta + 1 > (int)sizeof(buf_stack)) {
        ruta_heap = (char *)malloc((size_t)len_ruta + 1);
        if (!ruta_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        ruta = ruta_heap;
    }
    memcpy(ruta, args[0].como.cadena.texto, (size_t)len_ruta);
    ruta[len_ruta] = '\0';

    Lista *l = lista_nueva(0);
    if (!l) {
        if (ruta_heap) free(ruta_heap);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }

#ifdef _WIN32
    /* Patron: "ruta\*" para que FindFirstFile devuelva todas las entradas. */
    int pat_len = len_ruta + 3; /* "\*" + '\0' */
    char *patron = (char *)malloc((size_t)pat_len);
    if (!patron) {
        lista_liberar(l);
        if (ruta_heap) free(ruta_heap);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    memcpy(patron, ruta, (size_t)len_ruta);
    /* Anadir separador si no lo tiene */
    int p = len_ruta;
    if (p > 0 && ruta[p-1] != '\\' && ruta[p-1] != '/') {
        patron[p++] = '\\';
    }
    patron[p++] = '*';
    patron[p] = '\0';

    WIN32_FIND_DATAA datos;
    HANDLE h = FindFirstFileA(patron, &datos);
    free(patron);
    if (h == INVALID_HANDLE_VALUE) {
        lista_liberar(l);
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo listar '%s'", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
    do {
        const char *n = datos.cFileName;
        if (n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0'))) {
            continue; /* skip "." y ".." */
        }
        int nlen = (int)strlen(n);
        Valor v = valor_cadena_duplicar(n, nlen);
        if (v.tipo == VAL_NULO || !lista_agregar(l, v)) {
            FindClose(h);
            lista_liberar(l);
            if (ruta_heap) free(ruta_heap);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
    } while (FindNextFileA(h, &datos));
    FindClose(h);
#else
    DIR *d = opendir(ruta);
    if (!d) {
        lista_liberar(l);
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo listar '%s'", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *n = ent->d_name;
        if (n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0'))) {
            continue;
        }
        int nlen = (int)strlen(n);
        Valor v = valor_cadena_duplicar(n, nlen);
        if (v.tipo == VAL_NULO || !lista_agregar(l, v)) {
            closedir(d);
            lista_liberar(l);
            if (ruta_heap) free(ruta_heap);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
    }
    closedir(d);
#endif

    if (ruta_heap) free(ruta_heap);
    return valor_lista(l);
}

static Valor nativa_obtener_cwd(EvalError *err, int n_args, Valor *args,
                                  int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: obtener_cwd() no acepta argumentos, recibio %d",
            n_args);
    }
    char buf[4096];
    if (cor_getcwd(buf, sizeof(buf)) == NULL) {
        return error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo obtener el directorio actual");
    }
    /* Normalizamos \ a / para consistencia con stdlib ruta. */
    for (char *p = buf; *p; p++) {
        if (*p == '\\') *p = '/';
    }
    return valor_cadena_duplicar(buf, (int)strlen(buf));
}

static Valor nativa_directorio_crear(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: directorio_crear() requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: directorio_crear() espera una cadena con la ruta");
    }
    int len_ruta = args[0].como.cadena.longitud;
    char buf_stack[1024];
    char *ruta = buf_stack;
    char *ruta_heap = NULL;
    if (len_ruta + 1 > (int)sizeof(buf_stack)) {
        ruta_heap = (char *)malloc((size_t)len_ruta + 1);
        if (!ruta_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        ruta = ruta_heap;
    }
    memcpy(ruta, args[0].como.cadena.texto, (size_t)len_ruta);
    ruta[len_ruta] = '\0';

    int rc = cor_mkdir(ruta);
    if (rc != 0) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo crear directorio '%s'", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
    if (ruta_heap) free(ruta_heap);
    return valor_nulo();
}

/* v1.99: archivo_borrar(ruta) → nulo. Lanza ErrorDeIO si no existe o
 * la ruta es un directorio (en POSIX `unlink` falla con EISDIR; en
 * Windows `remove` también — `_rmdir` aparte). */
static Valor nativa_archivo_borrar(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_borrar() requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_borrar() espera una cadena con la ruta");
    }
    int len_ruta = args[0].como.cadena.longitud;
    char buf_stack[1024];
    char *ruta = buf_stack;
    char *ruta_heap = NULL;
    if (len_ruta + 1 > (int)sizeof(buf_stack)) {
        ruta_heap = (char *)malloc((size_t)len_ruta + 1);
        if (!ruta_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        ruta = ruta_heap;
    }
    memcpy(ruta, args[0].como.cadena.texto, (size_t)len_ruta);
    ruta[len_ruta] = '\0';

    int rc = remove(ruta);
    if (rc != 0) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo borrar '%s'", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
    if (ruta_heap) free(ruta_heap);
    return valor_nulo();
}

/* v1.99: directorio_borrar(ruta) → nulo. Solo borra directorios
 * VACIOS. Lanza ErrorDeIO si no existe, no es directorio, o no esta
 * vacio (rmdir/_rmdir). No es `rm -rf`. */
static Valor nativa_directorio_borrar(EvalError *err, int n_args, Valor *args,
                                        int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: directorio_borrar() requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: directorio_borrar() espera una cadena con la ruta");
    }
    int len_ruta = args[0].como.cadena.longitud;
    char buf_stack[1024];
    char *ruta = buf_stack;
    char *ruta_heap = NULL;
    if (len_ruta + 1 > (int)sizeof(buf_stack)) {
        ruta_heap = (char *)malloc((size_t)len_ruta + 1);
        if (!ruta_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        ruta = ruta_heap;
    }
    memcpy(ruta, args[0].como.cadena.texto, (size_t)len_ruta);
    ruta[len_ruta] = '\0';

#ifdef _WIN32
    int rc = _rmdir(ruta);
#else
    int rc = rmdir(ruta);
#endif
    if (rc != 0) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo borrar directorio '%s'", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
    if (ruta_heap) free(ruta_heap);
    return valor_nulo();
}

/* v1.99: archivo_info(ruta) → dict {tamano, mtime_epoch_ms,
 * es_archivo, es_directorio}. Lanza ErrorDeIO si la ruta no existe.
 * mtime se reporta en milisegundos desde epoch UNIX (precision por
 * segundo en Windows; podria mejorarse con GetFileAttributesEx). */
static Valor nativa_archivo_info(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_info() requiere 1 argumento, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_info() espera una cadena con la ruta");
    }
    int len_ruta = args[0].como.cadena.longitud;
    char buf_stack[1024];
    char *ruta = buf_stack;
    char *ruta_heap = NULL;
    if (len_ruta + 1 > (int)sizeof(buf_stack)) {
        ruta_heap = (char *)malloc((size_t)len_ruta + 1);
        if (!ruta_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        ruta = ruta_heap;
    }
    memcpy(ruta, args[0].como.cadena.texto, (size_t)len_ruta);
    ruta[len_ruta] = '\0';

#ifdef _WIN32
    struct _stat64 st;
    int rc = _stat64(ruta, &st);
#else
    struct stat st;
    int rc = stat(ruta, &st);
#endif
    if (rc != 0) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo obtener info de '%s'", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
    if (ruta_heap) free(ruta_heap);

#ifdef _WIN32
    bool es_dir = (st.st_mode & _S_IFDIR) != 0;
    bool es_arch = (st.st_mode & _S_IFREG) != 0;
#else
    bool es_dir = S_ISDIR(st.st_mode);
    bool es_arch = S_ISREG(st.st_mode);
#endif
    int64_t tamano = (int64_t)st.st_size;
    int64_t mtime_ms = (int64_t)st.st_mtime * 1000;

    Diccionario *d = dicc_nuevo();
    if (!d) return error_nativa(err, linea, columna, "memoria insuficiente");
    if (!dicc_asignar(d, valor_cadena_duplicar("tamano", 6),
                       valor_entero_de_i64(tamano))) {
        dicc_liberar(d);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    if (!dicc_asignar(d, valor_cadena_duplicar("mtime_epoch_ms", 14),
                       valor_entero_de_i64(mtime_ms))) {
        dicc_liberar(d);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    if (!dicc_asignar(d, valor_cadena_duplicar("es_archivo", 10),
                       valor_booleano(es_arch))) {
        dicc_liberar(d);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    if (!dicc_asignar(d, valor_cadena_duplicar("es_directorio", 13),
                       valor_booleano(es_dir))) {
        dicc_liberar(d);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    return valor_diccionario(d);
}

/* v1.105: archivo_copiar(origen, destino) → nulo.
 *
 * Copia bytes literalmente de `origen` a `destino`. `destino` se
 * trunca si existe. Lanza ErrorDeIO si origen no existe o
 * destino no se puede abrir para escritura.
 *
 * Implementacion: fread/fwrite en buffer de 64 KiB. Sin
 * preservacion de mtime/permisos (pendiente para release futura).
 * No es 'cp -r' — solo archivos individuales; para arboles usar
 * `archivos.copiar_arbol` (pure-Cornamusa). */
static Valor nativa_archivo_copiar(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_copiar() requiere 2 argumentos, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_copiar() requiere cadenas (origen, destino)");
    }

    int lo = args[0].como.cadena.longitud;
    int ld = args[1].como.cadena.longitud;
    char orig_stack[1024], dest_stack[1024];
    char *orig = orig_stack, *dest = dest_stack;
    char *orig_heap = NULL, *dest_heap = NULL;
    if (lo + 1 > (int)sizeof(orig_stack)) {
        orig_heap = (char *)malloc((size_t)lo + 1);
        if (!orig_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        orig = orig_heap;
    }
    if (ld + 1 > (int)sizeof(dest_stack)) {
        dest_heap = (char *)malloc((size_t)ld + 1);
        if (!dest_heap) {
            if (orig_heap) free(orig_heap);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        dest = dest_heap;
    }
    memcpy(orig, args[0].como.cadena.texto, (size_t)lo); orig[lo] = '\0';
    memcpy(dest, args[1].como.cadena.texto, (size_t)ld); dest[ld] = '\0';

    FILE *fi = fopen(orig, "rb");
    if (!fi) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo abrir '%s' para lectura", orig);
        if (orig_heap) free(orig_heap);
        if (dest_heap) free(dest_heap);
        return r;
    }
    FILE *fo = fopen(dest, "wb");
    if (!fo) {
        fclose(fi);
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo abrir '%s' para escritura", dest);
        if (orig_heap) free(orig_heap);
        if (dest_heap) free(dest_heap);
        return r;
    }

    enum { BUF_SZ = 65536 };
    char *buf = (char *)malloc(BUF_SZ);
    if (!buf) {
        fclose(fi); fclose(fo);
        if (orig_heap) free(orig_heap);
        if (dest_heap) free(dest_heap);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }

    bool error_io = false;
    while (!feof(fi)) {
        size_t leido = fread(buf, 1, BUF_SZ, fi);
        if (leido == 0) {
            if (ferror(fi)) error_io = true;
            break;
        }
        size_t escrito = fwrite(buf, 1, leido, fo);
        if (escrito != leido) { error_io = true; break; }
    }

    free(buf);
    fclose(fi);
    fclose(fo);
    if (orig_heap) free(orig_heap);
    if (dest_heap) free(dest_heap);
    if (error_io) {
        return error_nativa(err, linea, columna,
            "ErrorDeIO: error durante la copia");
    }
    return valor_nulo();
}

/* v1.111: archivo_mover(origen, destino) -> nulo.
 *
 * Renombra/mueve un archivo. ATOMICO en el mismo sistema de archivos
 * (rename() es atomico en POSIX y Windows). Cross-FS no es atomico
 * pero se intenta copiar+borrar como fallback.
 *
 * Lanza ErrorDeIO si la operacion falla (origen no existe, destino
 * sin permisos, etc.). El comportamiento Windows: rename() falla si
 * el destino ya existe; usamos MoveFileExA con MOVEFILE_REPLACE_EXISTING
 * para mantener consistencia con POSIX. */
static Valor nativa_archivo_mover(EvalError *err, int n_args, Valor *args,
                                    int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_mover() requiere 2 argumentos, recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_mover() requiere cadenas (origen, destino)");
    }
    int lo = args[0].como.cadena.longitud;
    int ld = args[1].como.cadena.longitud;
    char orig_stack[1024], dest_stack[1024];
    char *orig = orig_stack, *dest = dest_stack;
    char *orig_heap = NULL, *dest_heap = NULL;
    if (lo + 1 > (int)sizeof(orig_stack)) {
        orig_heap = (char *)malloc((size_t)lo + 1);
        if (!orig_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        orig = orig_heap;
    }
    if (ld + 1 > (int)sizeof(dest_stack)) {
        dest_heap = (char *)malloc((size_t)ld + 1);
        if (!dest_heap) {
            if (orig_heap) free(orig_heap);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        dest = dest_heap;
    }
    memcpy(orig, args[0].como.cadena.texto, (size_t)lo); orig[lo] = '\0';
    memcpy(dest, args[1].como.cadena.texto, (size_t)ld); dest[ld] = '\0';

#ifdef _WIN32
    /* MoveFileExA es nivel-Win32 y soporta REPLACE_EXISTING. */
    BOOL ok = MoveFileExA(orig, dest, MOVEFILE_REPLACE_EXISTING);
    int rc = ok ? 0 : -1;
#else
    int rc = rename(orig, dest);
#endif
    if (rc != 0) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo mover '%s' a '%s'", orig, dest);
        if (orig_heap) free(orig_heap);
        if (dest_heap) free(dest_heap);
        return r;
    }
    if (orig_heap) free(orig_heap);
    if (dest_heap) free(dest_heap);
    return valor_nulo();
}

/* v1.111: archivo_set_mtime(ruta, mtime_ms) -> nulo.
 *
 * Establece la fecha de modificacion (mtime) del archivo o directorio
 * en `ruta` a `mtime_ms` milisegundos desde UNIX epoch.
 *
 * POSIX: utimes() con struct timeval[2] = {atime, mtime}. Como no
 * queremos cambiar atime, se lee primero con stat y se reusa.
 *
 * Windows: SetFileTime() con FILETIME. Conversion: epoch UNIX
 * (1970) a epoch Windows (1601) anadiendo 11644473600 segundos.
 *
 * Lanza ErrorDeIO si la ruta no existe o la operacion falla.
 *
 * Tambien existe archivo_tocar(ruta) en stdlib/archivos.cor como
 * azucar: actualiza mtime al instante actual. */
static Valor nativa_archivo_set_mtime(EvalError *err, int n_args, Valor *args,
                                        int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_set_mtime() requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_set_mtime() ruta debe ser cadena");
    }
    int64_t mtime_ms;
    if (!valor_entero_a_i64(&args[1], &mtime_ms)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: archivo_set_mtime() mtime_ms debe ser entero");
    }
    int len = args[0].como.cadena.longitud;
    char buf_stack[1024];
    char *ruta = buf_stack;
    char *ruta_heap = NULL;
    if (len + 1 > (int)sizeof(buf_stack)) {
        ruta_heap = (char *)malloc((size_t)len + 1);
        if (!ruta_heap) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        ruta = ruta_heap;
    }
    memcpy(ruta, args[0].como.cadena.texto, (size_t)len);
    ruta[len] = '\0';

#ifdef _WIN32
    HANDLE h = CreateFileA(ruta, FILE_WRITE_ATTRIBUTES,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL, OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: no se pudo abrir '%s' para set_mtime", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
    /* FILETIME = numero de intervalos de 100ns desde 1601-01-01 UTC.
     * UNIX epoch (1970-01-01) - Windows epoch (1601-01-01) =
     *   11644473600 segundos = 116444736000000000 intervalos de 100ns. */
    int64_t intervalos = (mtime_ms * 10000LL) + 116444736000000000LL;
    FILETIME ft;
    ft.dwLowDateTime = (DWORD)(intervalos & 0xFFFFFFFFLL);
    ft.dwHighDateTime = (DWORD)(intervalos >> 32);
    BOOL ok = SetFileTime(h, NULL, NULL, &ft);
    CloseHandle(h);
    if (!ok) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: SetFileTime fallo para '%s'", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
#else
    /* utimes acepta dos timevals: [atime, mtime]. Preservamos atime
     * leyendo stat primero. */
    struct stat st;
    if (stat(ruta, &st) != 0) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: archivo_set_mtime: '%s' no existe", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
    struct timeval tv[2];
    tv[0].tv_sec = st.st_atime;
    tv[0].tv_usec = 0;
    tv[1].tv_sec = (time_t)(mtime_ms / 1000);
    tv[1].tv_usec = (suseconds_t)((mtime_ms % 1000) * 1000);
    if (utimes(ruta, tv) != 0) {
        Valor r = error_nativa(err, linea, columna,
            "ErrorDeIO: utimes fallo para '%s'", ruta);
        if (ruta_heap) free(ruta_heap);
        return r;
    }
#endif
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

/* v1.158: divmod(a, b) — devuelve la tupla (cociente, resto) de la
 * division entera euclidea. Paridad con Python divmod(a, b).
 *
 * Para enteros con `b > 0`, el resto siempre esta en [0, b).
 * Para `b < 0`, el resto esta en (b, 0].
 * El cociente q cumple a = q*b + r exactamente.
 *
 * b == 0 lanza ErrorAritmetico (division por cero).
 *
 * Solo soporta enteros por ahora (SMALL o BIG). Decimales lanzan
 * ErrorDeTipo — Python permite floats pero la semantica de
 * floor-div sobre IEEE 754 introduce errores que prefiero evitar
 * en una release pequena. */
static Valor nativa_divmod(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: divmod(a, b) requiere 2 argumentos");
    }
    if (!valor_es_entero(&args[0]) || !valor_es_entero(&args[1])) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: divmod() requiere enteros, no '%s' y '%s'",
            valor_nombre_tipo(&args[0]), valor_nombre_tipo(&args[1]));
    }
    int64_t a_i64, b_i64;
    bool a_fits = valor_entero_a_i64(&args[0], &a_i64);
    bool b_fits = valor_entero_a_i64(&args[1], &b_i64);
    /* Camino rapido: ambos caben en int64. */
    if (a_fits && b_fits) {
        if (b_i64 == 0) {
            return error_nativa(err, linea, columna,
                "ErrorAritmetico: divmod por cero");
        }
        /* C division truncates toward zero. Adjust to floor div. */
        int64_t q = a_i64 / b_i64;
        int64_t r = a_i64 - q * b_i64;
        /* Si el resto y b tienen signos opuestos, ajustar. */
        if (r != 0 && ((r < 0) != (b_i64 < 0))) {
            q -= 1;
            r += b_i64;
        }
        Tupla *t = tupla_nueva(2);
        if (!t) return error_nativa(err, linea, columna, "memoria insuficiente");
        t->elementos[0] = valor_entero_de_i64(q);
        t->elementos[1] = valor_entero_de_i64(r);
        return valor_tupla(t);
    }
    /* Camino bignum via libtommath. ma/mb son temporales en stack;
     * mq/mr son heap-allocated porque seran transferidos via
     * valor_entero_de_mp_normalizado (que toma ownership del puntero). */
    mp_int ma, mb;
    mp_int *mq = (mp_int *)malloc(sizeof(mp_int));
    mp_int *mr = (mp_int *)malloc(sizeof(mp_int));
    if (!mq || !mr) {
        free(mq); free(mr);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    if (mp_init_multi(&ma, &mb, mq, mr, NULL) != MP_OKAY) {
        free(mq); free(mr);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    if (args[0].tipo == VAL_ENTERO_SMALL) mp_set_i64(&ma, args[0].como.entero_small);
    else if (args[0].tipo == VAL_BOOLEANO) mp_set_l(&ma, args[0].como.booleano ? 1 : 0);
    else mp_copy(args[0].como.entero, &ma);
    if (args[1].tipo == VAL_ENTERO_SMALL) mp_set_i64(&mb, args[1].como.entero_small);
    else if (args[1].tipo == VAL_BOOLEANO) mp_set_l(&mb, args[1].como.booleano ? 1 : 0);
    else mp_copy(args[1].como.entero, &mb);
    if (mp_iszero(&mb)) {
        mp_clear_multi(&ma, &mb, mq, mr, NULL);
        free(mq); free(mr);
        return error_nativa(err, linea, columna,
            "ErrorAritmetico: divmod por cero");
    }
    /* mp_div hace truncacion hacia cero. Ajustar a floor div. */
    if (mp_div(&ma, &mb, mq, mr) != MP_OKAY) {
        mp_clear_multi(&ma, &mb, mq, mr, NULL);
        free(mq); free(mr);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    /* Si el resto es no-cero y tiene signo distinto a b, ajustar. */
    if (!mp_iszero(mr)
        && (mp_isneg(mr) != mp_isneg(&mb))) {
        mp_int uno;
        if (mp_init(&uno) != MP_OKAY) {
            mp_clear_multi(&ma, &mb, mq, mr, NULL);
            free(mq); free(mr);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        mp_set_l(&uno, 1);
        mp_sub(mq, &uno, mq);
        mp_add(mr, &mb, mr);
        mp_clear(&uno);
    }
    Tupla *t = tupla_nueva(2);
    if (!t) {
        mp_clear_multi(&ma, &mb, mq, mr, NULL);
        free(mq); free(mr);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    /* valor_entero_de_mp_normalizado toma ownership de mq/mr — los
     * libera con free() si demote a SMALL, o los guarda en el Valor. */
    t->elementos[0] = valor_entero_de_mp_normalizado(mq);
    t->elementos[1] = valor_entero_de_mp_normalizado(mr);
    mp_clear_multi(&ma, &mb, NULL);
    return valor_tupla(t);
}

/* v1.158: potencia_modular(base, exp, mod) — devuelve (base^exp) % mod
 * de forma eficiente, usando exponenciacion modular en tiempo
 * O(log(exp) * tam_mod). Paridad con Python pow(base, exp, mod).
 *
 * Acepta enteros. exp debe ser >= 0; mod debe ser != 0.
 * Usa mp_exptmod de libtommath internamente. */
static Valor nativa_potencia_modular(EvalError *err, int n_args, Valor *args,
                                          int linea, int columna) {
    if (n_args != 3) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: potencia_modular(base, exp, mod) requiere 3 argumentos");
    }
    for (int i = 0; i < 3; i++) {
        if (!valor_es_entero(&args[i])) {
            return error_nativa(err, linea, columna,
                "ErrorDeTipo: potencia_modular() requiere enteros, arg %d es '%s'",
                i, valor_nombre_tipo(&args[i]));
        }
    }
    mp_int mbase, mexp, mmod;
    mp_int *mres = (mp_int *)malloc(sizeof(mp_int));
    if (!mres) return error_nativa(err, linea, columna, "memoria insuficiente");
    if (mp_init_multi(&mbase, &mexp, &mmod, mres, NULL) != MP_OKAY) {
        free(mres);
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    for (int i = 0; i < 3; i++) {
        mp_int *dst = (i == 0) ? &mbase : (i == 1) ? &mexp : &mmod;
        if (args[i].tipo == VAL_ENTERO_SMALL) mp_set_i64(dst, args[i].como.entero_small);
        else if (args[i].tipo == VAL_BOOLEANO) mp_set_l(dst, args[i].como.booleano ? 1 : 0);
        else mp_copy(args[i].como.entero, dst);
    }
    if (mp_isneg(&mexp)) {
        mp_clear_multi(&mbase, &mexp, &mmod, mres, NULL);
        free(mres);
        return error_nativa(err, linea, columna,
            "ErrorDeValor: potencia_modular requiere exponente >= 0");
    }
    if (mp_iszero(&mmod)) {
        mp_clear_multi(&mbase, &mexp, &mmod, mres, NULL);
        free(mres);
        return error_nativa(err, linea, columna,
            "ErrorAritmetico: modulo no puede ser cero");
    }
    if (mp_exptmod(&mbase, &mexp, &mmod, mres) != MP_OKAY) {
        mp_clear_multi(&mbase, &mexp, &mmod, mres, NULL);
        free(mres);
        return error_nativa(err, linea, columna,
            "ErrorAritmetico: potencia_modular fallo");
    }
    Valor r = valor_entero_de_mp_normalizado(mres);
    mp_clear_multi(&mbase, &mexp, &mmod, NULL);
    return r;
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

/* v1.163: hash(x) — expone el hash interno (mismo que usan dicc y
 * conjunto). Devuelve un entero (puede ser negativo). No criptografico.
 *
 * Rechaza valores no hashables (listas, dicc, conjunto, lambdas con
 * captura mutable). Para esos tipos el dict/set ya lanza ErrorDeTipo —
 * hash() debe coincidir con esa semantica para que `hash(k)` y `d[k]`
 * fallen al unisono. */
static Valor nativa_hash(EvalError *err, int n_args, Valor *args,
                          int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: hash() requiere 1 argumento, recibio %d", n_args);
    }
    if (!valor_es_hashable(&args[0])) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: hash() no acepta valores de tipo %s (no hashable)",
            valor_nombre_tipo(&args[0]));
    }
    uint64_t h = hash_valor(&args[0]);
    return valor_entero_de_i64((int64_t)h);
}

/* v1.164: congelar(s) — devuelve un conjunto inmutable y hashable
 * con los mismos elementos. El original NO se modifica (paridad con
 * Python `frozenset()`). Si `s` ya es un frozenset, retorna una nueva
 * copia frozen (no devuelve el mismo objeto para evitar aliasing).
 *
 * Acepta tambien iterables (lista, tupla, cadena, rango, dicc — sobre
 * las claves) — equivalente a congelar(conjunto(it)) en una sola
 * llamada. */
static Valor nativa_congelar(EvalError *err, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: congelar() requiere 1 argumento, recibio %d", n_args);
    }
    Conjunto *nuevo = conj_nuevo();
    if (!nuevo) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    const Valor *it = &args[0];
    if (it->tipo == VAL_CONJUNTO) {
        Conjunto *src = it->como.conjunto;
        for (int i = 0; i < src->capacidad; i++) {
            const EntradaConjunto *e = &src->entradas[i];
            if (!e->ocupada) continue;
            if (!conj_agregar(nuevo, valor_clonar(&e->elemento))) {
                conj_liberar(nuevo);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
        }
    } else if (it->tipo == VAL_LISTA) {
        Lista *l = it->como.lista;
        for (int i = 0; i < l->cuenta; i++) {
            if (!valor_es_hashable(&l->elementos[i])) {
                conj_liberar(nuevo);
                return error_nativa(err, linea, columna,
                    "ErrorDeTipo: '%s' no es hashable",
                    valor_nombre_tipo(&l->elementos[i]));
            }
            if (!conj_agregar(nuevo, valor_clonar(&l->elementos[i]))) {
                conj_liberar(nuevo);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
        }
    } else if (it->tipo == VAL_TUPLA) {
        Tupla *t = it->como.tupla;
        for (int i = 0; i < t->cuenta; i++) {
            if (!valor_es_hashable(&t->elementos[i])) {
                conj_liberar(nuevo);
                return error_nativa(err, linea, columna,
                    "ErrorDeTipo: '%s' no es hashable",
                    valor_nombre_tipo(&t->elementos[i]));
            }
            if (!conj_agregar(nuevo, valor_clonar(&t->elementos[i]))) {
                conj_liberar(nuevo);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
        }
    } else if (it->tipo == VAL_CADENA) {
        const char *s = it->como.cadena.texto;
        int sl = it->como.cadena.longitud;
        int p = 0;
        while (p < sl) {
            utf8proc_int32_t cp;
            utf8proc_ssize_t cons = utf8proc_iterate(
                (const utf8proc_uint8_t *)(s + p), sl - p, &cp);
            if (cons <= 0) { p++; continue; }
            Valor cv = valor_cadena_duplicar(s + p, (int)cons);
            if (!conj_agregar(nuevo, cv)) {
                conj_liberar(nuevo);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            p += (int)cons;
        }
    } else {
        conj_liberar(nuevo);
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: congelar() requiere un iterable (conjunto/lista/tupla/cadena), no '%s'",
            valor_nombre_tipo(it));
    }
    nuevo->congelado = true;
    return valor_conjunto(nuevo);
}

/* v1.165: copia(x) — shallow copy. Para mutables (lista, dicc,
 * conjunto) construye un nuevo contenedor con `valor_clonar` de
 * cada elemento — los elementos siguen compartiendo referencia con
 * los originales, pero el contenedor en si es independiente.
 *
 * Para inmutables (entero, decimal, cadena, booleano, nulo, tupla,
 * rango) retorna `valor_clonar` directo — no hay diferencia entre
 * shallow y deep para ellos.
 *
 * Si el conjunto era frozen, la copia tambien lo es. */
static Valor nativa_copia(EvalError *err, int n_args, Valor *args,
                           int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: copia() requiere 1 argumento, recibio %d", n_args);
    }
    const Valor *v = &args[0];
    if (v->tipo == VAL_LISTA) {
        Lista *src = v->como.lista;
        Lista *dst = lista_nueva(src->cuenta);
        if (!dst) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        for (int i = 0; i < src->cuenta; i++) {
            if (!lista_agregar(dst, valor_clonar(&src->elementos[i]))) {
                lista_liberar(dst);
                return error_nativa(err, linea, columna,
                    "memoria insuficiente");
            }
        }
        return valor_lista(dst);
    }
    if (v->tipo == VAL_DICCIONARIO) {
        Diccionario *src = v->como.dicc;
        Diccionario *dst = dicc_nuevo();
        if (!dst) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        for (int idx = 0; idx < src->cuenta; idx++) {
            int slot = src->orden_insercion[idx];
            const EntradaDicc *e = &src->entradas[slot];
            if (!dicc_asignar(dst, valor_clonar(&e->clave),
                                    valor_clonar(&e->valor))) {
                dicc_liberar(dst);
                return error_nativa(err, linea, columna,
                    "memoria insuficiente");
            }
        }
        return valor_diccionario(dst);
    }
    if (v->tipo == VAL_CONJUNTO) {
        Conjunto *src = v->como.conjunto;
        Conjunto *dst = conj_nuevo();
        if (!dst) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        for (int i = 0; i < src->capacidad; i++) {
            if (!src->entradas[i].ocupada) continue;
            if (!conj_agregar(dst, valor_clonar(&src->entradas[i].elemento))) {
                conj_liberar(dst);
                return error_nativa(err, linea, columna,
                    "memoria insuficiente");
            }
        }
        dst->congelado = src->congelado;
        return valor_conjunto(dst);
    }
    /* Inmutables: valor_clonar ya devuelve la representacion correcta
     * (compartido por refcount donde aplica, escalar copiado donde
     * aplica). */
    return valor_clonar(v);
}

/* v1.165: copia_profunda(x) — deep copy recursivo. Para
 * contenedores anidados construye nuevos contenedores en cada
 * nivel; cualquier mutacion sobre la copia no afecta al original
 * por mas profunda que sea.
 *
 * Maneja ciclos via memoizador (puntero original -> Valor nuevo).
 * Sin el memo, una lista que se contiene a si misma colgaria la VM.
 *
 * Las claves de dicc se clonan shallow porque son hashables (y por
 * tanto inmutables en la practica). Los elementos de conjunto idem.
 *
 * Tipos no-colectivos (instancia, funcion, etc.) caen a
 * valor_clonar — equivalente a "share by reference". Para deep
 * copy de instancias habria que invocar un dunder __copia__ que
 * todavia no existe; documentado como limitacion. */
static Valor copia_profunda_rec(EvalError *err, const Valor *v,
                                 Diccionario *memo,
                                 int linea, int columna) {
    void *puntero = NULL;
    switch (v->tipo) {
        case VAL_LISTA:        puntero = v->como.lista; break;
        case VAL_DICCIONARIO:  puntero = v->como.dicc; break;
        case VAL_CONJUNTO:     puntero = v->como.conjunto; break;
        case VAL_TUPLA:        puntero = v->como.tupla; break;
        default: break;
    }
    if (puntero) {
        Valor clave = valor_entero_de_i64((int64_t)(intptr_t)puntero);
        Valor cached;
        if (dicc_obtener(memo, &clave, &cached)) {
            valor_destruir(&clave);
            return valor_clonar(&cached);
        }
        valor_destruir(&clave);
    }
    if (v->tipo == VAL_LISTA) {
        Lista *src = v->como.lista;
        Lista *dst = lista_nueva(src->cuenta);
        if (!dst) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        Valor resultado = valor_lista(dst);
        /* Cachear ANTES de recurrir para tolerar ciclos. */
        Valor mkey = valor_entero_de_i64((int64_t)(intptr_t)src);
        dicc_asignar(memo, mkey, valor_clonar(&resultado));
        for (int i = 0; i < src->cuenta; i++) {
            Valor copiado = copia_profunda_rec(err, &src->elementos[i],
                                                memo, linea, columna);
            if (err->tuvo_error) { valor_destruir(&resultado);
                                    return valor_nulo(); }
            if (!lista_agregar(dst, copiado)) {
                valor_destruir(&resultado);
                return error_nativa(err, linea, columna,
                    "memoria insuficiente");
            }
        }
        return resultado;
    }
    if (v->tipo == VAL_DICCIONARIO) {
        Diccionario *src = v->como.dicc;
        Diccionario *dst = dicc_nuevo();
        if (!dst) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        Valor resultado = valor_diccionario(dst);
        Valor mkey = valor_entero_de_i64((int64_t)(intptr_t)src);
        dicc_asignar(memo, mkey, valor_clonar(&resultado));
        for (int idx = 0; idx < src->cuenta; idx++) {
            int slot = src->orden_insercion[idx];
            const EntradaDicc *e = &src->entradas[slot];
            /* La clave es hashable -> shallow basta. */
            Valor k = valor_clonar(&e->clave);
            Valor val = copia_profunda_rec(err, &e->valor, memo,
                                             linea, columna);
            if (err->tuvo_error) {
                valor_destruir(&k);
                valor_destruir(&resultado);
                return valor_nulo();
            }
            if (!dicc_asignar(dst, k, val)) {
                valor_destruir(&resultado);
                return error_nativa(err, linea, columna,
                    "memoria insuficiente");
            }
        }
        return resultado;
    }
    if (v->tipo == VAL_CONJUNTO) {
        Conjunto *src = v->como.conjunto;
        Conjunto *dst = conj_nuevo();
        if (!dst) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        Valor resultado = valor_conjunto(dst);
        Valor mkey = valor_entero_de_i64((int64_t)(intptr_t)src);
        dicc_asignar(memo, mkey, valor_clonar(&resultado));
        for (int i = 0; i < src->capacidad; i++) {
            if (!src->entradas[i].ocupada) continue;
            /* Elemento hashable -> shallow. */
            if (!conj_agregar(dst, valor_clonar(&src->entradas[i].elemento))) {
                valor_destruir(&resultado);
                return error_nativa(err, linea, columna,
                    "memoria insuficiente");
            }
        }
        dst->congelado = src->congelado;
        return resultado;
    }
    if (v->tipo == VAL_TUPLA) {
        Tupla *src = v->como.tupla;
        Tupla *dst = tupla_nueva(src->cuenta);
        if (!dst) return error_nativa(err, linea, columna,
            "memoria insuficiente");
        Valor resultado = valor_tupla(dst);
        Valor mkey = valor_entero_de_i64((int64_t)(intptr_t)src);
        dicc_asignar(memo, mkey, valor_clonar(&resultado));
        for (int i = 0; i < src->cuenta; i++) {
            dst->elementos[i] = copia_profunda_rec(err, &src->elementos[i],
                                                    memo, linea, columna);
            if (err->tuvo_error) {
                valor_destruir(&resultado);
                return valor_nulo();
            }
        }
        return resultado;
    }
    /* Inmutables / no-colectivos: valor_clonar. */
    return valor_clonar(v);
}

static Valor nativa_copia_profunda(EvalError *err, int n_args, Valor *args,
                                    int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: copia_profunda() requiere 1 argumento, recibio %d",
            n_args);
    }
    Diccionario *memo = dicc_nuevo();
    if (!memo) return error_nativa(err, linea, columna,
        "memoria insuficiente");
    Valor resultado = copia_profunda_rec(err, &args[0], memo, linea, columna);
    dicc_liberar(memo);
    return resultado;
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

/* v1.109: `@escritor` decorador para vincular un setter a una
 * propiedad ya existente. Envuelve el closure en una Propiedad con
 * setter=closure y getter=NULL como marcador. OP_METODO detecta que
 * el marcador (propiedad sin getter pero con setter) y lo fusiona
 * con la propiedad ya guardada con ese mismo nombre.
 *
 * Patron de uso (Python-style):
 *
 *   clase Caja:
 *       @propiedad
 *       funcion lado(yo):
 *           retornar yo._lado
 *       fin funcion
 *
 *       @escritor
 *       funcion lado(yo, valor):
 *           si valor < 0:
 *               lanzar ErrorDeValor("lado debe ser >= 0")
 *           fin si
 *           yo._lado = valor
 *       fin funcion
 *   fin clase
 *
 * Si no hay propiedad previa con ese nombre cuando se procesa
 * @escritor, OP_METODO lanza error claro indicando el orden
 * requerido. */
static Valor nativa_escritor(EvalError *err, int n_args, Valor *args,
                               int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: escritor() requiere 1 argumento (callable)");
    }
    if (args[0].tipo != VAL_FUNCION_BC) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: escritor() espera una funcion, recibio '%s'",
            valor_nombre_tipo(&args[0]));
    }
    /* Crear una Propiedad-marcador: getter NULL, setter = la closure.
     * OP_METODO la detectara y fusionara con la propiedad existente. */
    Propiedad *p = (Propiedad *)gc_alocar(sizeof(Propiedad), GC_TIPO_PROPIEDAD);
    if (!p) {
        return error_nativa(err, linea, columna,
            "memoria insuficiente al crear setter");
    }
    closure_retener(args[0].como.closure);
    p->getter = NULL;
    p->setter = args[0].como.closure;
    p->refcount = 1;
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

/* v1.153: case conversion Unicode-aware via utf8proc_toupper/tolower.
 * Recorre code-points con utf8proc_iterate, aplica el mapping y
 * re-encode con utf8proc_encode_char. El buffer crece dinamicamente
 * porque algunos mappings (e.g. SS -> ß en aleman) cambian la longitud
 * en bytes. */
static Valor cadena_caso_unicode(EvalError *err, const Valor *v, bool a_minus,
                                    int linea, int columna) {
    if (v->tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: requiere una cadena, no '%s'",
            valor_nombre_tipo(v));
    }
    const char *s = v->como.cadena.texto;
    int sl = v->como.cadena.longitud;
    int cap = sl * 2 + 8;
    uint8_t *buf = (uint8_t *)malloc((size_t)cap);
    if (!buf) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    int w = 0;
    int i = 0;
    while (i < sl) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t cons = utf8proc_iterate(
            (const utf8proc_uint8_t *)(s + i), sl - i, &cp);
        if (cons <= 0) {
            /* Byte invalido — copiar tal cual y avanzar 1. */
            if (w + 1 > cap) {
                cap *= 2;
                uint8_t *nb = (uint8_t *)realloc(buf, (size_t)cap);
                if (!nb) { free(buf); return error_nativa(err, linea, columna, "memoria insuficiente"); }
                buf = nb;
            }
            buf[w++] = (uint8_t)s[i++];
            continue;
        }
        utf8proc_int32_t mapped = a_minus
            ? utf8proc_tolower(cp)
            : utf8proc_toupper(cp);
        if (w + 4 > cap) {
            cap = cap * 2 + 4;
            uint8_t *nb = (uint8_t *)realloc(buf, (size_t)cap);
            if (!nb) { free(buf); return error_nativa(err, linea, columna, "memoria insuficiente"); }
            buf = nb;
        }
        utf8proc_ssize_t n = utf8proc_encode_char(mapped, &buf[w]);
        if (n <= 0) {
            /* Imposible si mapped esta en rango Unicode; copiar el
             * original como fallback. */
            n = utf8proc_encode_char(cp, &buf[w]);
            if (n <= 0) n = 0;
        }
        w += (int)n;
        i += (int)cons;
    }
    Valor r = valor_cadena_duplicar((const char *)buf, w);
    free(buf);
    return r;
}

static Valor nativa_cadena_minusculas(EvalError *err, int n_args, Valor *args,
                                          int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: minusculas() no acepta argumentos");
    }
    return cadena_caso_unicode(err, &args[0], true, linea, columna);
}

/* v1.154: helper compartido para predicados es_alfa/es_digito/etc.
 * Itera los code-points y verifica que TODOS cumplan la condicion
 * indicada por `tipo`. Cadena vacia → falso (paridad con Python).
 *
 * tipo:
 *   1 = es_alfa     : LU/LL/LT/LM/LO
 *   2 = es_digito   : ND
 *   3 = es_alfanum  : LU/LL/LT/LM/LO/ND/NL/NO
 *   4 = es_espacios : ZS/ZL/ZP + ASCII whitespace (\t\n\r\f\v)
 */
static Valor cadena_predicado(EvalError *err, const Valor *v, int tipo,
                                  int linea, int columna) {
    if (v->tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: requiere una cadena, no '%s'",
            valor_nombre_tipo(v));
    }
    const char *s = v->como.cadena.texto;
    int sl = v->como.cadena.longitud;
    if (sl == 0) return valor_booleano(false);
    int i = 0;
    while (i < sl) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t cons = utf8proc_iterate(
            (const utf8proc_uint8_t *)(s + i), sl - i, &cp);
        if (cons <= 0) return valor_booleano(false);
        int cat = utf8proc_category(cp);
        bool ok = false;
        switch (tipo) {
            case 1:  /* es_alfa */
                ok = (cat == UTF8PROC_CATEGORY_LU || cat == UTF8PROC_CATEGORY_LL
                     || cat == UTF8PROC_CATEGORY_LT || cat == UTF8PROC_CATEGORY_LM
                     || cat == UTF8PROC_CATEGORY_LO);
                break;
            case 2:  /* es_digito */
                ok = (cat == UTF8PROC_CATEGORY_ND);
                break;
            case 3:  /* es_alfanum */
                ok = (cat == UTF8PROC_CATEGORY_LU || cat == UTF8PROC_CATEGORY_LL
                     || cat == UTF8PROC_CATEGORY_LT || cat == UTF8PROC_CATEGORY_LM
                     || cat == UTF8PROC_CATEGORY_LO || cat == UTF8PROC_CATEGORY_ND
                     || cat == UTF8PROC_CATEGORY_NL || cat == UTF8PROC_CATEGORY_NO);
                break;
            case 4:  /* es_espacios */
                ok = (cat == UTF8PROC_CATEGORY_ZS
                     || cat == UTF8PROC_CATEGORY_ZL
                     || cat == UTF8PROC_CATEGORY_ZP
                     || cp == '\t' || cp == '\n' || cp == '\r'
                     || cp == '\f' || cp == '\v');
                break;
        }
        if (!ok) return valor_booleano(false);
        i += (int)cons;
    }
    return valor_booleano(true);
}

#define DEFINIR_PREDICADO(nombre, tipo_num)                                    \
static Valor nativa_cadena_##nombre(EvalError *err, int n_args, Valor *args,   \
                                       int linea, int columna) {              \
    if (n_args != 1) {                                                         \
        return error_nativa(err, linea, columna,                               \
            "ErrorDeTipo: " #nombre "() no acepta argumentos");                \
    }                                                                          \
    return cadena_predicado(err, &args[0], tipo_num, linea, columna);          \
}

DEFINIR_PREDICADO(es_alfa,     1)
DEFINIR_PREDICADO(es_digito,   2)
DEFINIR_PREDICADO(es_alfanum,  3)
DEFINIR_PREDICADO(es_espacios, 4)

#undef DEFINIR_PREDICADO

/* v1.154: cadena.titulo() — primera letra de cada "palabra" en
 * mayuscula, el resto en minuscula. Una palabra empieza despues de
 * un caracter no alfabetico (incluyendo el inicio de la cadena).
 * Paridad con Python str.title(). */
/* v1.162: cadena.sin_acentos() — devuelve una copia con los acentos
 * y marcas combinantes (ä, é, ñ → ã, e, n; ñ → n) eliminados. Internamente
 * usa utf8proc_map con UTF8PROC_DECOMPOSE | UTF8PROC_STRIPMARK | UTF8PROC_COMPOSE:
 *
 *   1. DECOMPOSE: separa 'é' en 'e' + acento combinante.
 *   2. STRIPMARK: quita las marcas combinantes.
 *   3. COMPOSE: recompone lo que quede (sin marcas).
 *
 * Util para:
 *   - Slugs de URL (eliminar diacriticos antes de tolower).
 *   - Comparacion tolerante a acentos.
 *   - Normalizacion para busqueda.
 *
 * Nota: la 'ñ' tambien pierde la tilde por ser una marca combinante:
 *   "ñoño".sin_acentos() -> "nono".
 * Para preservar la 'ñ' especificamente, usa un postproceso a medida. */
static Valor nativa_cadena_sin_acentos(EvalError *err, int n_args, Valor *args,
                                            int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: sin_acentos() no acepta argumentos");
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: sin_acentos() requiere una cadena, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    const char *s = args[0].como.cadena.texto;
    int sl = args[0].como.cadena.longitud;
    if (sl == 0) return valor_cadena_duplicar("", 0);
    utf8proc_uint8_t *dst = NULL;
    /* DECOMPOSE | STRIPMARK quita las marcas combinantes. COMPOSE
     * (recomposicion) no es compatible con STRIPMARK + DECOMPOSE
     * — el orden de operaciones es importante: descomponer separa
     * 'é' en 'e' + combining acute, STRIPMARK quita el combining,
     * y al no recomponer, ya queda 'e' como deseamos. */
    utf8proc_ssize_t n = utf8proc_map(
        (const utf8proc_uint8_t *)s, sl, &dst,
        UTF8PROC_DECOMPOSE | UTF8PROC_STRIPMARK | UTF8PROC_STABLE);
    if (n < 0 || !dst) {
        const char *msg = (n < 0) ? utf8proc_errmsg(n) : "memoria insuficiente";
        if (dst) free(dst);
        return error_nativa(err, linea, columna,
            "ErrorInterno: sin_acentos: %s", msg ? msg : "fallo");
    }
    Valor r = valor_cadena_duplicar((const char *)dst, (int)n);
    free(dst);
    return r;
}

static Valor nativa_cadena_titulo(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: titulo() no acepta argumentos");
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: titulo() requiere una cadena, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    const char *s = args[0].como.cadena.texto;
    int sl = args[0].como.cadena.longitud;
    int cap = sl * 2 + 8;
    uint8_t *buf = (uint8_t *)malloc((size_t)cap);
    if (!buf) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    int w = 0;
    int i = 0;
    bool inicio_palabra = true;
    while (i < sl) {
        utf8proc_int32_t cp;
        utf8proc_ssize_t cons = utf8proc_iterate(
            (const utf8proc_uint8_t *)(s + i), sl - i, &cp);
        if (cons <= 0) {
            if (w + 1 > cap) {
                cap *= 2;
                uint8_t *nb = (uint8_t *)realloc(buf, (size_t)cap);
                if (!nb) { free(buf); return error_nativa(err, linea, columna, "memoria insuficiente"); }
                buf = nb;
            }
            buf[w++] = (uint8_t)s[i++];
            continue;
        }
        int cat = utf8proc_category(cp);
        bool es_letra = (cat == UTF8PROC_CATEGORY_LU || cat == UTF8PROC_CATEGORY_LL
                          || cat == UTF8PROC_CATEGORY_LT || cat == UTF8PROC_CATEGORY_LM
                          || cat == UTF8PROC_CATEGORY_LO);
        utf8proc_int32_t mapped = cp;
        if (es_letra) {
            mapped = inicio_palabra
                ? utf8proc_toupper(cp)
                : utf8proc_tolower(cp);
            inicio_palabra = false;
        } else {
            inicio_palabra = true;
        }
        if (w + 4 > cap) {
            cap = cap * 2 + 4;
            uint8_t *nb = (uint8_t *)realloc(buf, (size_t)cap);
            if (!nb) { free(buf); return error_nativa(err, linea, columna, "memoria insuficiente"); }
            buf = nb;
        }
        utf8proc_ssize_t n = utf8proc_encode_char(mapped, &buf[w]);
        if (n <= 0) {
            n = utf8proc_encode_char(cp, &buf[w]);
            if (n <= 0) n = 0;
        }
        w += (int)n;
        i += (int)cons;
    }
    Valor r = valor_cadena_duplicar((const char *)buf, w);
    free(buf);
    return r;
}

static Valor nativa_cadena_mayusculas(EvalError *err, int n_args, Valor *args,
                                          int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: mayusculas() no acepta argumentos");
    }
    return cadena_caso_unicode(err, &args[0], false, linea, columna);
}

/* v1.157: cadena.dividir_palabras() — divide por cualquier secuencia
 * de whitespace (espacios, \t, \n, \r, \f, \v + Unicode Z*),
 * descartando whitespace al inicio y al final. NO produce cadenas
 * vacias entre runs de whitespace. Paridad con Python str.split()
 * sin argumentos.
 *
 * Diferencia con separar(" "): este NO emite cadenas vacias entre
 * espacios consecutivos. "a  b".dividir_palabras() -> ["a", "b"]
 * vs "a  b".separar(" ") -> ["a", "", "b"]. */
static Valor nativa_cadena_dividir_palabras(EvalError *err, int n_args,
                                                 Valor *args,
                                                 int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: dividir_palabras() no acepta argumentos");
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: dividir_palabras() requiere una cadena, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    const char *s = args[0].como.cadena.texto;
    int sl = args[0].como.cadena.longitud;
    Lista *l = lista_nueva(0);
    if (!l) return error_nativa(err, linea, columna, "memoria insuficiente");

    int i = 0;
    while (i < sl) {
        /* Saltar whitespace. */
        while (i < sl) {
            utf8proc_int32_t cp;
            utf8proc_ssize_t cons = utf8proc_iterate(
                (const utf8proc_uint8_t *)(s + i), sl - i, &cp);
            if (cons <= 0) break;
            int cat = utf8proc_category(cp);
            bool es_ws = (cat == UTF8PROC_CATEGORY_ZS
                          || cat == UTF8PROC_CATEGORY_ZL
                          || cat == UTF8PROC_CATEGORY_ZP
                          || cp == '\t' || cp == '\n' || cp == '\r'
                          || cp == '\f' || cp == '\v');
            if (!es_ws) break;
            i += (int)cons;
        }
        if (i >= sl) break;
        /* Capturar palabra hasta el siguiente whitespace. */
        int inicio = i;
        while (i < sl) {
            utf8proc_int32_t cp;
            utf8proc_ssize_t cons = utf8proc_iterate(
                (const utf8proc_uint8_t *)(s + i), sl - i, &cp);
            if (cons <= 0) { i++; continue; }
            int cat = utf8proc_category(cp);
            bool es_ws = (cat == UTF8PROC_CATEGORY_ZS
                          || cat == UTF8PROC_CATEGORY_ZL
                          || cat == UTF8PROC_CATEGORY_ZP
                          || cp == '\t' || cp == '\n' || cp == '\r'
                          || cp == '\f' || cp == '\v');
            if (es_ws) break;
            i += (int)cons;
        }
        Valor v = valor_cadena_duplicar(s + inicio, i - inicio);
        if (!lista_agregar(l, v)) {
            lista_liberar(l);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
    }
    return valor_lista(l);
}

/* v1.157: cadena.rellenar_ceros(ancho) — rellena con '0' por la
 * izquierda hasta alcanzar `ancho` code-points. Si la cadena
 * empieza con '+' o '-', el signo se queda al frente y los ceros
 * van DESPUES (paridad con Python str.zfill).
 *
 * "5".rellenar_ceros(4)   -> "0005"
 * "-5".rellenar_ceros(4)  -> "-005"  (no "00-5")
 * "+5".rellenar_ceros(4)  -> "+005"
 * "12345".rellenar_ceros(3) -> "12345"  (sin cambios, ya cabe) */
static Valor nativa_cadena_rellenar_ceros(EvalError *err, int n_args,
                                                Valor *args,
                                                int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: rellenar_ceros(ancho) requiere 1 argumento");
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: rellenar_ceros() requiere una cadena, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    int64_t ancho_i64;
    if (!valor_entero_a_i64(&args[1], &ancho_i64)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: rellenar_ceros() requiere un entero como ancho");
    }
    if (ancho_i64 < 0 || ancho_i64 > 1000000) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: ancho fuera de rango [0, 1_000_000]");
    }
    int ancho = (int)ancho_i64;
    const char *s = args[0].como.cadena.texto;
    int sl = args[0].como.cadena.longitud;

    /* Contar code-points actuales. */
    int cps = 0;
    {
        int p = 0;
        while (p < sl) {
            utf8proc_int32_t cp;
            utf8proc_ssize_t cons = utf8proc_iterate(
                (const utf8proc_uint8_t *)(s + p), sl - p, &cp);
            if (cons <= 0) { p++; cps++; continue; }
            p += (int)cons;
            cps++;
        }
    }
    if (ancho <= cps) {
        return valor_cadena_duplicar(s, sl);
    }
    int n_ceros = ancho - cps;
    int signo_len = 0;
    if (sl > 0 && (s[0] == '+' || s[0] == '-')) signo_len = 1;
    int total_bytes = sl + n_ceros;
    char *buf = (char *)malloc((size_t)total_bytes);
    if (!buf) return error_nativa(err, linea, columna, "memoria insuficiente");
    int w = 0;
    if (signo_len) buf[w++] = s[0];
    for (int i = 0; i < n_ceros; i++) buf[w++] = '0';
    memcpy(buf + w, s + signo_len, (size_t)(sl - signo_len));
    w += sl - signo_len;
    Valor r = valor_cadena_duplicar(buf, w);
    free(buf);
    return r;
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
 * v1.122: nativas adicionales para cubrir metodos comunes sobre
 * tipos built-in (cadena.separar, cadena.reemplazar, cadena.recortar,
 * cadena.contiene, lista.contar, lista.contiene, lista.copiar,
 * dict.items, dict.obtener). Diseñadas para encajar en la tabla
 * METODOS_NATIVOS sin necesidad de wrappers en stdlib.
 * ────────────────────────────────────────────────────────────────── */

/* cadena_separar(s, sep) -> lista de cadenas.
 *
 * Si sep es vacio, devuelve lista de caracteres (cada code-point una
 * cadena). En caso normal, busca sep como substring en s y particiona.
 * Resultado consistente con cadenas.separar de la stdlib pure pero
 * O(n) en bytes (la version pura es O(n^2) por la concatenacion). */
static Valor nativa_cadena_separar(EvalError *err, int n_args, Valor *args,
                                      int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_separar(s, sep) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_separar() requiere cadenas");
    }
    const char *s = args[0].como.cadena.texto;
    int sl = args[0].como.cadena.longitud;
    const char *sep = args[1].como.cadena.texto;
    int sepl = args[1].como.cadena.longitud;

    Lista *l = lista_nueva(0);
    if (!l) return error_nativa(err, linea, columna, "memoria insuficiente");

    if (sepl == 0) {
        /* sep vacio: separar por code-point. */
        int p = 0;
        while (p < sl) {
            utf8proc_int32_t cp;
            utf8proc_ssize_t cons = utf8proc_iterate(
                (const utf8proc_uint8_t *)(s + p), sl - p, &cp);
            if (cons <= 0) { p++; continue; }
            Valor v = valor_cadena_duplicar(s + p, (int)cons);
            if (!lista_agregar(l, v)) {
                lista_liberar(l);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            p += (int)cons;
        }
        return valor_lista(l);
    }

    int i = 0;
    while (i <= sl) {
        int j = i;
        bool encontrado = false;
        while (j <= sl - sepl) {
            if (s[j] == sep[0] && memcmp(s + j, sep, (size_t)sepl) == 0) {
                encontrado = true;
                break;
            }
            j++;
        }
        int fin = encontrado ? j : sl;
        Valor v = valor_cadena_duplicar(s + i, fin - i);
        if (!lista_agregar(l, v)) {
            lista_liberar(l);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        if (!encontrado) break;
        i = j + sepl;
        if (i == sl) {
            /* sep al final: emit cadena vacia. */
            Valor vacio = valor_cadena_duplicar("", 0);
            if (!lista_agregar(l, vacio)) {
                lista_liberar(l);
                return error_nativa(err, linea, columna, "memoria insuficiente");
            }
            break;
        }
    }
    return valor_lista(l);
}

/* cadena_reemplazar(s, viejo, nuevo) -> cadena con todas las
 * ocurrencias de `viejo` reemplazadas por `nuevo`. O(n). Si viejo es
 * vacio devuelve s tal cual (evita bucle infinito). */
static Valor nativa_cadena_reemplazar(EvalError *err, int n_args, Valor *args,
                                         int linea, int columna) {
    if (n_args != 3) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_reemplazar(s, viejo, nuevo) requiere 3 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA
        || args[2].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_reemplazar() requiere cadenas");
    }
    const char *s = args[0].como.cadena.texto;
    int sl = args[0].como.cadena.longitud;
    const char *viejo = args[1].como.cadena.texto;
    int vl = args[1].como.cadena.longitud;
    if (vl == 0) {
        return valor_cadena_duplicar(s, sl);
    }
    const char *nuevo = args[2].como.cadena.texto;
    int nl = args[2].como.cadena.longitud;

    /* Pasada 1: contar ocurrencias para alocar el buffer una sola vez. */
    int ocurr = 0;
    int p = 0;
    while (p <= sl - vl) {
        if (s[p] == viejo[0] && memcmp(s + p, viejo, (size_t)vl) == 0) {
            ocurr++;
            p += vl;
        } else {
            p++;
        }
    }
    long total = (long)sl + (long)ocurr * ((long)nl - (long)vl);
    char *out = (char *)malloc((size_t)total + 1);
    if (!out) return error_nativa(err, linea, columna, "memoria insuficiente");
    long w = 0;
    p = 0;
    while (p < sl) {
        if (p <= sl - vl && s[p] == viejo[0]
            && memcmp(s + p, viejo, (size_t)vl) == 0) {
            memcpy(out + w, nuevo, (size_t)nl);
            w += nl;
            p += vl;
        } else {
            out[w++] = s[p++];
        }
    }
    out[w] = '\0';
    Valor r = valor_cadena_duplicar(out, (int)w);
    free(out);
    return r;
}

/* cadena_recortar(s) -> cadena sin whitespace en los extremos.
 * Whitespace: espacio, tab, \n, \r, \f, \v (ASCII). UTF-8 multibyte
 * con bit 0x80 set no se considera whitespace. */
static Valor nativa_cadena_recortar(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_recortar(s) requiere 1 argumento");
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_recortar() requiere una cadena");
    }
    const char *s = args[0].como.cadena.texto;
    int sl = args[0].como.cadena.longitud;
    int ini = 0, fin = sl;
    while (ini < fin) {
        unsigned char c = (unsigned char)s[ini];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r'
            || c == '\f' || c == '\v') ini++;
        else break;
    }
    while (fin > ini) {
        unsigned char c = (unsigned char)s[fin - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r'
            || c == '\f' || c == '\v') fin--;
        else break;
    }
    return valor_cadena_duplicar(s + ini, fin - ini);
}

/* v1.152: cadena.dividir_lineas() — divide por '\n' descartando la
 * linea vacia final que generaria `\n` al final. Tambien acepta
 * "\r\n" (Windows) y "\r" (legacy Mac). Paridad con Python
 * str.splitlines(). Sin parametros — siempre conserva contenido
 * (los terminadores se descartan). */
static Valor nativa_cadena_dividir_lineas(EvalError *err, int n_args,
                                              Valor *args,
                                              int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: dividir_lineas() no acepta argumentos");
    }
    if (args[0].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: dividir_lineas() requiere una cadena");
    }
    const char *s = args[0].como.cadena.texto;
    int sl = args[0].como.cadena.longitud;
    Lista *l = lista_nueva(0);
    if (!l) return error_nativa(err, linea, columna, "memoria insuficiente");
    int i = 0;
    while (i < sl) {
        int j = i;
        while (j < sl && s[j] != '\n' && s[j] != '\r') j++;
        Valor v = valor_cadena_duplicar(s + i, j - i);
        if (!lista_agregar(l, v)) {
            lista_liberar(l);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        if (j >= sl) break;
        /* Consumir terminador: \r\n, \n, o \r solo. */
        if (s[j] == '\r' && j + 1 < sl && s[j + 1] == '\n') {
            i = j + 2;
        } else {
            i = j + 1;
        }
    }
    return valor_lista(l);
}

/* Helper compartido para centrar/alinear. Devuelve nueva cadena
 * rellenada con `rel` hasta longitud `ancho` (en code points UTF-8).
 * `alineacion`: '<' izq, '>' der, '^' centro. Si la cadena ya tiene
 * ancho >= solicitado, se devuelve sin modificar (clonada). */
static Valor fmt_alinear_cadena(EvalError *err, const char *s, int sl,
                                  int ancho, char alineacion, char rel,
                                  int linea, int columna) {
    /* Contar code points en s. */
    int cps = 0;
    {
        int p = 0;
        while (p < sl) {
            utf8proc_int32_t cp;
            utf8proc_ssize_t cons = utf8proc_iterate(
                (const utf8proc_uint8_t *)(s + p), sl - p, &cp);
            if (cons <= 0) { p++; cps++; continue; }
            p += (int)cons;
            cps++;
        }
    }
    if (ancho <= cps) {
        return valor_cadena_duplicar(s, sl);
    }
    int n_rel = ancho - cps;
    int pad_izq = 0, pad_der = 0;
    if (alineacion == '<') { pad_der = n_rel; }
    else if (alineacion == '>') { pad_izq = n_rel; }
    else { pad_izq = n_rel / 2; pad_der = n_rel - pad_izq; }
    int total_len = sl + pad_izq + pad_der;
    char *buf = (char *)malloc((size_t)total_len);
    if (!buf) {
        return error_nativa(err, linea, columna, "memoria insuficiente");
    }
    int w = 0;
    for (int i = 0; i < pad_izq; i++) buf[w++] = rel;
    memcpy(buf + w, s, (size_t)sl);
    w += sl;
    for (int i = 0; i < pad_der; i++) buf[w++] = rel;
    Valor r = valor_cadena_duplicar(buf, total_len);
    free(buf);
    return r;
}

static bool fmt_args_alinear(EvalError *err, int n_args, Valor *args,
                               int linea, int columna,
                               int *out_ancho, char *out_rel,
                               const char *nombre) {
    if (n_args < 2 || n_args > 3) {
        error_nativa(err, linea, columna,
            "ErrorDeTipo: %s(s, ancho[, relleno]) requiere 2 o 3 argumentos",
            nombre);
        return false;
    }
    if (args[0].tipo != VAL_CADENA) {
        error_nativa(err, linea, columna,
            "ErrorDeTipo: %s() requiere una cadena, no '%s'",
            nombre, valor_nombre_tipo(&args[0]));
        return false;
    }
    int64_t ancho_i64;
    if (!valor_entero_a_i64(&args[1], &ancho_i64)) {
        error_nativa(err, linea, columna,
            "ErrorDeTipo: %s() requiere un entero como ancho", nombre);
        return false;
    }
    if (ancho_i64 < 0 || ancho_i64 > 1000000) {
        error_nativa(err, linea, columna,
            "ErrorDeValor: ancho fuera de rango [0, 1_000_000]");
        return false;
    }
    *out_ancho = (int)ancho_i64;
    *out_rel = ' ';
    if (n_args == 3) {
        if (args[2].tipo != VAL_CADENA || args[2].como.cadena.longitud != 1) {
            error_nativa(err, linea, columna,
                "ErrorDeValor: relleno debe ser una cadena de 1 caracter");
            return false;
        }
        *out_rel = args[2].como.cadena.texto[0];
    }
    return true;
}

/* v1.152: cadena.centrar(ancho[, relleno=" "]) — centra la cadena
 * en `ancho` code points rellenando con `relleno`. Si la cadena ya
 * es mas larga, se devuelve sin modificar. Paridad con Python
 * str.center(). */
static Valor nativa_cadena_centrar(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    int ancho; char rel;
    if (!fmt_args_alinear(err, n_args, args, linea, columna,
                            &ancho, &rel, "centrar")) {
        return valor_nulo();
    }
    return fmt_alinear_cadena(err,
        args[0].como.cadena.texto, args[0].como.cadena.longitud,
        ancho, '^', rel, linea, columna);
}

/* v1.152: cadena.alinear_izquierda(ancho[, relleno]) — paridad con
 * str.ljust(). */
static Valor nativa_cadena_alinear_izquierda(EvalError *err, int n_args,
                                                 Valor *args,
                                                 int linea, int columna) {
    int ancho; char rel;
    if (!fmt_args_alinear(err, n_args, args, linea, columna,
                            &ancho, &rel, "alinear_izquierda")) {
        return valor_nulo();
    }
    return fmt_alinear_cadena(err,
        args[0].como.cadena.texto, args[0].como.cadena.longitud,
        ancho, '<', rel, linea, columna);
}

/* v1.152: cadena.alinear_derecha(ancho[, relleno]) — paridad con
 * str.rjust(). */
static Valor nativa_cadena_alinear_derecha(EvalError *err, int n_args,
                                                Valor *args,
                                                int linea, int columna) {
    int ancho; char rel;
    if (!fmt_args_alinear(err, n_args, args, linea, columna,
                            &ancho, &rel, "alinear_derecha")) {
        return valor_nulo();
    }
    return fmt_alinear_cadena(err,
        args[0].como.cadena.texto, args[0].como.cadena.longitud,
        ancho, '>', rel, linea, columna);
}

/* cadena_contiene(s, sub) -> booleano. Wrapper de indice_de >= 0. */
static Valor nativa_cadena_contiene(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_contiene(s, sub) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CADENA || args[1].tipo != VAL_CADENA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena_contiene() requiere cadenas");
    }
    int sl = args[0].como.cadena.longitud;
    const char *sub = args[1].como.cadena.texto;
    int subl = args[1].como.cadena.longitud;
    if (subl == 0) return valor_booleano(true);
    if (subl > sl) return valor_booleano(false);
    const char *s = args[0].como.cadena.texto;
    int max_i = sl - subl;
    for (int i = 0; i <= max_i; i++) {
        if (s[i] == sub[0] && memcmp(s + i, sub, (size_t)subl) == 0) {
            return valor_booleano(true);
        }
    }
    return valor_booleano(false);
}

/* cadena_unir_metodo(sep, lista) -> cadena. Adapter del metodo
 * `cadena.unir(lista)`. La nativa global cadena_unir(lista, sep)
 * espera args invertidos; aqui reordenamos y delegamos. */
static Valor nativa_cadena_unir_metodo(EvalError *err, int n_args, Valor *args,
                                          int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: cadena.unir(lista) requiere 1 argumento (mas el receptor)");
    }
    /* args[0] = sep (receptor), args[1] = lista. Reordenar a (lista, sep). */
    Valor reord[2];
    reord[0] = args[1];
    reord[1] = args[0];
    return nativa_cadena_unir(err, 2, reord, linea, columna);
}

/* lista_contar(xs, x) -> entero. Cuenta apariciones de `x` en `xs`
 * usando valor_iguales (semantica de ==). */
static Valor nativa_lista_contar(EvalError *err, int n_args, Valor *args,
                                    int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: lista_contar(xs, x) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: lista_contar() requiere lista como primer arg");
    }
    Lista *l = args[0].como.lista;
    long cnt = 0;
    for (int i = 0; i < l->cuenta; i++) {
        if (valor_iguales(&l->elementos[i], &args[1])) cnt++;
    }
    return valor_entero_de_i64(cnt);
}

/* lista_contiene(xs, x) -> booleano. Igual semantica que el operador
 * `x en xs`. Wrapper explicito para usarse como metodo. */
static Valor nativa_lista_contiene(EvalError *err, int n_args, Valor *args,
                                      int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: lista_contiene(xs, x) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: lista_contiene() requiere lista como primer arg");
    }
    Lista *l = args[0].como.lista;
    for (int i = 0; i < l->cuenta; i++) {
        if (valor_iguales(&l->elementos[i], &args[1])) {
            return valor_booleano(true);
        }
    }
    return valor_booleano(false);
}

/* lista_copiar(xs) -> lista nueva con los mismos elementos (shallow
 * copy). Equivalente a `xs[0:]`. */
static Valor nativa_lista_copiar(EvalError *err, int n_args, Valor *args,
                                    int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: lista_copiar(xs) requiere 1 argumento");
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: lista_copiar() requiere una lista");
    }
    Lista *src = args[0].como.lista;
    Lista *nueva = lista_nueva(src->cuenta);
    if (!nueva) return error_nativa(err, linea, columna, "memoria insuficiente");
    for (int i = 0; i < src->cuenta; i++) {
        if (!lista_agregar(nueva, valor_clonar(&src->elementos[i]))) {
            lista_liberar(nueva);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
    }
    return valor_lista(nueva);
}

/* v1.155: lista.vaciar() — quita todos los elementos in-place,
 * dejando la lista como []. Paridad con Python list.clear(). */
static Valor nativa_lista_vaciar(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: vaciar() no acepta argumentos");
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: vaciar() requiere una lista, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Lista *l = args[0].como.lista;
    for (int i = 0; i < l->cuenta; i++) {
        valor_destruir(&l->elementos[i]);
    }
    l->cuenta = 0;
    return valor_nulo();
}

/* v1.155: lista.extender(iterable) — agrega todos los elementos
 * de `iterable` al final de la lista, in-place. Paridad con Python
 * list.extend(). Acepta lista, tupla, cadena, rango y conjunto. */
static Valor nativa_lista_extender(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: extender(otro) requiere 1 argumento");
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: extender() requiere una lista como receptor, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Lista *dst = args[0].como.lista;
    const Valor *it = &args[1];
    if (it->tipo == VAL_LISTA) {
        Lista *src = it->como.lista;
        /* Cuidado: si dst == src, evitar bucle infinito iterando una
         * snapshot. Tomamos la cuenta de src ANTES de empezar a
         * agregar. */
        int n_src = src->cuenta;
        for (int i = 0; i < n_src; i++) {
            if (!lista_agregar(dst, valor_clonar(&src->elementos[i]))) {
                return error_nativa(err, linea, columna,
                    "memoria insuficiente al extender lista");
            }
        }
    } else if (it->tipo == VAL_TUPLA) {
        Tupla *t = it->como.tupla;
        for (int i = 0; i < t->cuenta; i++) {
            if (!lista_agregar(dst, valor_clonar(&t->elementos[i]))) {
                return error_nativa(err, linea, columna,
                    "memoria insuficiente al extender lista");
            }
        }
    } else if (it->tipo == VAL_CADENA) {
        /* Cadena → cada code-point UTF-8 como cadena de 1 cp. */
        const char *s = it->como.cadena.texto;
        int sl = it->como.cadena.longitud;
        int p = 0;
        while (p < sl) {
            utf8proc_int32_t cp;
            utf8proc_ssize_t cons = utf8proc_iterate(
                (const utf8proc_uint8_t *)(s + p), sl - p, &cp);
            if (cons <= 0) { p++; continue; }
            Valor cv = valor_cadena_duplicar(s + p, (int)cons);
            if (!lista_agregar(dst, cv)) {
                return error_nativa(err, linea, columna,
                    "memoria insuficiente al extender lista");
            }
            p += (int)cons;
        }
    } else if (it->tipo == VAL_CONJUNTO) {
        Conjunto *c = it->como.conjunto;
        for (int i = 0; i < c->capacidad; i++) {
            const EntradaConjunto *e = &c->entradas[i];
            if (!e->ocupada) continue;
            if (!lista_agregar(dst, valor_clonar(&e->elemento))) {
                return error_nativa(err, linea, columna,
                    "memoria insuficiente al extender lista");
            }
        }
    } else {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: extender() requiere un iterable (lista/tupla/cadena/conjunto), no '%s'",
            valor_nombre_tipo(it));
    }
    return valor_nulo();
}

/* dict_items(d) -> lista de [clave, valor] en orden de insercion.
 * Patron canonico para iterar pares de un diccionario. */
static Valor nativa_dict_items(EvalError *err, int n_args, Valor *args,
                                  int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: dict_items(d) requiere 1 argumento");
    }
    if (args[0].tipo != VAL_DICCIONARIO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: dict_items() requiere un diccionario");
    }
    Diccionario *d = args[0].como.dicc;
    Lista *l = lista_nueva(d->cuenta);
    if (!l) return error_nativa(err, linea, columna, "memoria insuficiente");
    for (int i = 0; i < d->cuenta; i++) {
        int slot = d->orden_insercion[i];
        Lista *par = lista_nueva(2);
        if (!par) { lista_liberar(l);
            return error_nativa(err, linea, columna, "memoria insuficiente"); }
        if (!lista_agregar(par, valor_clonar(&d->entradas[slot].clave))
            || !lista_agregar(par, valor_clonar(&d->entradas[slot].valor))) {
            lista_liberar(par); lista_liberar(l);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
        if (!lista_agregar(l, valor_lista(par))) {
            lista_liberar(par); lista_liberar(l);
            return error_nativa(err, linea, columna, "memoria insuficiente");
        }
    }
    return valor_lista(l);
}

/* dict_obtener(d, k, defecto) -> valor en d[k], o defecto si no existe.
 * NO lanza ErrorDeClave. */
static Valor nativa_dict_obtener(EvalError *err, int n_args, Valor *args,
                                    int linea, int columna) {
    if (n_args != 3) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: dict_obtener(d, k, defecto) requiere 3 argumentos");
    }
    if (args[0].tipo != VAL_DICCIONARIO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: dict_obtener() requiere un diccionario");
    }
    Valor out;
    if (dicc_obtener(args[0].como.dicc, &args[1], &out)) {
        return out;
    }
    return valor_clonar(&args[2]);
}

/* v1.151: dicc.sacar(k) o dicc.sacar(k, default) — quita la clave `k`
 * del dict y devuelve su valor. Si no existe:
 *   - 2 args (sin default): lanza ErrorDeClave.
 *   - 3 args (con default): devuelve `default` sin lanzar.
 * Muta el dict in-place. */
static Valor nativa_dict_sacar(EvalError *err, int n_args, Valor *args,
                                  int linea, int columna) {
    if (n_args < 2 || n_args > 3) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: sacar(d, k[, defecto]) requiere 2 o 3 argumentos");
    }
    if (args[0].tipo != VAL_DICCIONARIO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: sacar() requiere un diccionario, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Valor out;
    if (dicc_quitar(args[0].como.dicc, &args[1], &out)) {
        return out;  /* dicc_quitar transfiere ownership */
    }
    /* Clave ausente. */
    if (n_args == 3) {
        return valor_clonar(&args[2]);
    }
    /* Sin default: lanzar ErrorDeClave con la clave en el mensaje. */
    char buf[256];
    valor_a_cadena(&args[1], buf, sizeof(buf));
    return error_nativa(err, linea, columna,
        "ErrorDeClave: %s", buf);
}

/* v1.151: dicc.vaciar() — quita todas las entradas in-place.
 * Devuelve nulo. */
static Valor nativa_dict_vaciar(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: vaciar() no acepta argumentos");
    }
    if (args[0].tipo != VAL_DICCIONARIO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: vaciar() requiere un diccionario, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Diccionario *d = args[0].como.dicc;
    /* Recorrer entradas y liberar las ocupadas. */
    for (int i = 0; i < d->capacidad; i++) {
        EntradaDicc *e = &d->entradas[i];
        if (e->ocupada) {
            valor_destruir(&e->clave);
            valor_destruir(&e->valor);
            e->ocupada = false;
        }
    }
    d->cuenta = 0;
    d->version++;
    return valor_nulo();
}

/* v1.150: dicc.actualizar(otro) — copia todas las parejas de `otro`
 * a `args[0]`, sobrescribiendo las claves que ya existian. Devuelve
 * nulo (mutacion in-place, paridad con Python dict.update). */
static Valor nativa_dict_actualizar(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: actualizar(d, otro) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_DICCIONARIO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: actualizar() requiere un diccionario como receptor, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    if (args[1].tipo != VAL_DICCIONARIO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: actualizar() requiere un diccionario como argumento, no '%s'",
            valor_nombre_tipo(&args[1]));
    }
    Diccionario *dst = args[0].como.dicc;
    Diccionario *src = args[1].como.dicc;
    /* Iterar el src y copiar cada (clave, valor) a dst. */
    for (int i = 0; i < src->capacidad; i++) {
        const EntradaDicc *e = &src->entradas[i];
        if (!e->ocupada) continue;
        if (!dicc_asignar(dst, valor_clonar(&e->clave),
                              valor_clonar(&e->valor))) {
            return error_nativa(err, linea, columna,
                "memoria insuficiente al actualizar diccionario");
        }
    }
    return valor_nulo();
}

/* ──────────────────────────────────────────────────────────────────
 * v1.128: nativas adicionales para metodos sobre conjunto y tupla.
 *
 * Antes de v1.128 la tabla METODOS_NATIVOS no tenia entradas para
 * VAL_CONJUNTO ni VAL_TUPLA. Esta release completa las operaciones
 * canonicas de conjunto (union/interseccion/diferencia/es_subconjunto)
 * mas un wrapper de pertenencia, y para tupla los tres metodos comunes
 * de secuencia inmutable (contar/contiene/indice_de).
 * ────────────────────────────────────────────────────────────────── */

/* Helper privado: itera todos los elementos de `c` y los agrega a
 * `dst`. Toma cada elemento con valor_clonar. */
static bool _conj_copiar_todos(Conjunto *dst, const Conjunto *c,
                                 EvalError *err, int linea, int columna) {
    for (int i = 0; i < c->capacidad; i++) {
        const EntradaConjunto *e = &c->entradas[i];
        if (!e->ocupada) continue;
        if (!conj_agregar(dst, valor_clonar(&e->elemento))) {
            error_nativa(err, linea, columna, "memoria insuficiente");
            return false;
        }
    }
    return true;
}

/* conjunto_union(a, b) -> conjunto nuevo con todos los elementos de
 * a y b. */
static Valor nativa_conjunto_union(EvalError *err, int n_args, Valor *args,
                                      int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_union(a, b) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CONJUNTO || args[1].tipo != VAL_CONJUNTO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_union() requiere dos conjuntos");
    }
    Conjunto *r = conj_nuevo();
    if (!r) return error_nativa(err, linea, columna, "memoria insuficiente");
    if (!_conj_copiar_todos(r, args[0].como.conjunto, err, linea, columna)
        || !_conj_copiar_todos(r, args[1].como.conjunto, err, linea, columna)) {
        conj_liberar(r);
        return valor_nulo();
    }
    return valor_conjunto(r);
}

/* conjunto_interseccion(a, b) -> conjunto nuevo con elementos en a Y b. */
static Valor nativa_conjunto_interseccion(EvalError *err, int n_args,
                                              Valor *args,
                                              int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_interseccion(a, b) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CONJUNTO || args[1].tipo != VAL_CONJUNTO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_interseccion() requiere dos conjuntos");
    }
    Conjunto *a = args[0].como.conjunto;
    Conjunto *b = args[1].como.conjunto;
    /* Iterar sobre el mas pequeno para minimizar trabajo (O(min)). */
    Conjunto *menor = a, *mayor = b;
    if (b->cuenta < a->cuenta) { menor = b; mayor = a; }
    Conjunto *r = conj_nuevo();
    if (!r) return error_nativa(err, linea, columna, "memoria insuficiente");
    for (int i = 0; i < menor->capacidad; i++) {
        const EntradaConjunto *e = &menor->entradas[i];
        if (!e->ocupada) continue;
        if (conj_contiene(mayor, &e->elemento)) {
            if (!conj_agregar(r, valor_clonar(&e->elemento))) {
                conj_liberar(r);
                return error_nativa(err, linea, columna,
                    "memoria insuficiente");
            }
        }
    }
    return valor_conjunto(r);
}

/* conjunto_diferencia(a, b) -> conjunto nuevo con elementos en a pero NO en b. */
static Valor nativa_conjunto_diferencia(EvalError *err, int n_args,
                                            Valor *args,
                                            int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_diferencia(a, b) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CONJUNTO || args[1].tipo != VAL_CONJUNTO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_diferencia() requiere dos conjuntos");
    }
    Conjunto *a = args[0].como.conjunto;
    Conjunto *b = args[1].como.conjunto;
    Conjunto *r = conj_nuevo();
    if (!r) return error_nativa(err, linea, columna, "memoria insuficiente");
    for (int i = 0; i < a->capacidad; i++) {
        const EntradaConjunto *e = &a->entradas[i];
        if (!e->ocupada) continue;
        if (!conj_contiene(b, &e->elemento)) {
            if (!conj_agregar(r, valor_clonar(&e->elemento))) {
                conj_liberar(r);
                return error_nativa(err, linea, columna,
                    "memoria insuficiente");
            }
        }
    }
    return valor_conjunto(r);
}

/* conjunto_es_subconjunto(a, b) -> verdadero si a ⊆ b. */
static Valor nativa_conjunto_es_subconjunto(EvalError *err, int n_args,
                                                Valor *args,
                                                int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_es_subconjunto(a, b) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CONJUNTO || args[1].tipo != VAL_CONJUNTO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_es_subconjunto() requiere dos conjuntos");
    }
    Conjunto *a = args[0].como.conjunto;
    Conjunto *b = args[1].como.conjunto;
    if (a->cuenta > b->cuenta) return valor_booleano(false);
    for (int i = 0; i < a->capacidad; i++) {
        const EntradaConjunto *e = &a->entradas[i];
        if (!e->ocupada) continue;
        if (!conj_contiene(b, &e->elemento)) return valor_booleano(false);
    }
    return valor_booleano(true);
}

/* conjunto_contiene(s, x) -> booleano. Wrapper de conj_contiene. */
static Valor nativa_conjunto_contiene(EvalError *err, int n_args, Valor *args,
                                          int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_contiene(s, x) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_CONJUNTO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_contiene() requiere un conjunto");
    }
    if (!valor_es_hashable(&args[1])) {
        return valor_booleano(false);
    }
    return valor_booleano(conj_contiene(args[0].como.conjunto, &args[1]));
}

/* conjunto_copiar(s) -> conjunto nuevo con los mismos elementos (shallow). */
static Valor nativa_conjunto_copiar(EvalError *err, int n_args, Valor *args,
                                        int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_copiar(s) requiere 1 argumento");
    }
    if (args[0].tipo != VAL_CONJUNTO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: conjunto_copiar() requiere un conjunto");
    }
    Conjunto *r = conj_nuevo();
    if (!r) return error_nativa(err, linea, columna, "memoria insuficiente");
    if (!_conj_copiar_todos(r, args[0].como.conjunto, err, linea, columna)) {
        conj_liberar(r);
        return valor_nulo();
    }
    return valor_conjunto(r);
}

/* v1.156: conjunto.vaciar() — quita todas las entradas in-place.
 * Paridad con Python set.clear(). */
static Valor nativa_conjunto_vaciar(EvalError *err, int n_args, Valor *args,
                                         int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: vaciar() no acepta argumentos");
    }
    if (args[0].tipo != VAL_CONJUNTO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: vaciar() requiere un conjunto, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    if (args[0].como.conjunto->congelado) {
        return error_conjunto_congelado(err, linea, columna, "vaciar");
    }
    Conjunto *c = args[0].como.conjunto;
    for (int i = 0; i < c->capacidad; i++) {
        EntradaConjunto *e = &c->entradas[i];
        if (e->ocupada) {
            valor_destruir(&e->elemento);
            e->ocupada = false;
        }
    }
    c->cuenta = 0;
    return valor_nulo();
}

/* v1.156: conjunto.actualizar(iterable) — agrega todos los
 * elementos de `iterable` al conjunto. Paridad con Python
 * set.update(). Acepta conjunto, lista, tupla, cadena. */
static Valor nativa_conjunto_actualizar(EvalError *err, int n_args, Valor *args,
                                            int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: actualizar(otro) requiere 1 argumento");
    }
    if (args[0].tipo != VAL_CONJUNTO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: actualizar() requiere un conjunto como receptor, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    if (args[0].como.conjunto->congelado) {
        return error_conjunto_congelado(err, linea, columna, "actualizar");
    }
    Conjunto *dst = args[0].como.conjunto;
    const Valor *it = &args[1];
    if (it->tipo == VAL_CONJUNTO) {
        Conjunto *src = it->como.conjunto;
        /* No es self-update peligroso: conj_agregar puede rehash, pero
         * iteramos sobre snapshot del array hasta capacidad. Si dst ==
         * src, los elementos ya estan presentes asi que agregar es
         * idempotente. */
        for (int i = 0; i < src->capacidad; i++) {
            const EntradaConjunto *e = &src->entradas[i];
            if (!e->ocupada) continue;
            if (!valor_es_hashable(&e->elemento)) {
                return error_nativa(err, linea, columna,
                    "ErrorDeTipo: '%s' no es hashable",
                    valor_nombre_tipo(&e->elemento));
            }
            if (!conj_agregar(dst, valor_clonar(&e->elemento))) {
                return error_nativa(err, linea, columna,
                    "memoria insuficiente al actualizar conjunto");
            }
        }
    } else if (it->tipo == VAL_LISTA) {
        Lista *l = it->como.lista;
        for (int i = 0; i < l->cuenta; i++) {
            if (!valor_es_hashable(&l->elementos[i])) {
                return error_nativa(err, linea, columna,
                    "ErrorDeTipo: '%s' no es hashable",
                    valor_nombre_tipo(&l->elementos[i]));
            }
            if (!conj_agregar(dst, valor_clonar(&l->elementos[i]))) {
                return error_nativa(err, linea, columna,
                    "memoria insuficiente al actualizar conjunto");
            }
        }
    } else if (it->tipo == VAL_TUPLA) {
        Tupla *t = it->como.tupla;
        for (int i = 0; i < t->cuenta; i++) {
            if (!valor_es_hashable(&t->elementos[i])) {
                return error_nativa(err, linea, columna,
                    "ErrorDeTipo: '%s' no es hashable",
                    valor_nombre_tipo(&t->elementos[i]));
            }
            if (!conj_agregar(dst, valor_clonar(&t->elementos[i]))) {
                return error_nativa(err, linea, columna,
                    "memoria insuficiente al actualizar conjunto");
            }
        }
    } else if (it->tipo == VAL_CADENA) {
        const char *s = it->como.cadena.texto;
        int sl = it->como.cadena.longitud;
        int p = 0;
        while (p < sl) {
            utf8proc_int32_t cp;
            utf8proc_ssize_t cons = utf8proc_iterate(
                (const utf8proc_uint8_t *)(s + p), sl - p, &cp);
            if (cons <= 0) { p++; continue; }
            Valor cv = valor_cadena_duplicar(s + p, (int)cons);
            if (!conj_agregar(dst, cv)) {
                return error_nativa(err, linea, columna,
                    "memoria insuficiente al actualizar conjunto");
            }
            p += (int)cons;
        }
    } else {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: actualizar() requiere un iterable (conjunto/lista/tupla/cadena), no '%s'",
            valor_nombre_tipo(it));
    }
    return valor_nulo();
}

/* v1.156: conjunto.descartar(elem) — quita `elem` si esta presente,
 * sin lanzar si no lo esta. Paridad con Python set.discard().
 * `quitar(elem)` ya lanza ErrorDeClave si no existe; esta es la
 * variante "silenciosa". */
static Valor nativa_conjunto_descartar(EvalError *err, int n_args, Valor *args,
                                            int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: descartar(elem) requiere 1 argumento");
    }
    if (args[0].tipo != VAL_CONJUNTO) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: descartar() requiere un conjunto, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    if (args[0].como.conjunto->congelado) {
        return error_conjunto_congelado(err, linea, columna, "descartar");
    }
    if (!valor_es_hashable(&args[1])) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: '%s' no es hashable",
            valor_nombre_tipo(&args[1]));
    }
    /* conj_quitar retorna false si no estaba presente. Ignoramos. */
    (void)conj_quitar(args[0].como.conjunto, &args[1]);
    return valor_nulo();
}

/* tupla_contar(t, x) -> entero con apariciones por igualdad. */
static Valor nativa_tupla_contar(EvalError *err, int n_args, Valor *args,
                                     int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tupla_contar(t, x) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_TUPLA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tupla_contar() requiere una tupla");
    }
    Tupla *t = args[0].como.tupla;
    long cnt = 0;
    for (int i = 0; i < t->cuenta; i++) {
        if (valor_iguales(&t->elementos[i], &args[1])) cnt++;
    }
    return valor_entero_de_i64(cnt);
}

/* tupla_contiene(t, x) -> booleano. Igual semantica que `x en t`. */
static Valor nativa_tupla_contiene(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tupla_contiene(t, x) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_TUPLA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tupla_contiene() requiere una tupla");
    }
    Tupla *t = args[0].como.tupla;
    for (int i = 0; i < t->cuenta; i++) {
        if (valor_iguales(&t->elementos[i], &args[1])) {
            return valor_booleano(true);
        }
    }
    return valor_booleano(false);
}

/* tupla_indice_de(t, x) -> entero con el indice de la primera aparicion,
 * o -1 si no esta. */
static Valor nativa_tupla_indice_de(EvalError *err, int n_args, Valor *args,
                                        int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tupla_indice_de(t, x) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_TUPLA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: tupla_indice_de() requiere una tupla");
    }
    Tupla *t = args[0].como.tupla;
    for (int i = 0; i < t->cuenta; i++) {
        if (valor_iguales(&t->elementos[i], &args[1])) {
            return valor_entero_de_i64(i);
        }
    }
    return valor_entero_de_i64(-1);
}

/* lista_indice_de(xs, x) -> entero con el indice de la primera aparicion,
 * o -1 si no esta. v1.128: completa lista.contar/contiene de v1.122 con
 * indice_de para paridad con cadena.indice_de y tupla.indice_de. */
static Valor nativa_lista_indice_de(EvalError *err, int n_args, Valor *args,
                                        int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: lista_indice_de(xs, x) requiere 2 argumentos");
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: lista_indice_de() requiere una lista");
    }
    Lista *l = args[0].como.lista;
    for (int i = 0; i < l->cuenta; i++) {
        if (valor_iguales(&l->elementos[i], &args[1])) {
            return valor_entero_de_i64(i);
        }
    }
    return valor_entero_de_i64(-1);
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
 * v1.103: nativas matematicas (raiz, log, exp, trig, redondeo).
 *
 * Acepta entero/decimal/booleano como entrada; devuelve decimal.
 * Errores tipicos lanzan ErrorDeValor atrapable (raiz/log de
 * argumentos invalidos). Sin manejo especial de NaN/inf —
 * `tangente(PI/2)` o `ln(-0.5)` devuelven NaN/inf segun la libm.
 * ────────────────────────────────────────────────────────────────── */
#include <math.h>

static bool _val_a_double(const Valor *v, double *out) {
    if (v->tipo == VAL_DECIMAL) { *out = v->como.decimal; return true; }
    if (v->tipo == VAL_ENTERO_SMALL) { *out = (double)v->como.entero_small; return true; }
    if (v->tipo == VAL_ENTERO) { *out = mp_get_double(v->como.entero); return true; }
    if (v->tipo == VAL_BOOLEANO) { *out = v->como.booleano ? 1.0 : 0.0; return true; }
    return false;
}

#define MAT_UNARIA(nombre_c, nombre_dom, fn_c)                                 \
    static Valor nativa_mat_##nombre_c(EvalError *err, int n_args, Valor *args,\
                                          int linea, int columna) {            \
        if (n_args != 1) {                                                     \
            return error_nativa(err, linea, columna,                           \
                "ErrorDeTipo: " nombre_dom "() requiere 1 argumento, recibio %d",\
                n_args);                                                       \
        }                                                                      \
        double x;                                                              \
        if (!_val_a_double(&args[0], &x)) {                                    \
            return error_nativa(err, linea, columna,                           \
                "ErrorDeTipo: " nombre_dom "() requiere un numero");           \
        }                                                                      \
        return valor_decimal(fn_c(x));                                         \
    }

/* sqrt: validacion explicita de negativo */
static Valor nativa_mat_raiz(EvalError *err, int n_args, Valor *args,
                               int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: raiz() requiere 1 argumento, recibio %d", n_args);
    }
    double x;
    if (!_val_a_double(&args[0], &x)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: raiz() requiere un numero");
    }
    if (x < 0.0) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: raiz() de numero negativo");
    }
    return valor_decimal(sqrt(x));
}

/* ln: validacion explicita de no-positivo */
static Valor nativa_mat_ln(EvalError *err, int n_args, Valor *args,
                             int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: ln() requiere 1 argumento, recibio %d", n_args);
    }
    double x;
    if (!_val_a_double(&args[0], &x)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: ln() requiere un numero");
    }
    if (x <= 0.0) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: ln() de numero no positivo");
    }
    return valor_decimal(log(x));
}

static Valor nativa_mat_log10(EvalError *err, int n_args, Valor *args,
                                int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: log10() requiere 1 argumento, recibio %d", n_args);
    }
    double x;
    if (!_val_a_double(&args[0], &x)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: log10() requiere un numero");
    }
    if (x <= 0.0) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: log10() de numero no positivo");
    }
    return valor_decimal(log10(x));
}

MAT_UNARIA(exp,         "exp",        exp)
MAT_UNARIA(seno,        "seno",       sin)
MAT_UNARIA(coseno,      "coseno",     cos)
MAT_UNARIA(tangente,    "tangente",   tan)

/* asin/acos: validacion de dominio [-1, 1] */
static Valor nativa_mat_arco_seno(EvalError *err, int n_args, Valor *args,
                                    int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: arco_seno() requiere 1 argumento");
    }
    double x;
    if (!_val_a_double(&args[0], &x)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: arco_seno() requiere un numero");
    }
    if (x < -1.0 || x > 1.0) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: arco_seno() requiere x en [-1, 1]");
    }
    return valor_decimal(asin(x));
}

static Valor nativa_mat_arco_coseno(EvalError *err, int n_args, Valor *args,
                                      int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: arco_coseno() requiere 1 argumento");
    }
    double x;
    if (!_val_a_double(&args[0], &x)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: arco_coseno() requiere un numero");
    }
    if (x < -1.0 || x > 1.0) {
        return error_nativa(err, linea, columna,
            "ErrorDeValor: arco_coseno() requiere x en [-1, 1]");
    }
    return valor_decimal(acos(x));
}

MAT_UNARIA(arco_tangente, "arco_tangente", atan)
MAT_UNARIA(techo,         "techo",         ceil)
MAT_UNARIA(suelo,         "suelo",         floor)

/* round(): emula HALF_AWAY_FROM_ZERO independientemente de la libm
 * (`round` C99 lo hace pero documentamos comportamiento). */
static Valor nativa_mat_redondear(EvalError *err, int n_args, Valor *args,
                                    int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: redondear() requiere 1 argumento");
    }
    double x;
    if (!_val_a_double(&args[0], &x)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: redondear() requiere un numero");
    }
    return valor_decimal(round(x));
}

static Valor nativa_mat_arco_tangente2(EvalError *err, int n_args, Valor *args,
                                         int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: arco_tangente2(y, x) requiere 2 argumentos");
    }
    double y, x;
    if (!_val_a_double(&args[0], &y) || !_val_a_double(&args[1], &x)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: arco_tangente2() requiere numeros");
    }
    return valor_decimal(atan2(y, x));
}

/* potencia(x, y) = x^y. Funcion auxiliar; tambien hay `**` operador. */
static Valor nativa_mat_potencia(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: potencia(x, y) requiere 2 argumentos");
    }
    double x, y;
    if (!_val_a_double(&args[0], &x) || !_val_a_double(&args[1], &y)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: potencia() requiere numeros");
    }
    return valor_decimal(pow(x, y));
}

/* v1.110: constantes especiales para representar infinito y NaN.
 * Util en codigo cientifico que necesita valores limite (limites de
 * funciones, integrales divergentes, sentinelas). En cornamusa el
 * operador `/` lanza ErrorAritmetico para division por cero, asi que
 * estas constantes son la unica forma de obtener inf/nan reales. */
static Valor nativa_mat_infinito(EvalError *err, int n_args, Valor *args,
                                   int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: infinito() no acepta argumentos");
    }
    return valor_decimal(INFINITY);
}

static Valor nativa_mat_no_numero(EvalError *err, int n_args, Valor *args,
                                    int linea, int columna) {
    (void)args;
    if (n_args != 0) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: no_numero() no acepta argumentos");
    }
    return valor_decimal(NAN);
}

/* v1.110: predicados para detectar inf y NaN. `x != x` es la forma
 * estandar de testear NaN sin requerir bit fiddling. */
static Valor nativa_mat_es_infinito(EvalError *err, int n_args, Valor *args,
                                      int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: es_infinito() requiere 1 argumento");
    }
    double x;
    if (!_val_a_double(&args[0], &x)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: es_infinito() requiere un numero");
    }
    return valor_booleano(isinf(x) != 0);
}

static Valor nativa_mat_es_no_numero(EvalError *err, int n_args, Valor *args,
                                       int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: es_no_numero() requiere 1 argumento");
    }
    double x;
    if (!_val_a_double(&args[0], &x)) {
        return error_nativa(err, linea, columna,
            "ErrorDeTipo: es_no_numero() requiere un numero");
    }
    return valor_booleano(isnan(x) != 0);
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
    {"escribir", 8, nativa_escribir},
    {"imprimir_error", 14, nativa_imprimir_error},
    {"longitud", 8, nativa_longitud},
    {"tipo",     4, nativa_tipo},
    {"rango",    5, nativa_rango},
    {"enumerar", 8, nativa_enumerar},                       /* v1.192 */
    /* Conversores (v1.1). */
    {"cadena",      6,  nativa_cadena},
    {"entero",      6,  nativa_entero},
    {"binario",     7,  nativa_binario},        /* v1.159 */
    {"hexadecimal", 11, nativa_hexadecimal},    /* v1.159 */
    {"octal",       5,  nativa_octal},          /* v1.159 */
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
    {"inverso",  7, nativa_inverso},      /* v1.160 */
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
    {"ErrorDeAtributo", 15, nativa_exc_ErrorDeAtributo},
    /* GC manual (v0.8.1). */
    {"recolectar",      10, nativa_recolectar},
    /* Sistema (v0.9.2). */
    {"obtener_argv",    12, nativa_obtener_argv},
    /* Entorno (v1.104). */
    {"obtener_variable_entorno",     24, nativa_obtener_variable_entorno},
    {"establecer_variable_entorno",  27, nativa_establecer_variable_entorno},
    {"variables_entorno",            17, nativa_variables_entorno},
    {"directorio_inicio",            17, nativa_directorio_inicio},
    /* Entorno (v1.108). */
    {"usuario_actual",               14, nativa_usuario_actual},
    {"hostname",                      8, nativa_hostname},
    {"directorio_temporal",          19, nativa_directorio_temporal},
    {"salir",            5, nativa_salir},
    /* I/O de archivos (v1.8). */
    {"archivo_leer",     12, nativa_archivo_leer},
    {"archivo_escribir", 16, nativa_archivo_escribir},
    {"archivo_existe",   14, nativa_archivo_existe},
    {"archivo_lineas",   14, nativa_archivo_lineas},
    {"archivo_agregar",  15, nativa_archivo_agregar},
    /* Filesystem (v1.97). */
    {"archivo_es_directorio", 21, nativa_archivo_es_directorio},
    {"directorio_listar",     17, nativa_directorio_listar},
    {"obtener_cwd",           11, nativa_obtener_cwd},
    {"directorio_crear",      16, nativa_directorio_crear},
    /* Filesystem (v1.99). */
    {"archivo_borrar",        14, nativa_archivo_borrar},
    {"directorio_borrar",     17, nativa_directorio_borrar},
    {"archivo_info",          12, nativa_archivo_info},
    /* Filesystem (v1.105). */
    {"archivo_copiar",        14, nativa_archivo_copiar},
    /* Filesystem (v1.111). */
    {"archivo_mover",         13, nativa_archivo_mover},
    {"archivo_set_mtime",     17, nativa_archivo_set_mtime},
    /* JSON (v1.9). */
    {"json_parsear",     12, nativa_json_parsear},
    {"json_serializar",  15, nativa_json_serializar},
    /* Numéricos y reflexión (v1.11). */
    {"absoluto",         8,  nativa_absoluto},
    {"redondear",        9,  nativa_redondear},
    {"divmod",           6,  nativa_divmod},               /* v1.158 */
    {"potencia_modular", 16, nativa_potencia_modular},     /* v1.158 */
    {"instancia_de",    12,  nativa_instancia_de},
    {"subclase_de",     11,  nativa_subclase_de},
    {"id",               2,  nativa_id},
    {"repr",             4,  nativa_repr},
    {"hash",             4,  nativa_hash},                  /* v1.163 */
    {"congelar",         8,  nativa_congelar},              /* v1.164 */
    {"copia",            5,  nativa_copia},                 /* v1.165 */
    {"copia_profunda",  14,  nativa_copia_profunda},        /* v1.165 */
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
    {"escritor",             8, nativa_escritor},   /* v1.109 */
    /* @estaticometodo (v1.84). */
    {"estaticometodo",      14, nativa_estaticometodo},
    /* @clasemetodo (v1.85). */
    {"clasemetodo",         11, nativa_clasemetodo},
    /* Azar (v1.26). */
    {"azar_decimal",        12, nativa_azar_decimal},
    {"azar_entero",         11, nativa_azar_entero},
    {"azar_semilla",        12, nativa_azar_semilla},
    /* Matematicas (v1.103). */
    {"mat_raiz",             8, nativa_mat_raiz},
    {"mat_ln",               6, nativa_mat_ln},
    {"mat_log10",            9, nativa_mat_log10},
    {"mat_exp",              7, nativa_mat_exp},
    {"mat_seno",             8, nativa_mat_seno},
    {"mat_coseno",          10, nativa_mat_coseno},
    {"mat_tangente",        12, nativa_mat_tangente},
    {"mat_arco_seno",       13, nativa_mat_arco_seno},
    {"mat_arco_coseno",     15, nativa_mat_arco_coseno},
    {"mat_arco_tangente",   17, nativa_mat_arco_tangente},
    {"mat_arco_tangente2",  18, nativa_mat_arco_tangente2},
    {"mat_techo",            9, nativa_mat_techo},
    {"mat_suelo",            9, nativa_mat_suelo},
    {"mat_redondear",       13, nativa_mat_redondear},
    {"mat_potencia",        12, nativa_mat_potencia},
    /* Matematicas: constantes especiales y predicados (v1.110). */
    {"mat_infinito",        12, nativa_mat_infinito},
    {"mat_no_numero",       13, nativa_mat_no_numero},
    {"mat_es_infinito",     15, nativa_mat_es_infinito},
    {"mat_es_no_numero",    16, nativa_mat_es_no_numero},
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

void nativos_iterar_nombres(void (*cb)(const char *, int, void *), void *ctx) {
    if (!cb) return;
    for (int i = 0; i < N_NATIVAS; i++) {
        cb(NATIVAS[i].nombre, NATIVAS[i].longitud, ctx);
    }
}

/* ──────────────────────────────────────────────────────────────────
 * v1.122: tabla de metodos sobre tipos nativos.
 *
 * Mapea `(TipoValor, nombre_metodo) -> FnNativa subyacente`. El
 * usuario invoca `lista.añadir(x)` o `"hola".minusculas()` y la VM
 * busca aqui — si encuentra match, crea un MetodoNativoLigado y al
 * llamar la fn nativa se invoca con (receptor, args...).
 *
 * Solo registramos metodos cuya nativa global YA existe. Los nombres
 * son los castellanos comunes (vs. los prefijados `cadena_*` que se
 * exportan globalmente). Esto valida ejemplos como 05_listas
 * (numeros.añadir(6) / numeros.insertar(0, 0)).
 * ────────────────────────────────────────────────────────────────── */
typedef struct {
    TipoValor tipo;
    const char *nombre;
    int longitud;
    FnNativa fn;
} MetodoNativoEntrada;

static const MetodoNativoEntrada METODOS_NATIVOS[] = {
    /* Listas */
    {VAL_LISTA,      "añadir",      7, nativa_agregar},   /* 7 bytes UTF-8 */
    {VAL_LISTA,      "agregar",     7, nativa_agregar},
    {VAL_LISTA,      "insertar",    8, nativa_insertar},
    {VAL_LISTA,      "quitar",      6, nativa_quitar},
    {VAL_LISTA,      "ordenar",     7, nativa_ordenar},
    {VAL_LISTA,      "invertir",    8, nativa_invertir},
    {VAL_LISTA,      "contar",      6, nativa_lista_contar},     /* v1.122 */
    {VAL_LISTA,      "contiene",    8, nativa_lista_contiene},   /* v1.122 */
    {VAL_LISTA,      "copiar",      6, nativa_lista_copiar},     /* v1.122 */
    {VAL_LISTA,      "vaciar",      6, nativa_lista_vaciar},     /* v1.155 */
    {VAL_LISTA,      "extender",    8, nativa_lista_extender},   /* v1.155 */
    /* Cadenas */
    {VAL_CADENA,     "minusculas",  10, nativa_cadena_minusculas},      /* v1.153 Unicode */
    {VAL_CADENA,     "mayusculas",  10, nativa_cadena_mayusculas},      /* v1.153 Unicode */
    {VAL_CADENA,     "titulo",       6, nativa_cadena_titulo},          /* v1.154 */
    {VAL_CADENA,     "sin_acentos", 11, nativa_cadena_sin_acentos},     /* v1.162 */
    {VAL_CADENA,     "dividir_palabras", 16, nativa_cadena_dividir_palabras}, /* v1.157 */
    {VAL_CADENA,     "rellenar_ceros",   14, nativa_cadena_rellenar_ceros},   /* v1.157 */
    {VAL_CADENA,     "es_alfa",      7, nativa_cadena_es_alfa},         /* v1.154 */
    {VAL_CADENA,     "es_digito",    9, nativa_cadena_es_digito},       /* v1.154 */
    {VAL_CADENA,     "es_alfanum",  10, nativa_cadena_es_alfanum},      /* v1.154 */
    {VAL_CADENA,     "es_espacios", 11, nativa_cadena_es_espacios},     /* v1.154 */
    {VAL_CADENA,     "empieza_con", 11, nativa_cadena_empieza_con},
    {VAL_CADENA,     "termina_con", 11, nativa_cadena_termina_con},
    {VAL_CADENA,     "indice_de",   9,  nativa_cadena_indice_de},
    {VAL_CADENA,     "separar",     7,  nativa_cadena_separar},   /* v1.122 */
    {VAL_CADENA,     "reemplazar",  10, nativa_cadena_reemplazar},/* v1.122 */
    {VAL_CADENA,     "recortar",    8,  nativa_cadena_recortar},  /* v1.122 */
    {VAL_CADENA,     "contiene",    8,  nativa_cadena_contiene},  /* v1.122 */
    {VAL_CADENA,     "unir",        4,  nativa_cadena_unir_metodo},/* v1.122 */
    {VAL_CADENA,     "dividir_lineas", 14, nativa_cadena_dividir_lineas}, /* v1.152 */
    {VAL_CADENA,     "centrar",     7,  nativa_cadena_centrar},          /* v1.152 */
    {VAL_CADENA,     "alinear_izquierda", 17, nativa_cadena_alinear_izquierda}, /* v1.152 */
    {VAL_CADENA,     "alinear_derecha",   15, nativa_cadena_alinear_derecha},   /* v1.152 */
    {VAL_LISTA,      "indice_de",   9, nativa_lista_indice_de},   /* v1.128 */
    /* Diccionarios */
    {VAL_DICCIONARIO, "claves",     6, nativa_claves},
    {VAL_DICCIONARIO, "valores",    7, nativa_valores},
    {VAL_DICCIONARIO, "items",      5, nativa_dict_items},        /* v1.122 */
    {VAL_DICCIONARIO, "obtener",    7, nativa_dict_obtener},      /* v1.122 */
    {VAL_DICCIONARIO, "actualizar", 10, nativa_dict_actualizar},  /* v1.150 */
    {VAL_DICCIONARIO, "sacar",       5, nativa_dict_sacar},        /* v1.151 */
    {VAL_DICCIONARIO, "vaciar",      6, nativa_dict_vaciar},       /* v1.151 */
    /* Conjuntos (v1.128) */
    {VAL_CONJUNTO, "agregar",        7, nativa_agregar},  /* nativa global ya acepta conj */
    {VAL_CONJUNTO, "añadir",         7, nativa_agregar},  /* 7 bytes UTF-8 */
    {VAL_CONJUNTO, "quitar",         6, nativa_quitar},
    {VAL_CONJUNTO, "union",          5, nativa_conjunto_union},
    {VAL_CONJUNTO, "interseccion",  12, nativa_conjunto_interseccion},
    {VAL_CONJUNTO, "diferencia",    10, nativa_conjunto_diferencia},
    {VAL_CONJUNTO, "es_subconjunto",14, nativa_conjunto_es_subconjunto},
    {VAL_CONJUNTO, "contiene",       8, nativa_conjunto_contiene},
    {VAL_CONJUNTO, "copiar",         6, nativa_conjunto_copiar},
    {VAL_CONJUNTO, "vaciar",         6, nativa_conjunto_vaciar},      /* v1.156 */
    {VAL_CONJUNTO, "actualizar",    10, nativa_conjunto_actualizar},  /* v1.156 */
    {VAL_CONJUNTO, "descartar",      9, nativa_conjunto_descartar},   /* v1.156 */
    /* Tuplas (v1.128) */
    {VAL_TUPLA,    "contar",         6, nativa_tupla_contar},
    {VAL_TUPLA,    "contiene",       8, nativa_tupla_contiene},
    {VAL_TUPLA,    "indice_de",      9, nativa_tupla_indice_de},
};
#define N_METODOS_NATIVOS \
    (int)(sizeof(METODOS_NATIVOS) / sizeof(METODOS_NATIVOS[0]))

/* Busca metodo nativo por (tipo del receptor, nombre). Devuelve NULL si
 * no hay match. `nombre_out` se rellena con el puntero a la cadena
 * estatica del nombre — util para mensajes de error y para que
 * MetodoNativoLigado mantenga una referencia estable. */
FnNativa nativos_buscar_metodo(TipoValor tipo, const char *nombre, int len,
                                  const char **nombre_out) {
    for (int i = 0; i < N_METODOS_NATIVOS; i++) {
        const MetodoNativoEntrada *e = &METODOS_NATIVOS[i];
        if (e->tipo == tipo
            && e->longitud == len
            && memcmp(e->nombre, nombre, (size_t)len) == 0) {
            if (nombre_out) *nombre_out = e->nombre;
            return e->fn;
        }
    }
    if (nombre_out) *nombre_out = NULL;
    return NULL;
}
