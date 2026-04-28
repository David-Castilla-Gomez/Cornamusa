#include "nativos.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "evaluador.h"
#include "tommath.h"
#include "utf8proc.h"
#include "valor.h"

/*
 * Helper: pone un error en el evaluador desde un built-in. Las
 * built-ins no tienen un Expr* sino sólo línea/columna del call-site.
 */
static Valor error_nativa(Evaluador *ev, int linea, int columna,
                          const char *fmt, ...) {
    if (ev->error.tuvo_error) return valor_nulo();
    ev->error.tuvo_error = true;
    ev->error.linea = linea;
    ev->error.columna = columna;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ev->error.mensaje, sizeof(ev->error.mensaje), fmt, ap);
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

static Valor nativa_imprimir(Evaluador *ev, int n_args, Valor *args,
                              int linea, int columna) {
    (void)ev; (void)linea; (void)columna;
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

static Valor nativa_longitud(Evaluador *ev, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: longitud() requiere 1 argumento, recibio %d", n_args);
    }
    const Valor *v = &args[0];
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
                return error_nativa(ev, linea, columna,
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
        Valor out;
        out.tipo = VAL_ENTERO;
        out.dueno_cadena = false;
        out.como.entero = resultado;
        return out;
    }
    return error_nativa(ev, linea, columna,
        "ErrorDeTipo: longitud() no soporta '%s'", valor_nombre_tipo(v));
}

/* ──────────────────────────────────────────────────────────────────
 * tipo(x)
 *
 * Devuelve cadena con el nombre del tipo del valor, en castellano.
 * ────────────────────────────────────────────────────────────────── */

