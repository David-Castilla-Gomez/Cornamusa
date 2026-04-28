#include "valor.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "tommath.h"

/* ──────────────────────────────────────────────────────────────────
 * Helpers internos
 * ────────────────────────────────────────────────────────────────── */

/*
 * Aloca un mp_int en heap. mp_int es la struct de libtommath; su
 * `mp_init` aloca también un array interno. Si falla, devolvemos NULL.
 */
static mp_int *nuevo_mp_int(void) {
    mp_int *m = (mp_int *)malloc(sizeof(mp_int));
    if (m == NULL) return NULL;
    if (mp_init(m) != MP_OKAY) {
        free(m);
        return NULL;
    }
    return m;
}

/*
 * Copia un literal numérico al heap eliminando guiones bajos (que
 * libtommath no acepta) y descartando prefijo de base si lo hay.
 * Devuelve la cadena resultante (alocada con malloc) o NULL si OOM.
 *
 * `base_out` recibe la base detectada (10, 16, 8, 2).
 */
static char *normalizar_literal_entero(const char *lexema, int longitud,
                                        int *base_out) {
    int base = 10;
    int inicio = 0;
    /* Detectar prefijos: 0x/0X, 0o/0O, 0b/0B. */
    if (longitud >= 2 && lexema[0] == '0') {
        char p = lexema[1];
        if (p == 'x' || p == 'X') { base = 16; inicio = 2; }
        else if (p == 'o' || p == 'O') { base = 8; inicio = 2; }
        else if (p == 'b' || p == 'B') { base = 2; inicio = 2; }
    }
    *base_out = base;

    char *limpio = (char *)malloc((size_t)(longitud - inicio) + 1);
    if (limpio == NULL) return NULL;
    int j = 0;
    for (int i = inicio; i < longitud; i++) {
        if (lexema[i] != '_') {
            limpio[j++] = lexema[i];
        }
    }
    limpio[j] = '\0';
    return limpio;
}

/* Para decimales: copia eliminando '_'. Sin prefijos. */
static char *normalizar_literal_decimal(const char *lexema, int longitud) {
    char *limpio = (char *)malloc((size_t)longitud + 1);
    if (limpio == NULL) return NULL;
    int j = 0;
    for (int i = 0; i < longitud; i++) {
        if (lexema[i] != '_') {
            limpio[j++] = lexema[i];
        }
    }
    limpio[j] = '\0';
    return limpio;
}

/* ──────────────────────────────────────────────────────────────────
 * Constructores
 * ────────────────────────────────────────────────────────────────── */

Valor valor_nulo(void) {
    Valor v;
    v.tipo = VAL_NULO;
    v.dueno_cadena = false;
    return v;
}

Valor valor_booleano(bool b) {
    Valor v;
    v.tipo = VAL_BOOLEANO;
    v.dueno_cadena = false;
    v.como.booleano = b;
    return v;
}

Valor valor_decimal(double d) {
    Valor v;
    v.tipo = VAL_DECIMAL;
    v.dueno_cadena = false;
    v.como.decimal = d;
    return v;
}

Valor valor_entero_de_lexema(const char *lexema, int longitud) {
    int base;
    char *limpio = normalizar_literal_entero(lexema, longitud, &base);
    if (limpio == NULL) return valor_nulo();

    mp_int *m = nuevo_mp_int();
    if (m == NULL) { free(limpio); return valor_nulo(); }

    if (mp_read_radix(m, limpio, base) != MP_OKAY) {
        mp_clear(m);
        free(m);
        free(limpio);
        return valor_nulo();
    }
    free(limpio);

    Valor v;
    v.tipo = VAL_ENTERO;
    v.dueno_cadena = false;
    v.como.entero = m;
    return v;
}

Valor valor_entero_de_long(long n) {
    mp_int *m = nuevo_mp_int();
    if (m == NULL) return valor_nulo();
    mp_set_l(m, n);
    Valor v;
    v.tipo = VAL_ENTERO;
    v.dueno_cadena = false;
    v.como.entero = m;
    return v;
}

Valor valor_decimal_de_lexema(const char *lexema, int longitud) {
    char *limpio = normalizar_literal_decimal(lexema, longitud);
    if (limpio == NULL) return valor_nulo();
    double d = strtod(limpio, NULL);
    free(limpio);
    return valor_decimal(d);
}