static Valor nativa_tipo(Evaluador *ev, int n_args, Valor *args,
                         int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(ev, linea, columna,
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
    if (v->tipo == VAL_BOOLEANO) {
        mp_set_l(out, v->como.booleano ? 1 : 0);
        return true;
    }
    return false;
}

static Valor nativa_rango(Evaluador *ev, int n_args, Valor *args,
                          int linea, int columna) {
    if (n_args < 1 || n_args > 3) {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: rango() acepta 1, 2 o 3 argumentos, recibio %d",
            n_args);
    }
    for (int i = 0; i < n_args; i++) {
        if (args[i].tipo != VAL_ENTERO && args[i].tipo != VAL_BOOLEANO) {
            return error_nativa(ev, linea, columna,
                "ErrorDeTipo: rango() solo acepta enteros, recibio '%s'",
                valor_nombre_tipo(&args[i]));
        }
    }

    mp_int *mi = (mp_int *)malloc(sizeof(mp_int));
    mp_int *mf = (mp_int *)malloc(sizeof(mp_int));
    mp_int *mp = (mp_int *)malloc(sizeof(mp_int));
    if (!mi || !mf || !mp) {
        free(mi); free(mf); free(mp);
        return error_nativa(ev, linea, columna, "memoria insuficiente");
    }
    if (mp_init_multi(mi, mf, mp, NULL) != MP_OKAY) {
        free(mi); free(mf); free(mp);
        return error_nativa(ev, linea, columna, "memoria insuficiente");
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
                return error_nativa(ev, linea, columna,
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
    return error_nativa(ev, linea, columna, "memoria insuficiente");
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
    else if (v->tipo == VAL_ENTERO) {
        if (mp_count_bits(v->como.entero) > 62) return false;
        i = (long)mp_get_i64(v->como.entero);
    } else {
        return false;
    }
    if (i < 0) i += total;
    if (i < 0 || i >= total) return false;
    *out = i;
    return true;
}

/*
 * agregar(lista, x) — añade `x` al final. Devuelve nulo (Python
 * `list.append`).
 */
static Valor nativa_agregar(Evaluador *ev, int n_args, Valor *args,
                             int linea, int columna) {
    if (n_args != 2) {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: agregar() requiere 2 argumentos (lista, valor), recibio %d",
            n_args);
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: agregar() requiere una lista como primer argumento, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    /* Tomamos un clon del valor: el llamador conserva ownership de args. */
    Valor copia = valor_clonar(&args[1]);
    if (!lista_agregar(args[0].como.lista, copia)) {
        return error_nativa(ev, linea, columna, "memoria insuficiente");
    }
    return valor_nulo();
}

/*
 * quitar(lista, indice=-1) — elimina y devuelve el elemento en `indice`.
 * Sin argumento de índice usa -1 (último). Indice negativo cuenta
 * desde el final.
 */
static Valor nativa_quitar(Evaluador *ev, int n_args, Valor *args,
                            int linea, int columna) {
    if (n_args < 1 || n_args > 2) {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: quitar() requiere 1 o 2 argumentos, recibio %d", n_args);
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: quitar() requiere una lista, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Lista *l = args[0].como.lista;
    if (l->cuenta == 0) {
        return error_nativa(ev, linea, columna,
            "ErrorDeIndice: quitar() de una lista vacia");
    }
    long i;
    if (n_args == 2) {
        if (!indice_a_long_natural(&args[1], &i, l->cuenta)) {
            return error_nativa(ev, linea, columna,
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
static Valor nativa_insertar(Evaluador *ev, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args != 3) {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: insertar() requiere 3 argumentos, recibio %d", n_args);
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: insertar() requiere una lista, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Lista *l = args[0].como.lista;
    /* Para insertar permitimos índices en [-cuenta, cuenta] (clamping
       a [0, cuenta] tras normalizar). */
    long i;
    if (args[1].tipo == VAL_BOOLEANO) i = args[1].como.booleano ? 1 : 0;
    else if (args[1].tipo == VAL_ENTERO
             && mp_count_bits(args[1].como.entero) <= 62) {
        i = (long)mp_get_i64(args[1].como.entero);
    } else {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: indice de insertar() debe ser entero");
    }
    if (i < 0) i += l->cuenta;
    if (i < 0) i = 0;
    if (i > l->cuenta) i = l->cuenta;

    /* Reservar espacio (lista_agregar para forzar crecimiento, luego
       desplazar). */
    if (!lista_agregar(l, valor_nulo())) {
        return error_nativa(ev, linea, columna, "memoria insuficiente");
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
static Valor nativa_invertir(Evaluador *ev, int n_args, Valor *args,
                              int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: invertir() requiere 1 argumento, recibio %d", n_args);
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(ev, linea, columna,
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
    bool an = a->tipo == VAL_ENTERO || a->tipo == VAL_DECIMAL || a->tipo == VAL_BOOLEANO;
    bool bn = b->tipo == VAL_ENTERO || b->tipo == VAL_DECIMAL || b->tipo == VAL_BOOLEANO;
    if (an && bn) {
        bool a_ent = a->tipo == VAL_ENTERO || a->tipo == VAL_BOOLEANO;
        bool b_ent = b->tipo == VAL_ENTERO || b->tipo == VAL_BOOLEANO;
        if (a_ent && b_ent) {
            mp_int ma, mb;
            if (mp_init_multi(&ma, &mb, NULL) != MP_OKAY) return 0;
            mp_err r1 = MP_OKAY, r2 = MP_OKAY;
            if (a->tipo == VAL_BOOLEANO) mp_set_l(&ma, a->como.booleano ? 1 : 0);
            else r1 = mp_copy(a->como.entero, &ma);
            if (b->tipo == VAL_BOOLEANO) mp_set_l(&mb, b->como.booleano ? 1 : 0);
            else r2 = mp_copy(b->como.entero, &mb);
            (void)r1; (void)r2;
            int c = mp_cmp(&ma, &mb);
            mp_clear_multi(&ma, &mb, NULL);
            if (c == MP_LT) return -1;
            if (c == MP_GT) return 1;
            return 0;
        }
        double da = a->tipo == VAL_DECIMAL ? a->como.decimal
                  : a->tipo == VAL_BOOLEANO ? (a->como.booleano ? 1.0 : 0.0)
                  : mp_get_double(a->como.entero);
        double db = b->tipo == VAL_DECIMAL ? b->como.decimal
                  : b->tipo == VAL_BOOLEANO ? (b->como.booleano ? 1.0 : 0.0)
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

static Valor nativa_ordenar(Evaluador *ev, int n_args, Valor *args,
                             int linea, int columna) {
    if (n_args != 1) {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: ordenar() requiere 1 argumento, recibio %d", n_args);
    }
    if (args[0].tipo != VAL_LISTA) {
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: ordenar() requiere una lista, no '%s'",
            valor_nombre_tipo(&args[0]));
    }
    Lista *l = args[0].como.lista;
    g_ordenar_error = false;
    qsort(l->elementos, (size_t)l->cuenta, sizeof(Valor), comparador_ordenar);
    if (g_ordenar_error) {
        g_ordenar_error = false;
        return error_nativa(ev, linea, columna,
            "ErrorDeTipo: ordenar() no puede comparar tipos mixtos no numericos");
    }
    return valor_nulo();
}

/* ──────────────────────────────────────────────────────────────────
 * Registro
 * ────────────────────────────────────────────────────────────────── */

void nativos_registrar(Entorno *globales) {
    /* Los nombres apuntan a literales de cadena estáticos: viven todo
       el programa, no se liberan. Las claves del entorno guardan estos
       punteros directamente. */
    static const char NOMBRE_IMPRIMIR[] = "imprimir";
    static const char NOMBRE_LONGITUD[] = "longitud";
    static const char NOMBRE_TIPO[]     = "tipo";
    static const char NOMBRE_RANGO[]    = "rango";
    static const char NOMBRE_AGREGAR[]  = "agregar";
    static const char NOMBRE_QUITAR[]   = "quitar";
    static const char NOMBRE_INSERTAR[] = "insertar";
    static const char NOMBRE_INVERTIR[] = "invertir";
    static const char NOMBRE_ORDENAR[]  = "ordenar";

    entorno_definir(globales, NOMBRE_IMPRIMIR, 8,
        valor_nativa(NOMBRE_IMPRIMIR, nativa_imprimir));
    entorno_definir(globales, NOMBRE_LONGITUD, 8,
        valor_nativa(NOMBRE_LONGITUD, nativa_longitud));
    entorno_definir(globales, NOMBRE_TIPO, 4,
        valor_nativa(NOMBRE_TIPO, nativa_tipo));
    entorno_definir(globales, NOMBRE_RANGO, 5,
        valor_nativa(NOMBRE_RANGO, nativa_rango));

    entorno_definir(globales, NOMBRE_AGREGAR, 7,
        valor_nativa(NOMBRE_AGREGAR, nativa_agregar));
    entorno_definir(globales, NOMBRE_QUITAR, 6,
        valor_nativa(NOMBRE_QUITAR, nativa_quitar));
    entorno_definir(globales, NOMBRE_INSERTAR, 8,
        valor_nativa(NOMBRE_INSERTAR, nativa_insertar));
    entorno_definir(globales, NOMBRE_INVERTIR, 8,
        valor_nativa(NOMBRE_INVERTIR, nativa_invertir));
    entorno_definir(globales, NOMBRE_ORDENAR, 7,
        valor_nativa(NOMBRE_ORDENAR, nativa_ordenar));
}