Valor valor_cadena_referencia(const char *texto, int longitud) {
    Valor v;
    v.tipo = VAL_CADENA;
    v.dueno_cadena = false;
    v.como.cadena.texto = texto;
    v.como.cadena.longitud = longitud;
    return v;
}

Valor valor_cadena_duplicar(const char *texto, int longitud) {
    char *copia = (char *)malloc((size_t)longitud + 1);
    if (copia == NULL) return valor_nulo();
    if (longitud > 0) memcpy(copia, texto, (size_t)longitud);
    copia[longitud] = '\0';
    Valor v;
    v.tipo = VAL_CADENA;
    v.dueno_cadena = true;
    v.como.cadena.texto = copia;
    v.como.cadena.longitud = longitud;
    return v;
}

/* ──────────────────────────────────────────────────────────────────
 * Destrucción y copia
 * ────────────────────────────────────────────────────────────────── */

void valor_destruir(Valor *v) {
    if (v == NULL) return;
    switch (v->tipo) {
        case VAL_ENTERO:
            if (v->como.entero) {
                mp_clear(v->como.entero);
                free(v->como.entero);
                v->como.entero = NULL;
            }
            break;
        case VAL_CADENA:
            if (v->dueno_cadena && v->como.cadena.texto) {
                /* cast para deshacer const — sabemos que la asignamos
                   nosotros con malloc cuando dueno_cadena==true. */
                free((char *)v->como.cadena.texto);
                v->como.cadena.texto = NULL;
                v->dueno_cadena = false;
            }
            break;
        default:
            break;
    }
    v->tipo = VAL_NULO;
}

Valor valor_clonar(const Valor *v) {
    if (v == NULL) return valor_nulo();
    switch (v->tipo) {
        case VAL_NULO:      return valor_nulo();
        case VAL_BOOLEANO:  return valor_booleano(v->como.booleano);
        case VAL_DECIMAL:   return valor_decimal(v->como.decimal);
        case VAL_ENTERO: {
            mp_int *m = nuevo_mp_int();
            if (m == NULL) return valor_nulo();
            if (mp_copy(v->como.entero, m) != MP_OKAY) {
                mp_clear(m);
                free(m);
                return valor_nulo();
            }
            Valor c;
            c.tipo = VAL_ENTERO;
            c.dueno_cadena = false;
            c.como.entero = m;
            return c;
        }
        case VAL_CADENA:
            /* Si la fuente original es referencia, mantenemos referencia.
               Si es dueño, duplicamos para que la copia tenga la suya. */
            if (v->dueno_cadena) {
                return valor_cadena_duplicar(v->como.cadena.texto,
                                              v->como.cadena.longitud);
            }
            return valor_cadena_referencia(v->como.cadena.texto,
                                            v->como.cadena.longitud);
        case VAL_FUNCION:
        case VAL_NATIVA: {
            /* Funciones se comparten por puntero; copiar el struct
               sin duplicar el callable. */
            Valor c = *v;
            return c;
        }
    }
    return valor_nulo();
}

/* ──────────────────────────────────────────────────────────────────
 * Inspección
 * ────────────────────────────────────────────────────────────────── */

void valor_imprimir(const Valor *v, FILE *out) {
    char buffer[1024];
    valor_a_cadena(v, buffer, sizeof(buffer));
    fputs(buffer, out);
}

int valor_a_cadena(const Valor *v, char *buffer, int capacidad) {
    if (v == NULL || capacidad <= 0) return 0;
    int n = 0;

    switch (v->tipo) {
        case VAL_NULO:
            n = snprintf(buffer, (size_t)capacidad, "nulo");
            break;
        case VAL_BOOLEANO:
            n = snprintf(buffer, (size_t)capacidad, "%s",
                v->como.booleano ? "verdadero" : "falso");
            break;
        case VAL_DECIMAL:
            /* %g usa formato corto eligiendo entre fijo y científica.
               Para integer-valued floats, añadimos .0 para distinguir
               de enteros (estilo Python). */
            n = snprintf(buffer, (size_t)capacidad, "%g", v->como.decimal);
            if (n > 0 && n < capacidad
                && strchr(buffer, '.') == NULL
                && strchr(buffer, 'e') == NULL
                && strchr(buffer, 'n') == NULL /* nan */
                && strchr(buffer, 'i') == NULL /* inf */) {
                /* Valor entero como float: añadir ".0". */
                if (n + 2 < capacidad) {
                    buffer[n++] = '.';
                    buffer[n++] = '0';
                    buffer[n] = '\0';
                }
            }
            break;
        case VAL_ENTERO: {
            /* mp_radix_size devuelve el espacio necesario incluyendo
               '\0'. Usamos un buffer temporal si el resultado no cabe. */
            int tam = 0;
            if (mp_radix_size(v->como.entero, 10, &tam) != MP_OKAY) {
                n = snprintf(buffer, (size_t)capacidad, "<error>");
                break;
            }
            if (tam <= capacidad) {
                size_t escritos;
                if (mp_to_radix(v->como.entero, buffer, (size_t)capacidad,
                                &escritos, 10) != MP_OKAY) {
                    n = snprintf(buffer, (size_t)capacidad, "<error>");
                } else {
                    /* mp_to_radix incluye el '\0' en `escritos`. La
                       convención de valor_a_cadena es bytes escritos
                       SIN contar el terminador. */
                    n = (int)escritos - 1;
                    if (n < 0) n = 0;
                }
            } else {
                /* No cabe — escribir tantos dígitos como podamos.
                   Casos así son raros en uso normal. */
                n = snprintf(buffer, (size_t)capacidad, "<entero grande>");
            }
            break;
        }
        case VAL_CADENA: {
            /* Imprimir sin comillas (representación tipo print, no repr). */
            int longitud = v->como.cadena.longitud;
            if (longitud >= capacidad) longitud = capacidad - 1;
            if (longitud > 0) memcpy(buffer, v->como.cadena.texto,
                                      (size_t)longitud);
            buffer[longitud] = '\0';
            n = longitud;
            break;
        }
        case VAL_FUNCION:
            n = snprintf(buffer, (size_t)capacidad, "<funcion>");
            break;
        case VAL_NATIVA:
            n = snprintf(buffer, (size_t)capacidad, "<funcion nativa>");
            break;
    }

    if (n < 0) n = 0;
    if (n >= capacidad) n = capacidad - 1;
    buffer[n] = '\0';
    return n;
}

const char *valor_nombre_tipo(const Valor *v) {
    if (v == NULL) return "nulo";
    switch (v->tipo) {
        case VAL_NULO:      return "nulo";
        case VAL_BOOLEANO:  return "booleano";
        case VAL_ENTERO:    return "entero";
        case VAL_DECIMAL:   return "decimal";
        case VAL_CADENA:    return "cadena";
        case VAL_FUNCION:   return "funcion";
        case VAL_NATIVA:    return "funcion";  /* mismas semánticas externas */
    }
    return "desconocido";
}

bool valor_es_verdadero(const Valor *v) {
    if (v == NULL) return false;
    switch (v->tipo) {
        case VAL_NULO:      return false;
        case VAL_BOOLEANO:  return v->como.booleano;
        case VAL_ENTERO:    return mp_iszero(v->como.entero) == MP_NO;
        case VAL_DECIMAL:   return v->como.decimal != 0.0;
        case VAL_CADENA:    return v->como.cadena.longitud > 0;
        case VAL_FUNCION:
        case VAL_NATIVA:    return true;
    }
    return false;
}

bool valor_iguales(const Valor *a, const Valor *b) {
    if (a == NULL || b == NULL) return a == b;

    /* Caso especial: entero == decimal compara matemáticamente. */
    if (a->tipo == VAL_ENTERO && b->tipo == VAL_DECIMAL) {
        /* Convertir entero a double y comparar. Pierde precisión para
           enteros grandes, pero es la semántica de Python. */
        return mp_get_double(a->como.entero) == b->como.decimal;
    }
    if (a->tipo == VAL_DECIMAL && b->tipo == VAL_ENTERO) {
        return a->como.decimal == mp_get_double(b->como.entero);
    }

    if (a->tipo != b->tipo) return false;

    switch (a->tipo) {
        case VAL_NULO:      return true;
        case VAL_BOOLEANO:  return a->como.booleano == b->como.booleano;
        case VAL_DECIMAL:   return a->como.decimal == b->como.decimal;
        case VAL_ENTERO:    return mp_cmp(a->como.entero, b->como.entero) == MP_EQ;
        case VAL_CADENA:
            if (a->como.cadena.longitud != b->como.cadena.longitud) return false;
            return memcmp(a->como.cadena.texto, b->como.cadena.texto,
                          (size_t)a->como.cadena.longitud) == 0;
        case VAL_FUNCION:   return a->como.funcion == b->como.funcion;
        case VAL_NATIVA:    return a->como.nativa == b->como.nativa;
    }
    return false;
}
